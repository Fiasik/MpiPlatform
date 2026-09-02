// Спайк: вместо того чтобы вставлять в отчёт растровый снимок графика
// (Report::Saver::saveReport -> xlsx.insertImage), пишем сами точки в ячейки
// на отдельном листе и строим нативный график средствами Excel поверх этого
// диапазона (xlsxdocument.h -> insertChart/Chart::addSeries). Работаем на
// КОПИИ шаблона отчёта (Report_copy.xlsx), исходный Reports/Report.xlsx не
// трогаем.
//
// Три графика — Task/Pressure/Friction — ставятся на лист "ТО" ровно в те
// позиции и того же размера, что сейчас использует Report::Builder.cpp /
// Report::Saver.cpp для вставки картинок (taskImageRow=83, pressureImageRow
// =111, frictionImageRow=139, 885x460px). Форма данных Pressure/Friction
// подогнана под реальные референсы (Pressure_1.bmp/Friction_1.bmp).

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QRandomGenerator>
#include <cmath>

#include "xlsxdocument.h"
#include "xlsxchart.h"
#include "xlsxcellrange.h"
#include "xlsxabstractsheet.h"

using namespace QXlsx;

namespace {

double commandedStroke(double mA)
{
    constexpr double rampStart = 5.0;
    constexpr double rampEnd = 19.0;
    constexpr double strokeMax = 24.7;

    if (mA <= rampStart) return 0.0;
    if (mA >= rampEnd) return strokeMax;
    return strokeMax * (mA - rampStart) / (rampEnd - rampStart);
}

double frictionForce(double x)
{
    if (x < 2.0) {
        const double t = (x - 1.0) / 1.0;
        return 63.0 - t * 16.0;
    }
    if (x < 18.0)
        return 46.0 + 2.0 * std::sin((x - 2.0) / 3.0);

    const double t = (x - 18.0) / (24.0 - 18.0);
    return 46.0 + t * 9.0;
}

double strokeFromPressure(double bar, double startBar, double endBar)
{
    constexpr double strokeMax = 25.6;
    if (bar <= startBar) return 0.0;
    if (bar >= endBar) return strokeMax;
    return strokeMax * (bar - startBar) / (endBar - startBar);
}

// Вставляет scatter-график на текущий выбранный лист поверх диапазона
// dataSheet!A(1..lastRow) x B..(1+seriesCount), тонкими линиями без
// маркеров, с жёстко заданным диапазоном оси X.
Chart* insertThinScatterChart(
    Document& xlsx,
    AbstractSheet* dataSheetPtr,
    int anchorRow,
    const QString& title,
    const QString& xTitle,
    const QString& yTitle,
    double xMin,
    double xMax,
    int lastRow,
    int seriesCount)
{
    Chart* chart = xlsx.insertChart(anchorRow, 1, QSize(885, 460));
    chart->setChartType(Chart::CT_ScatterChart);
    chart->setChartTitle(title);
    chart->setChartLegend(Chart::Bottom);
    chart->setAxisTitle(Chart::Bottom, xTitle);
    chart->setAxisTitle(Chart::Left, yTitle);

    chart->addSeries(CellRange(1, 1, lastRow, 1 + seriesCount), dataSheetPtr,
                      /*headerH=*/true, /*headerV=*/false, /*swapHeaders=*/true);
    chart->setSeriesLineStyle(/*showMarker=*/false, /*lineWidthPt=*/1.0);
    chart->setAxisRange(Chart::Bottom, xMin, xMax);

    return chart;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString inputPath = QDir::current().filePath(QStringLiteral("Report_copy.xlsx"));
    const QString outputPath = QDir::current().filePath(QStringLiteral("Report_native_chart_3.xlsx"));

    if (!QFile::exists(inputPath)) {
        qWarning() << "Не найден" << inputPath << "— копия шаблона не скопирована рядом с exe?";
        return 1;
    }

    Document xlsx(inputPath);
    auto* rng = QRandomGenerator::global();

    // ---------- Task ----------
    {
        const QString sheet = QStringLiteral("Данные Task");
        xlsx.addSheet(sheet);
        xlsx.selectSheet(sheet);

        xlsx.write(1, 1, QStringLiteral("Ток, мА"));
        xlsx.write(1, 2, QStringLiteral("Задание, мм"));
        xlsx.write(1, 3, QStringLiteral("Датчик линейных перемещений, мм"));

        constexpr double mAMin = 3.0;
        constexpr double mAMax = 21.0;
        constexpr int sampleCount = 450;

        for (int i = 0; i < sampleCount; ++i) {
            const double t = static_cast<double>(i) / (sampleCount - 1);
            const double mA = mAMin + t * (mAMax - mAMin);
            const double sensorLag = commandedStroke(qMax(mAMin, mA - 0.25));
            const double sensorNoise = (rng->generateDouble() - 0.5) * 0.3;

            const int row = i + 2;
            xlsx.write(row, 1, mA);
            xlsx.write(row, 2, commandedStroke(mA));
            xlsx.write(row, 3, qMax(0.0, sensorLag + sensorNoise));
        }

        xlsx.selectSheet(QStringLiteral("ТО"));
        insertThinScatterChart(xlsx, xlsx.sheet(sheet), /*anchorRow=*/83,
            QStringLiteral("Task (нативный график Excel)"),
            QStringLiteral("Ток, мА"), QStringLiteral("Перемещение, мм"),
            mAMin, mAMax, sampleCount + 1, /*seriesCount=*/2);
    }

    // ---------- Pressure: форма под Pressure_1.bmp, прямой/обратный ход ----------
    {
        const QString sheet = QStringLiteral("Данные Pressure");
        xlsx.addSheet(sheet);
        xlsx.selectSheet(sheet);

        xlsx.write(1, 1, QStringLiteral("Давление, бар"));
        xlsx.write(1, 2, QStringLiteral("Перемещение (прямой ход), мм"));
        xlsx.write(1, 3, QStringLiteral("Перемещение (обратный ход), мм"));

        constexpr double barMin = 0.0;
        constexpr double barMax = 2.0;
        constexpr int sampleCount = 350;
        constexpr double fwdStart = 0.42, fwdEnd = 1.02;
        constexpr double bwdStart = 0.50, bwdEnd = 1.10;

        for (int i = 0; i < sampleCount; ++i) {
            const double t = static_cast<double>(i) / (sampleCount - 1);
            const double bar = barMin + t * (barMax - barMin);

            double fwd = strokeFromPressure(bar, fwdStart, fwdEnd);
            double bwd = strokeFromPressure(bar, bwdStart, bwdEnd);
            if (bar > fwdStart && bar < fwdEnd) fwd += (rng->generateDouble() - 0.5) * 0.4;
            if (bar > bwdStart && bar < bwdEnd) bwd += (rng->generateDouble() - 0.5) * 0.4;

            const int row = i + 2;
            xlsx.write(row, 1, bar);
            xlsx.write(row, 2, qMax(0.0, fwd));
            xlsx.write(row, 3, qMax(0.0, bwd));
        }

        xlsx.selectSheet(QStringLiteral("ТО"));
        insertThinScatterChart(xlsx, xlsx.sheet(sheet), /*anchorRow=*/111,
            QStringLiteral("Pressure (нативный график Excel)"),
            QStringLiteral("Давление, бар"), QStringLiteral("Перемещение, мм"),
            barMin, barMax, sampleCount + 1, /*seriesCount=*/2);
    }

    // ---------- Friction: форма под Friction_1.bmp ----------
    {
        const QString sheet = QStringLiteral("Данные Friction");
        xlsx.addSheet(sheet);
        xlsx.selectSheet(sheet);

        xlsx.write(1, 1, QStringLiteral("Перемещение, мм"));
        xlsx.write(1, 2, QStringLiteral("Трение от перемещения, Н"));

        constexpr double xMin = 1.0;
        constexpr double xMax = 24.0;
        constexpr int sampleCount = 400;

        for (int i = 0; i < sampleCount; ++i) {
            const double t = static_cast<double>(i) / (sampleCount - 1);
            const double x = xMin + t * (xMax - xMin);
            const double noise = (rng->generateDouble() - 0.5) * 3.0;

            const int row = i + 2;
            xlsx.write(row, 1, x);
            xlsx.write(row, 2, qMax(0.0, frictionForce(x) + noise));
        }

        xlsx.selectSheet(QStringLiteral("ТО"));
        insertThinScatterChart(xlsx, xlsx.sheet(sheet), /*anchorRow=*/139,
            QStringLiteral("Friction (нативный график Excel)"),
            QStringLiteral("Перемещение, мм"), QStringLiteral("Трение, Н"),
            xMin, xMax, sampleCount + 1, /*seriesCount=*/1);
    }

    if (!xlsx.saveAs(outputPath)) {
        qWarning() << "Не удалось сохранить" << outputPath;
        return 1;
    }

    qInfo().noquote() << "Сохранено:" << outputPath;

    // Простая проверка на битость: открываем то, что сами только что
    // записали, и читаем обратно данные Task.
    Document check(outputPath);
    if (!check.selectSheet(QStringLiteral("Данные Task"))) {
        qWarning() << "Файл сохранился, но лист с данными не открывается обратно!";
        return 1;
    }

    const QVariant probe = check.read(2, 1);
    if (!probe.isValid()) {
        qWarning() << "Лист открылся, но данные в ячейке A2 не читаются!";
        return 1;
    }

    qInfo().noquote() << "Повторное открытие прошло успешно, A2 =" << probe.toString()
                       << "— файл не битый.";

    return 0;
}
