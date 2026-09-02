#pragma once

#include <QObject>
#include <QDir>
#include <QHash>
#include <QString>

#include "Excel/Data.h"
#include "Widgets/Chart/ChartType.h"
#include "Storage/ChartSnapshot.h"

namespace Widgets::Chart {
class ChartView;
}

namespace Report {

class Saver : public QObject
{
    Q_OBJECT

public:
    explicit Saver(QObject* parent = nullptr);
    ~Saver() override;

    [[nodiscard]] const QDir& directory() const;

    void setBasePath(const QString& basePath);

    void saveChartSnapshot(
        Widgets::Chart::ChartType chart,
        const QImage& image,
        Widgets::Chart::ChartView* chartView);
    bool ensureDirectory();

    bool saveReport(const Excel::ReportData& report, const QString& templatePath);

    // Report::Pdf::Builder сам пишет PDF по пути (QtRPT::printPDF), в
    // отличие от saveReport() (данные в память -> Saver сохраняет) —
    // тут только вычисляем путь внутри сессионной папки, как уже делает
    // MainWindow::commentsPdfPath() для отдельного PDF-комментария.
    // ensureDirectory() нужно вызвать заранее, отдельно (см. тот же
    // прецедент).
    [[nodiscard]] QString reportPdfPath(const QString& fileName) const;

    bool saveChartCsv(Widgets::Chart::ChartType chart, const ChartSnapshot& snapshot);

private:
    bool createAutoDirectory();
    bool chooseDirectoryManually();
    void cleanupEmptyDirectory();

private:
    QDir m_dir;
    QString m_basePath;
    bool m_isDirectoryCreated = false;
    QHash<QString, quint16> m_chartCounter;

signals:
    bool question(const QString& title, const QString& text);
    void setDirectoryToSave(const QString& currentPath, QString& result);
};

}