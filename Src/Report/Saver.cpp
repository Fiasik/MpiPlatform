#include "Saver.h"

#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QDataStream>
#include <QDebug>

#include "Widgets/Chart/ChartView.h"
#include "Widgets/Chart/ChartType.h"
#include "Report/Csv/Writer.h"

#include "xlsxdocument.h"
#include "xlsxdatavalidation.h"
#include "xlsxformat.h"
#include "xlsxcell.h"
#include "xlsxchart.h"
#include "xlsxcellrange.h"

using namespace QXlsx;

namespace Report {

Saver::Saver(QObject* parent)
    : QObject(parent)
{
}

Saver::~Saver()
{
    cleanupEmptyDirectory();
}

const QDir& Saver::directory() const
{
    return m_dir;
}

void Saver::setBasePath(const QString& basePath)
{
    if (m_isDirectoryCreated) {
        qWarning() << "[Saver]: base path is ignored because directory is already created:"
                   << m_dir.path();
        return;
    }

    m_basePath = basePath;
}

bool Saver::ensureDirectory()
{
    if (m_isDirectoryCreated)
        return true;

    if (createAutoDirectory())
        return true;

    return chooseDirectoryManually();
}

bool Saver::createAutoDirectory()
{
    const QString dateFolder =
        QDate::currentDate().toString(QStringLiteral("dd_MM_yyyy"));

    QDir baseDir(QCoreApplication::applicationDirPath());

    QString rootPath = baseDir.path();
    if (!m_basePath.isEmpty())
        rootPath = baseDir.filePath(m_basePath);

    QDir rootDir(rootPath);

    if (!rootDir.exists()) {
        if (!QDir().mkpath(rootPath))
            return false;
        rootDir.setPath(rootPath);
    }

    if (rootDir.exists(dateFolder)) {
        for (int i = 2; i < 50; ++i) {
            const QString folderName = QStringLiteral("%1_%2")
                                           .arg(dateFolder)
                                           .arg(i);
            if (rootDir.mkdir(folderName)) {
                m_dir = QDir(rootDir.filePath(folderName));
                m_isDirectoryCreated = true;
                return true;
            }
        }
        return false;
    }

    if (!rootDir.mkdir(dateFolder))
        return false;

    m_dir = QDir(rootDir.filePath(dateFolder));
    m_isDirectoryCreated = true;
    return true;
}

bool Saver::chooseDirectoryManually()
{
    QDir startDir = m_isDirectoryCreated
        ? m_dir
        : QDir(QCoreApplication::applicationDirPath());

    while (!m_isDirectoryCreated) {
        QString folderPath;

        do {
            emit setDirectoryToSave(startDir.path(), folderPath);
        } while (folderPath.isEmpty());

        QDir chosenDir(folderPath);

        if (!chosenDir.exists()) {
            if (!QDir().mkpath(folderPath))
                continue;
            chosenDir.setPath(folderPath);
        }

        if (!chosenDir.isEmpty()) {
            bool answer = emit question(
                QStringLiteral("Внимание!"),
                QStringLiteral("Папка не пуста. Вы действительно хотите выбрать эту папку?"));

            if (!answer)
                continue;
        }

        m_dir = chosenDir;
        m_isDirectoryCreated = true;
    }

    return true;
}

void Saver::saveChartSnapshot(Widgets::Chart::ChartType chart,
                              const QImage& image,
                              Widgets::Chart::ChartView* chartView)
{
    if (image.isNull())
        return;

    if (!chartView)
        return;

    if (!ensureDirectory())
        return;

    const QString name = Widgets::Chart::chartTypeDisplayName(chart);
    const quint16 index = ++m_chartCounter[name];

    const QString baseName =
        QStringLiteral("%1_%2").arg(name).arg(index);

    const QString imagePath =
        m_dir.filePath(baseName + QStringLiteral(".png"));

    if (!image.save(imagePath))
        qWarning() << "[Saver] Не удалось сохранить снимок графика:" << imagePath;

    QFile out(m_dir.filePath(baseName + QStringLiteral(".data")));

    if (out.open(QIODevice::WriteOnly)) {
        QDataStream stream(&out);
        stream.setVersion(QDataStream::Qt_6_2);
        chartView->saveToStream(stream);
        out.flush();
        out.close();
    } else {
        qWarning() << "[Saver] Не удалось открыть файл для снимка графика:"
                   << out.fileName() << out.errorString();
    }
}

bool Saver::saveReport(const Excel::ReportData& report, const QString& templatePath)
{
    if (!ensureDirectory())
        return false;

    const QString reportPath =
        m_dir.filePath(QStringLiteral("report.xlsx"));

    Document xlsx(templatePath);

    for (const auto& item : report.data) {
        if (!xlsx.selectSheet(item.sheet)) {
            qWarning() << "Не найден лист" << item.sheet;
            continue;
        }

        if (item.row <= 0 || item.col <= 0) {
            qWarning() << "Невалидные координаты ячейки"
                       << item.row << item.col;
            continue;
        }

        if (item.hasBackgroundColor) {
            // Берём формат уже существующей в шаблоне ячейки (выравнивание,
            // шрифт и т.д.) и только добавляем заливку — xlsx.write(...,
            // format) заменяет формат ячейки целиком, а не дополняет его.
            Format format;
            if (const Cell* cell = xlsx.cellAt(item.row, item.col))
                format = cell->format();

            format.setFillPattern(Format::PatternSolid);
            format.setPatternBackgroundColor(item.backgroundColor);
            xlsx.write(item.row, item.col, item.value, format);
        } else {
            xlsx.write(item.row, item.col, item.value);
        }
    }

    constexpr int targetWidth = 885;
    constexpr int targetHeight = 460;

    for (const auto& img : report.images) {
        if (img.image.isNull())
            continue;

        if (!xlsx.selectSheet(img.sheet)) {
            qWarning() << "Не найден лист" << img.sheet;
            continue;
        }

        if (img.row <= 0 || img.col <= 0) {
            qWarning() << "Невалидные координаты изображения"
                       << img.row << img.col;
            continue;
        }

        const QImage scaled = img.image.scaled(
            targetWidth,
            targetHeight,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation
            );

        xlsx.insertImage(img.row, img.col, scaled);
    }

    // ВРЕМЕННО НЕ ИСПОЛЬЗУЕТСЯ — report.charts сейчас всегда пуст (Pages/
    // Blocks снова пишут через writer.image(), не writer.chart()), цикл
    // ниже оставлен рабочим на будущее (см. ChartCell в Data.h).
    constexpr QSize chartSize(885, 460);

    // Ряды каждого графика кладём в свободные столбцы того же листа, правее
    // реального контента шаблона (ТО занят до U, ТШР — до V) — так график
    // остаётся нативным Excel-объектом (в отличие от растровой картинки) и
    // при этом не требует отдельного листа "Данные...". kChartDataBaseColumn
    // — с запасом; kChartDataColumnGap отделяет блок данных одного графика
    // от следующего на том же листе.
    constexpr int kChartDataBaseColumn = 28; // AB
    constexpr int kChartDataColumnGap = 2;
    QHash<QString, int> nextDataColumn;

    for (const auto& cc : report.charts) {
        if (cc.series.isEmpty())
            continue;

        if (!xlsx.selectSheet(cc.sheet)) {
            qWarning() << "Не найден лист" << cc.sheet << "для графика" << cc.title;
            continue;
        }

        if (cc.anchorRow <= 0 || cc.anchorCol <= 0) {
            qWarning() << "Невалидные координаты графика" << cc.title
                       << cc.anchorRow << cc.anchorCol;
            continue;
        }

        int maxPoints = 0;
        for (const ChartSeries& s : cc.series)
            maxPoints = qMax(maxPoints, s.points.size());
        if (maxPoints == 0)
            continue;

        int& dataCol = nextDataColumn[cc.sheet];
        if (dataCol == 0)
            dataCol = kChartDataBaseColumn;
        const int startCol = dataCol;

        // Шапка (row=1) — заголовки столбцов; addSeries() ниже возьмёт их
        // как имена рядов (headerH=true, swapHeaders=true). Все ряды одного
        // графика сняты по общей сетке (см. Program::updateMainCharts/
        // updateTimeChart — один и тот же x на серию за один вызов), поэтому
        // общий x-столбец из первого ряда корректен и для остальных.
        xlsx.write(1, startCol, cc.xTitle);
        for (int i = 0; i < cc.series.size(); ++i)
            xlsx.write(1, startCol + 1 + i, cc.series.at(i).name);

        const ChartSeries& xSeries = cc.series.first();
        for (int row = 0; row < xSeries.points.size(); ++row)
            xlsx.write(row + 2, startCol, xSeries.points.at(row).x());

        for (int i = 0; i < cc.series.size(); ++i) {
            const ChartSeries& s = cc.series.at(i);
            for (int row = 0; row < s.points.size(); ++row)
                xlsx.write(row + 2, startCol + 1 + i, s.points.at(row).y());
        }

        QXlsx::Chart* chart = xlsx.insertChart(cc.anchorRow, cc.anchorCol, chartSize);
        chart->setChartType(QXlsx::Chart::CT_ScatterChart);
        chart->setChartTitle(cc.title);
        chart->setChartLegend(QXlsx::Chart::Bottom);
        chart->setAxisTitle(QXlsx::Chart::Bottom, cc.xTitle);
        chart->setAxisTitle(QXlsx::Chart::Left, cc.yTitle);
        chart->addSeries(
            QXlsx::CellRange(1, startCol, maxPoints + 1, startCol + cc.series.size()),
            xlsx.sheet(cc.sheet),
            /*headerH=*/true, /*headerV=*/false, /*swapHeaders=*/true);
        chart->setSeriesLineStyle(/*showMarker=*/false, /*lineWidthPt=*/1.0);

        dataCol = startCol + cc.series.size() + 1 + kChartDataColumnGap;
    }

    for (const auto& v : report.validation) {
        if (!xlsx.selectSheet(v.sheet)) {
            qWarning() << "Не найден лист для validation:" << v.sheet;
            continue;
        }

        DataValidation validation(
            DataValidation::List,
            DataValidation::Equal,
            v.formula
            );

        validation.addRange(v.range);
        xlsx.addDataValidation(validation);
    }

    const bool ok = xlsx.saveAs(reportPath);

    if (!ok) {
        qWarning() << "Не удалось сохранить отчёт:"
                   << reportPath
                   << "Возможно, report.xlsx открыт в Excel.";
    }

    return ok && QFile::exists(reportPath);
}

QString Saver::reportPdfPath(const QString& fileName) const
{
    return m_dir.filePath(fileName);
}

bool Saver::saveChartCsv(Widgets::Chart::ChartType chart, const ChartSnapshot& snapshot)
{
    if (!ensureDirectory())
        return false;

    const QString path =
        m_dir.filePath(Widgets::Chart::chartTypeDisplayName(chart) + QStringLiteral(".csv"));

    return Csv::writeChartCsv(path, snapshot);
}

void Saver::cleanupEmptyDirectory()
{
    if (!m_isDirectoryCreated)
        return;

    if (!m_dir.exists())
        return;

    const QFileInfoList entries = m_dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries);

    if (!entries.isEmpty())
        return;

    const QString path = m_dir.absolutePath();
    QDir parent = m_dir;
    parent.cdUp();
    parent.rmdir(QFileInfo(path).fileName());
}

}