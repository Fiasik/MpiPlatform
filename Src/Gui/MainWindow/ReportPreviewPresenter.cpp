#include "ReportPreviewPresenter.h"
#include "ui_ReportPreviewPanel.h"

#include <QEvent>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QPrintPreviewWidget>
#include <QPrinter>
#include <QSignalBlocker>
#include <QWheelEvent>

#include <cmath>

#include "Gui/MainWindow/ReportGenerationPresenter.h"
#include "Gui/Style/RoleStyler.h"
#include "Report/Pdf/Builder.h"

using Report::Pdf::Act;

namespace {
// Масштаб 1.0 — страница в натуральную величину; дальше этих пределов
// Ctrl + колесо и −/+ не уводят.
constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 5.0;
}

ReportPreviewPresenter::ReportPreviewPresenter(ReportGenerationPresenter* report, QObject* parent)
    : QObject(parent)
    , m_report(report)
    , m_ui(std::make_unique<Ui::ReportPreviewPanel>())
    , m_printer(std::make_unique<QPrinter>(QPrinter::HighResolution))
{
    m_page = new QWidget();
    m_ui->setupUi(m_page);
    Gui::Style::applyRoles(m_page);

    // PdfFormat — чтобы размеры страницы и шрифтов брались у того же
    // PDF-движка, что пишет файл, а не у системного принтера (его может и
    // не быть).
    m_printer->setOutputFormat(QPrinter::PdfFormat);

    // В .ui его не положить: uic зовёт конструктор без QPrinter, и виджет
    // заводит себе принтер с экранным разрешением.
    m_preview = new QPrintPreviewWidget(m_printer.get(), m_ui->page_reportPreview_view);
    // Страница целиком: «по ширине» в широком окне центрирует страницу, и
    // шапка акта уходит за верхний край (проверено на реальном испытании).
    m_preview->setZoomMode(QPrintPreviewWidget::FitInView);
    m_ui->verticalLayout_reportPreviewView->addWidget(m_preview);

    connect(m_preview, &QPrintPreviewWidget::paintRequested, this, &ReportPreviewPresenter::paint);
    connect(m_preview, &QPrintPreviewWidget::previewChanged, this, &ReportPreviewPresenter::updatePageControls);

    connect(m_ui->pushButton_reportPreview_refresh, &QPushButton::clicked,
            this, &ReportPreviewPresenter::refresh);
    connect(m_ui->comboBox_reportPreview_act, &QComboBox::currentIndexChanged, this, [this] {
        render();
        showPage(1);
    });

    connect(m_ui->pushButton_reportPreview_prevPage, &QPushButton::clicked, this, [this] {
        showPage(m_preview->currentPage() - 1);
    });
    connect(m_ui->pushButton_reportPreview_nextPage, &QPushButton::clicked, this, [this] {
        showPage(m_preview->currentPage() + 1);
    });
    connect(m_ui->pushButton_reportPreview_zoomIn, &QPushButton::clicked,
            this, [this] { zoomBy(1.2); });
    connect(m_ui->pushButton_reportPreview_zoomOut, &QPushButton::clicked,
            this, [this] { zoomBy(1.0 / 1.2); });
    connect(m_ui->pushButton_reportPreview_fitWidth, &QPushButton::clicked,
            m_preview, &QPrintPreviewWidget::fitToWidth);
    connect(m_ui->pushButton_reportPreview_fitPage, &QPushButton::clicked,
            m_preview, &QPrintPreviewWidget::fitInView);

    // Открыли вкладку — снимаем данные заново; Ctrl + колесо над страницей —
    // масштаб (см. eventFilter()).
    m_page->installEventFilter(this);
    if (auto* view = m_preview->findChild<QGraphicsView*>()) {
        m_previewViewport = view->viewport();
        m_previewViewport->installEventFilter(this);
    }

    m_ui->stackedWidget_reportPreview->setCurrentWidget(m_ui->page_reportPreview_empty);
    updatePageControls();
}

ReportPreviewPresenter::~ReportPreviewPresenter()
{
    // Страницей владеет QTabWidget, но QPrintPreviewWidget держит указатель
    // на m_printer — убираем страницу раньше принтера.
    if (m_previewViewport)
        m_previewViewport->removeEventFilter(this);
    if (m_page) {
        m_page->removeEventFilter(this);
        delete m_page;
    }
}

bool ReportPreviewPresenter::eventFilter(QObject* watched, QEvent* event)
{
    // Несамопроизвольный Show — это переключение на вкладку; самопроизвольный
    // (окно развернули из панели задач) данные не меняет.
    if (watched == m_page && event->type() == QEvent::Show && !event->spontaneous())
        refresh();

    // Без Ctrl колесо, как и было, листает страницы.
    if (watched == m_previewViewport && event->type() == QEvent::Wheel) {
        auto* wheel = static_cast<QWheelEvent*>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            // 120 — один щелчок колеса; тачпад шлёт доли — масштаб плавный.
            const int delta = wheel->angleDelta().y();
            if (delta != 0)
                zoomBy(std::pow(1.2, delta / 120.0));
            return true;
        }
    }

    return QObject::eventFilter(watched, event);
}

void ReportPreviewPresenter::refresh()
{
    m_snapshot = m_report->collectInput();
    fillActs();
    render();
}

void ReportPreviewPresenter::refreshIfVisible()
{
    if (m_page && m_page->isVisible())
        refresh();
}

std::optional<Act> ReportPreviewPresenter::currentAct() const
{
    const QVariant data = m_ui->comboBox_reportPreview_act->currentData();
    if (!data.isValid())
        return std::nullopt;
    return Act(data.toInt());
}

void ReportPreviewPresenter::fillActs()
{
    const std::optional<Act> previous = currentAct();

    QList<Act> acts;
    if (m_snapshot) {
        acts = m_snapshot->pdfActs();

        // Комментарии — отдельный PDF: его пишет ReportGenerationPresenter
        // при правке комментария, если есть хоть один непустой.
        for (const QString& comment : std::as_const(m_snapshot->testComments)) {
            if (!comment.trimmed().isEmpty()) {
                acts.append(Act::Comments);
                break;
            }
        }
    }

    QComboBox* combo = m_ui->comboBox_reportPreview_act;
    const QSignalBlocker blocker(combo);
    combo->clear();
    for (const Act act : acts)
        combo->addItem(Report::Pdf::actTitle(act), int(act));

    if (previous) {
        const int index = combo->findData(int(*previous));
        if (index >= 0)
            combo->setCurrentIndex(index);
    }
    combo->setEnabled(!acts.isEmpty());
}

void ReportPreviewPresenter::render()
{
    if (!currentAct()) {
        QString text = tr("PDF-актов пока нет: акт печатается только по тесту, пройденному в этой сессии.");
        if (m_snapshot && m_snapshot->telemetry.testMain && !m_snapshot->buildTechnicalInspectionPdf)
            text += QLatin1Char('\n') + tr("Акт ТО выключен галочкой «Создать PDF отчет ТО».");
        m_ui->label_reportPreview_empty->setText(text);
        m_ui->stackedWidget_reportPreview->setCurrentWidget(m_ui->page_reportPreview_empty);
        updatePageControls();
        return;
    }

    // Сначала рисуем, потом показываем: при первом показе виджет сам
    // запросил бы перерисовку, и акт собрался бы дважды.
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    m_preview->updatePreview();
    QGuiApplication::restoreOverrideCursor();

    m_ui->stackedWidget_reportPreview->setCurrentWidget(m_ui->page_reportPreview_view);
    updatePageControls();
}

void ReportPreviewPresenter::paint(QPrinter* printer)
{
    const std::optional<Act> act = currentAct();
    if (!m_snapshot || !act)
        return;

    // Ошибку шаблона Builder уже пишет в лог; в предпросмотре будет пусто.
    Report::Pdf::Builder().preview(*act, m_snapshot->context(), printer);
}

void ReportPreviewPresenter::showPage(int page)
{
    // QPrintPreviewWidget::setCurrentPage() листает, но previewChanged() не
    // шлёт (проверено на Qt 6.8) — счётчик и кнопки обновляем сами.
    m_preview->setCurrentPage(page);
    updatePageControls();
}

void ReportPreviewPresenter::zoomBy(qreal factor)
{
    const qreal target = m_preview->zoomFactor() * factor;
    if ((factor > 1.0 && target > kMaxZoom) || (factor < 1.0 && target < kMinZoom))
        return;
    m_preview->setZoomFactor(target);
}

void ReportPreviewPresenter::updatePageControls()
{
    const bool shown = m_ui->stackedWidget_reportPreview->currentWidget() == m_ui->page_reportPreview_view;
    const int count = shown ? m_preview->pageCount() : 0;
    const int page = count > 0 ? m_preview->currentPage() : 0;

    m_ui->label_reportPreview_page->setText(tr("стр. %1 из %2").arg(page).arg(count));
    m_ui->pushButton_reportPreview_prevPage->setEnabled(page > 1);
    m_ui->pushButton_reportPreview_nextPage->setEnabled(page < count);
    m_ui->pushButton_reportPreview_zoomIn->setEnabled(count > 0);
    m_ui->pushButton_reportPreview_zoomOut->setEnabled(count > 0);
    m_ui->pushButton_reportPreview_fitWidth->setEnabled(count > 0);
    m_ui->pushButton_reportPreview_fitPage->setEnabled(count > 0);
}
