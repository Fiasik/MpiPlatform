#pragma once

#include <QObject>
#include <QByteArray>
#include <QImage>
#include <QDir>
#include <QHash>
#include <QString>

#include "Excel/Data.h"
#include "Shared/Chart/ChartType.h"
#include "Storage/ChartSnapshot.h"

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
        Shared::Chart::ChartType chart,
        const QImage& image,
        const QByteArray& chartData);
    bool ensureDirectory();

    // Забывает текущую папку сессии: следующий ensureDirectory() создаст
    // новую по актуальному basePath. Нужен, когда по "Назад" сменилась
    // позиция/объект — графики новой сессии не должны попасть в папку
    // прежней. Пустая (ничего не сохранившая) папка убирается.
    void resetDirectory();

    bool saveReport(const Excel::ReportData& report, const QString& templatePath);

    // report.xlsx внутри сессионной папки — и для шаблонного saveReport(),
    // и для Report::Excel::generate(), и для кнопки "Открыть отчёт".
    [[nodiscard]] QString reportXlsxPath() const;

    // Report::Pdf::Builder сам пишет PDF по пути (QtRPT::printPDF), в
    // отличие от saveReport() (данные в память -> Saver сохраняет) —
    // тут только вычисляем путь внутри сессионной папки, как уже делает
    // MainWindow::commentsPdfPath() для отдельного PDF-комментария.
    // ensureDirectory() нужно вызвать заранее, отдельно (см. тот же
    // прецедент).
    [[nodiscard]] QString reportPdfPath(const QString& fileName) const;

    bool saveChartCsv(Shared::Chart::ChartType chart, const ChartSnapshot& snapshot);

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
    // Папка сессии создана (или выбрана вручную) — по ней сессию привязывают
    // к sessions.folder_path.
    void directoryCreated(const QString& absolutePath);

    bool question(const QString& title, const QString& text);
    void setDirectoryToSave(const QString& currentPath, QString& result);
};

}