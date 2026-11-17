#include "ChartFrame.h"

#include <QDataStream>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "Gui/Style/Palette.h"
#include "Widgets/Chart/ChartView.h"

namespace Labeler {

namespace {

// Тот же приём, что в DataViewer (hideEmptySeries()): у .data фиксированный
// набор серий на тип теста, а у конкретного клапана часть каналов давления
// не подключена — пустой чип в легенде только мешает.
void hideEmptySeries(Widgets::Chart::ChartView* chartView)
{
    const auto& seriesList = chartView->series();
    for (int i = 0; i < seriesList.size(); ++i) {
        if (seriesList.at(i)->points().isEmpty())
            chartView->setSeriesAvailable(static_cast<quint8>(i), false);
    }
}

QToolButton* makeToolButton(const QString& text, const QString& tip, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setAutoRaise(true);
    b->setCursor(Qt::PointingHandCursor);
    // Фокус клавиатуры остаётся у окна: стрелки и цифры — горячие клавиши
    // разметки, а не навигация по кнопкам.
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}

}

ChartFrame::ChartFrame(QWidget* parent)
    : QFrame(parent)
{
    // QFrame[role="section"] в theme.qss — та же рамка-карточка, что у
    // секций основного окна.
    setProperty("role", QStringLiteral("section"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 6, 8, 8);
    root->setSpacing(4);

    auto* header = new QHBoxLayout;
    header->setSpacing(4);

    m_title = new QLabel(this);
    m_title->setProperty("role", QStringLiteral("title"));
    header->addWidget(m_title);

    m_fileLabel = new QLabel(this);
    m_fileLabel->setEnabled(false);  // приглушённый цвет — вспомогательная подпись
    header->addWidget(m_fileLabel);
    header->addStretch(1);

    m_prev = makeToolButton(QStringLiteral("◂"), tr("Предыдущий прогон"), this);
    m_counter = new QLabel(this);
    m_next = makeToolButton(QStringLiteral("▸"), tr("Следующий прогон"), this);
    m_expand = makeToolButton(QStringLiteral("⤢"), tr("На всю карточку (Esc — обратно)"), this);
    header->addWidget(m_prev);
    header->addWidget(m_counter);
    header->addWidget(m_next);
    header->addWidget(m_expand);
    root->addLayout(header);

    m_results = new QLabel(this);
    m_results->setTextFormat(Qt::RichText);
    m_results->setWordWrap(true);
    m_results->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_results->hide();
    root->addWidget(m_results);

    connect(m_prev, &QToolButton::clicked, this, [this] { showFile(m_index - 1); });
    connect(m_next, &QToolButton::clicked, this, [this] { showFile(m_index + 1); });
    connect(m_expand, &QToolButton::clicked, this, &ChartFrame::expandToggled);

    m_stack = new QStackedWidget(this);
    m_placeholder = new QLabel(m_stack);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setEnabled(false);
    m_stack->addWidget(m_placeholder);

    m_chartHost = new QWidget(m_stack);
    auto* hostLayout = new QVBoxLayout(m_chartHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    m_stack->addWidget(m_chartHost);
    root->addWidget(m_stack, 1);

    showPlaceholder(tr("нет в сессии"));
}

void ChartFrame::setTitle(const QString& title)
{
    m_title->setText(title);
}

void ChartFrame::setChartObjectName(const QString& name)
{
    m_chartObjectName = name;
}

void ChartFrame::setResults(const QString& html)
{
    m_results->setText(html);
    m_results->setVisible(!html.isEmpty());
}

void ChartFrame::setFiles(const QStringList& paths)
{
    setResults({});
    m_paths = paths;
    m_index = -1;
    if (m_paths.isEmpty()) {
        showPlaceholder(tr("нет в сессии"));
        updateSwitcher();
        return;
    }
    // Последний прогон — обычно тот, после которого оператор закончил тест.
    showFile(int(m_paths.size()) - 1);
}

void ChartFrame::applyPalette(const Gui::Style::Palette& palette)
{
    m_palette = &palette;
    if (m_chart)
        m_chart->applyPalette(palette);
}

void ChartFrame::setExpanded(bool expanded)
{
    m_expand->setText(expanded ? QStringLiteral("⤡") : QStringLiteral("⤢"));
}

QString ChartFrame::currentFileName() const
{
    if (m_index < 0 || m_index >= m_paths.size())
        return {};
    return QFileInfo(m_paths.at(m_index)).fileName();
}

void ChartFrame::showPlaceholder(const QString& text)
{
    if (m_chart) {
        delete m_chart;
        m_chart = nullptr;
    }
    m_placeholder->setText(text);
    m_fileLabel->clear();
    m_stack->setCurrentWidget(m_placeholder);
}

void ChartFrame::showFile(int index)
{
    if (index < 0 || index >= m_paths.size())
        return;
    m_index = index;
    updateSwitcher();

    const QString path = m_paths.at(index);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        showPlaceholder(tr("не открывается: %1").arg(file.errorString()));
        return;
    }

    QDataStream stream(&file);
    // Та же версия, что пишет Report::Saver::saveChartSnapshot(); старые
    // файлы MpiSki в том же формате.
    stream.setVersion(QDataStream::Qt_6_2);

    // Новый ChartView на каждый файл — тот же проверенный путь, что в
    // DataViewer::openFile(): loadFromStream() рассчитан на пустой график.
    auto* chart = new Widgets::Chart::ChartView(m_chartHost);
    // До первого показа: стиль по #objectName применяется при полировке.
    chart->setObjectName(m_chartObjectName);
    if (m_palette)
        chart->applyPalette(*m_palette);
    chart->loadFromStream(stream);

    if (stream.status() != QDataStream::Ok) {
        delete chart;
        showPlaceholder(tr("файл повреждён или это не график"));
        m_fileLabel->setText(QFileInfo(path).fileName());
        return;
    }

    hideEmptySeries(chart);

    if (m_chart)
        delete m_chart;
    m_chart = chart;
    m_chartHost->layout()->addWidget(m_chart);
    m_chart->viewport()->installEventFilter(this);
    m_fileLabel->setText(QFileInfo(path).fileName());
    m_stack->setCurrentWidget(m_chartHost);
}

void ChartFrame::updateSwitcher()
{
    const bool several = m_paths.size() > 1;
    m_prev->setVisible(several);
    m_next->setVisible(several);
    m_counter->setVisible(several);
    if (several) {
        m_counter->setText(QStringLiteral("%1/%2").arg(m_index + 1).arg(m_paths.size()));
        m_prev->setEnabled(m_index > 0);
        m_next->setEnabled(m_index < m_paths.size() - 1);
    }
}

bool ChartFrame::eventFilter(QObject* watched, QEvent* event)
{
    if (m_chart && watched == m_chart->viewport() && event->type() == QEvent::MouseButtonDblClick) {
        emit expandToggled();
        return true;
    }
    return QFrame::eventFilter(watched, event);
}

}
