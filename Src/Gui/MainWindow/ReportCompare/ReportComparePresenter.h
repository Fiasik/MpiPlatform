#pragma once

// Сравнение отчётов во вкладке «Предпросмотр» (эксперимент, CMake-опция
// MPIPLATFORM_EXPERIMENTAL_REPORT_COMPARE, по умолчанию ON).
//
// В строке инструментов появляется «Сравнить с»: выбранная прошлая сессия
// той же позиции встаёт слева (её акт собирается заново по данным сессии,
// см. SessionReportLoader), текущая — справа, как её запишет «Сформировать
// отчёт». Ячейки Акта ТО, текст которых отличается, подсвечивает сам
// QtRPT при отрисовке: заливка — у изменившегося значения, синяя рамка — у
// сменившегося вывода («Примечание»), оранжевая — у ячейки, выбранной в
// списке отличий. Второй вид — таблица результатов двух сессий. Над
// страницами — предупреждение, если сравнение может обмануть (другое
// направление хода, другая карточка клапана).
//
// Виджеты (ReportCompareTools.ui, ReportComparePanel.ui) встраивает
// ReportPreviewPresenter: панель — в свою строку инструментов, страницу — в
// свой стек; удаляет их он же вместе со страницей вкладки.

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>

#include <memory>
#include <optional>

#include "Gui/MainWindow/ReportCompare/ReportDiff.h"
#include "Gui/MainWindow/ReportCompare/SessionReportLoader.h"
#include "Gui/MainWindow/ReportGenerationWorker.h"
#include "Report/Pdf/Builder.h"

class QPrinter;
class QPrintPreviewWidget;
class QWidget;
class RptFieldObject;

namespace Ui {
class ReportCompareTools;
class ReportComparePanel;
}

class ReportComparePresenter : public QObject
{
    Q_OBJECT

public:
    explicit ReportComparePresenter(QObject* parent = nullptr);
    ~ReportComparePresenter() override;

    QWidget* tools() const { return m_tools; }
    QWidget* panel() const { return m_panel; }

    // Текущая сессия — её в списке «Сравнить с» нет.
    void setCurrentSessionId(qint64 sessionId);

    // Свежий снимок данных отчёта текущей сессии (тот же, что рисует
    // предпросмотр): обновляет список прошлых сессий и отличия.
    void setCurrent(const ReportGenerationWorker::Input& current);

    // Выбрана сессия для сравнения — предпросмотр показывает panel().
    bool isActive() const;
    void render(Report::Pdf::Act act);

    // Страницы и масштаб — обе стороны разом. pagesShown() — false в виде
    // «Таблица»: листать там нечего.
    bool pagesShown() const;
    int pageCount() const;
    int currentPage() const;
    void showPage(int page);
    void zoomBy(qreal factor);
    void fitToWidth();
    void fitInView();

signals:
    // Сравнение включили/выключили — предпросмотру пересобрать вид.
    void activeChanged();
    // Сменилась страница, их число или вид.
    void pagesChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void fillSessions();
    void selectSession(int index);
    void updateDiffs();
    void fillDiffList();
    void fillTable();
    void updateWarning();
    void updateHeaders();
    void updateTools();
    void repaintPages();
    void paintSide(QPrinter* printer, bool older);
    void highlight(RptFieldObject& field) const;
    void connectScrolling();
    QString olderDate(const QString& format) const;

    QPointer<QWidget> m_tools;
    QPointer<QWidget> m_panel;
    std::unique_ptr<Ui::ReportCompareTools> m_toolsUi;
    std::unique_ptr<Ui::ReportComparePanel> m_ui;
    // Как у предпросмотра и PDF-файла: HighResolution + PdfFormat.
    std::unique_ptr<QPrinter> m_olderPrinter;
    std::unique_ptr<QPrinter> m_newerPrinter;
    QPrintPreviewWidget* m_olderPreview = nullptr;
    QPrintPreviewWidget* m_newerPreview = nullptr;
    // Окна просмотра внутри QPrintPreviewWidget — Ctrl + колесо.
    QList<QPointer<QWidget>> m_viewports;
    bool m_syncingScroll = false;

    qint64 m_currentSessionId = -1;
    std::optional<ReportGenerationWorker::Input> m_current;
    QVector<ReportCompare::PastSession> m_sessions;
    std::optional<ReportCompare::PastSession> m_selected;
    std::optional<ReportGenerationWorker::Input> m_older;
    QString m_olderError;
    // Собранные данные прошлых сессий: обновление предпросмотра (каждый
    // показ вкладки) не перечитывает базу и графики.
    QHash<qint64, ReportGenerationWorker::Input> m_cache;
    std::optional<Report::Pdf::Act> m_act;

    QVector<ReportCompare::DiffItem> m_diffs;
    QSet<QString> m_diffKeys;
    QString m_selectedKey;
};
