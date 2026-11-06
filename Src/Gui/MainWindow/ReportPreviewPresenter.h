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

#include <QObject>
#include <QPointer>

#include <memory>
#include <optional>

#include "ReportGenerationWorker.h"

class QPrinter;
class QPrintPreviewWidget;
class QWidget;
class ReportGenerationPresenter;

namespace Ui { class ReportPreviewPanel; }

class ReportPreviewPresenter : public QObject
{
    Q_OBJECT

public:
    explicit ReportPreviewPresenter(ReportGenerationPresenter* report, QObject* parent = nullptr);
    ~ReportPreviewPresenter() override;

    // Страница вкладки; владельцем становится QTabWidget, куда её добавят.
    QWidget* panel() const { return m_page; }

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

    ReportGenerationPresenter* m_report;
    QPointer<QWidget> m_page;
    std::unique_ptr<Ui::ReportPreviewPanel> m_ui;
    // Как у PDF-файла (HighResolution + PdfFormat), см. Builder::preview().
    std::unique_ptr<QPrinter> m_printer;
    QPrintPreviewWidget* m_preview = nullptr;
    // Окно просмотра внутри QPrintPreviewWidget — ловим на нём Ctrl + колесо.
    QPointer<QWidget> m_previewViewport;
    std::optional<ReportGenerationWorker::Input> m_snapshot;
};
