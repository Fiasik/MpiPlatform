// Синтетика — тот же приём, что и в experiments/verify-report/main.cpp
// (реальные структуры данных, без реального железа/анализаторов). Генерим
// лист "ТО" для Standard/Ilim/Ski одним запуском одного бинарника — не
// три сборки с разным MPIPLATFORM_VARIANT, а один прогон с runtime-параметром
// variant, см. комментарий в Generator.cpp про рантайм- vs compile-time
// ветвление.
#include "Generator.h"

#include <QGuiApplication>
#include <QDir>
#include <QImage>
#include <QMap>
#include <QPainter>
#include <QDebug>
#include <QtMath>
#include <QVector>

namespace {

// Плейсхолдер вместо реального ChartView::grab() — эксперимент нарочно не
// тянет за собой Gui/Widgets::Chart (это "без вспомогательных файлов", то
// есть и без зависимости от живого UI тоже, не только без .xlsx-шаблона).
QImage placeholderChart(const QString& title, const QColor& lineColor)
{
    QImage img(1280, 720, QImage::Format_ARGB32);
    img.fill(Qt::white);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);

    p.setPen(QPen(QColor("#B9C0CB"), 1));
    p.drawRect(0, 0, img.width() - 1, img.height() - 1);

    p.setPen(QPen(lineColor, 3));
    QPolygon poly;
    for (int i = 0; i <= 20; ++i) {
        const int x = i * img.width() / 20;
        const int y = img.height() - 60 - int(200 * qSin(i * 0.4) + 200);
        poly << QPoint(x, y);
    }
    p.drawPolyline(poly);

    p.setPen(Qt::black);
    QFont f = p.font();
    f.setPointSize(16);
    p.setFont(f);
    p.drawText(QRect(20, 20, img.width() - 40, 40), Qt::AlignLeft, title);

    return img;
}

}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    ObjectInfo object;
    object.object = "Установка первичной переработки нефти";
    object.manufactory = "Цех №3";
    object.department = "Компрессорная станция";
    object.FIO = "Иванов И. И.";

    ValveInfo valve;
    valve.positionNumber = "12-FV-034";
    valve.manufacturer = "Valtek";
    valve.valveModel = "V200";
    valve.serialNumber = "SN-2024-1187";
    valve.DN = "DN50";
    valve.PN = "PN40";
    valve.positionerModel = "SVI II AP";
    valve.dinamicErrorRecomend = "2.0";
    valve.valveStroke = "25.0";
    valve.driveModel = "39-71361";
    valve.driveRangeLow = 0.4;
    valve.driveRangeHigh = 1.2;
    valve.solenoidValveModel = "СЭП-101";
    valve.limitSwitchModel = "КВ-24";
    valve.positionSensorModel = "ДП-12";
    valve.driveType = DriveType::SpringDiaphragm;

    MaterialsOfComponentParts materials;
    materials.corpus = "Сталь";
    materials.cap = "Сталь";
    materials.saddle = "Сталь";
    materials.CV = "12.4/15.5";
    materials.ball = "Сталь нержавеющая";
    materials.disk = "-";
    materials.plunger = "Сталь нержавеющая";
    materials.shaft = "-";
    materials.stock = "Сталь нержавеющая";
    materials.guideSleeve = "PTFE";

    OtherParameters params;
    params.date = "02.09.2026";
    params.safePosition = "Открыт";
    params.strokeMovement = "линейное";

    Telemetry telemetry;
    telemetry.valveStrokeRecord.real = 24.8;
    telemetry.supplyRecord.pressure_bar = 4.5;
    telemetry.crossingStatus.dynamicError = CrossingStatus::State::Ok;
    telemetry.crossingStatus.valveStroke = CrossingStatus::State::Fail;
    telemetry.crossingStatus.spring = CrossingStatus::State::Ok;
    telemetry.crossingStatus.frictionPercent = CrossingStatus::State::Ok;

    Domain::Tests::Main::Result mainResult;
    mainResult.dynamicErrorReal = 1.2;
    mainResult.lowLimitPressure = 0.6;
    mainResult.highLimitPressure = 3.8;
    mainResult.springLow = 0.4;
    mainResult.springHigh = 1.2;
    mainResult.frictionPercent = 4.1;
    mainResult.frictionForce = 186.0;
    telemetry.testMain = mainResult;

    Domain::Tests::Stroke::Result strokeResult;
    strokeResult.forwardTimeMs = 3200;
    strokeResult.backwardTimeMs = 3400;
    telemetry.testStroke = strokeResult;

    using Report::Html::ConclusionField;
    const QMap<ConclusionField, QString> conclusionAnswers = {
        { ConclusionField::SeatingZone, "без видимых дефектов" },
        { ConclusionField::PositionerAdaptation, "удовлетворительно" },
        { ConclusionField::AuxiliaryEquipment, "удовлетворительно" },
        { ConclusionField::FrictionGraph, "зоны повышенного трения отсутствуют" },
        { ConclusionField::GlandLeak, "видимая течь не наблюдается" },
    };
    const QString conclusionSummary =
        "Работа позиционера неудовлетворительная. Произвести ремонт/замену позиционера/запчастей "
        "позиционера, калибровку позиционера. Работа механической части неудовлетворительная. "
        "Произвести ревизию клапана: выполнить разборку, чистку, мойку внутренних деталей, "
        "проверка рабочей поверхности плунжер/седло/шар, проверка штока/вала (при необходимости "
        "заменить). Произвести замену сальниковой набивки, уплотнений. Произвести ревизию привода. "
        "После ремонта произвести настройку позиционера.";
    const QStringList spareParts = {
        "сальниковое уплотнение (согласно исполнению)",
        "комплект мягких уплотнений",
        "диск/уплотнение диска",
    };

    ReportGen::TechnicalInspectionData data{
        object, valve, materials, params, telemetry,
        /*positionerModel=*/true,
        /*includeSolenoid=*/false,
        conclusionAnswers,
        conclusionSummary,
        spareParts,
        placeholderChart("Задание", QColor("#2A689F")),
        placeholderChart("Давление от перемещения", QColor("#D62828")),
        placeholderChart("Сила трения от перемещения", QColor("#457448")),
    };

    const QString outDir = QStringLiteral(MPIPLATFORM_SOURCE_DIR
                                          "/experiments/report-generator/out");
    QDir().mkpath(outDir);

    struct Case { Domain::Variant variant; QString name; bool solenoid; };
    const QVector<Case> cases = {
        {Domain::Variant::Standard, "Standard", false},
        {Domain::Variant::Ilim, "Ilim", false},
        {Domain::Variant::Ski, "Ski", false},
        // Ski C_SACVT реально включает соленоидные поля в ТО (см. Builder::
        // buildSki()) — отдельный кейс, чтобы проверить, что параметр
        // includeSolenoid действительно работает, а не просто существует.
        {Domain::Variant::Ski, "Ski_SACVT", true},
    };

    bool allOk = true;
    for (const auto& c : cases) {
        ReportGen::TechnicalInspectionData caseData = data;
        caseData.includeSolenoid = c.solenoid;

        const QString path = outDir + "/TO_" + c.name + ".xlsx";
        const bool ok = ReportGen::generateTechnicalInspection(caseData, c.variant, "ТО", path);
        qInfo().noquote() << (ok ? "OK  " : "FAIL") << path;
        allOk = allOk && ok;
    }

    return allOk ? 0 : 1;
}
