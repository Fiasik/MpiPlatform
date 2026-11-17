#pragma once

// Вкладка «Предпросмотр» в «Формировании отчёта»: PDF-акты из тех же
// данных, что возьмёт «Сформировать отчёт» (ReportGenerationPresenter::
// collectInput()), нарисованные тем же Report::Pdf::Builder, но в
// QPrintPreviewWidget — файлы не пишутся. Excel (report.xlsx) здесь не
// показывается: у него другой движок (QXlsx), нарисовать его нечем.
//
// Вид панели — в ReportPreviewPanel.ui. Данные снимаются заново при каждом
// открытии вкладки и по «Обновить»; между обновлениями предпросмотр
// показывает этот снимок (смена акта в списке его не пересобирает).
//
// Пока в главном окне открыт просмотр архива («История» → «Открыть
// графики»), вместо черновика текущей сессии — Акт ТО той, прошлой: собран
// заново из базы и её папки (ReportCompare::loadInput), сверху плашка об
// этом и кнопка открыть настоящий файл отчёта из папки, если он там есть.

#include <QObject>
#include <QPointer>

#include <memory>
#include <optional>

#include "ReportGenerationWorker.h"

class QPrinter;
class QPrintPreviewWidget;
class QWidget;
class ReportGenerationPresenter;
#ifdef MPIPLATFORM_EXPERIMENTAL_REPORT_COMPARE
class ReportComparePresenter;
#endif

namespace Ui { class ReportPreviewPanel; }

class ReportPreviewPresenter : public QObject
{
    Q_OBJECT

public:
    explicit ReportPreviewPresenter(ReportGenerationPresenter* report, QObject* parent = nullptr);
    ~ReportPreviewPresenter() override;

    // Страница вкладки; владельцем становится QTabWidget, куда её добавят.
    QWidget* panel() const { return m_page; }

#ifdef MPIPLATFORM_EXPERIMENTAL_REPORT_COMPARE
    // Сравнение отчётов (эксперимент): текущую сессию в «Сравнить с» не
    // предлагаем.
    void setCurrentSessionId(qint64 sessionId);
#endif

    // Акт прошлой сессии вместо черновика текущей. input — nullopt, если
    // собрать не удалось (причина — error, её покажет пустая страница);
    // folder — папка сессии, там ищется выданный отчёт.
    void showPastSession(const QString& title, const QString& folder,
                         std::optional<ReportGenerationWorker::Input> input, const QString& error);
    // Снова черновик текущей сессии.
    void clearPastSession();

signals:
    // «Закрыть просмотр» на плашке прошлой сессии — закрывать просмотр
    // архива целиком решает главное окно.
    void closePastSessionRequested();

public slots:
    // Заново снять данные отчёта и перерисовать выбранный акт.
    void refresh();
    // То же, но только если вкладка сейчас на экране (галочка «Создать PDF
    // отчет ТО» под вкладками меняет список актов).
    void refreshIfVisible();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    std::optional<Report::Pdf::Act> currentAct() const;
    void fillActs();
    void render();
    void paint(QPrinter* printer);
    void updatePageControls();
    void showPage(int page);
    void zoomBy(qreal factor);
    void fitToWidth();
    void fitInView();
    // Сейчас на экране сравнение двух сессий (эксперимент) — листание и
    // масштаб относятся к нему.
    bool comparing() const;
    int shownPage() const;
    void updatePastBanner();
    // Файл отчёта, записанный «Сформировать отчёт» в папку прошлой сессии:
    // Акт ТО (PDF), иначе report.xlsx; пусто — в папке его нет.
    QString issuedReportPath() const;

    struct PastSession {
        QString title;
        QString folder;
        QString error;
        std::optional<ReportGenerationWorker::Input> input;
    };

    ReportGenerationPresenter* m_report;
    QPointer<QWidget> m_page;
    std::unique_ptr<Ui::ReportPreviewPanel> m_ui;
    // Как у PDF-файла (HighResolution + PdfFormat), см. Builder::preview().
    std::unique_ptr<QPrinter> m_printer;
    QPrintPreviewWidget* m_preview = nullptr;
    // Окно просмотра внутри QPrintPreviewWidget — ловим на нём Ctrl + колесо.
    QPointer<QWidget> m_previewViewport;
    std::optional<ReportGenerationWorker::Input> m_snapshot;
    std::optional<PastSession> m_past;
#ifdef MPIPLATFORM_EXPERIMENTAL_REPORT_COMPARE
    // Виджеты сравнения живут на m_page и удаляются вместе с ней — раньше
    // этого объекта (см. деструктор).
    std::unique_ptr<ReportComparePresenter> m_compare;
#endif
};
