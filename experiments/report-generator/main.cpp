// Превью report.xlsx без железа: синтетические данные (те же структуры, что
// у приложения) -> боевой Report::Excel::generate(), все варианты из одной
// сборки:
//  full.xlsx          — Ilim, все тесты (ТО, ТШР, ТЧР, ТРС) и длинные тексты:
//                       перенос строк, каждый акт влезает в свои страницы;
//  minimal.xlsx       — Standard, ни одного теста, пустые поля и списки:
//                       только лист ТО, без скрытого "Списки";
//  ski_<паттерн>.xlsx — Ski, те же данные + циклические тесты;
//  ski_B_CVT_empty    — Ski без данных: пустые таблицы ЦТ, подписи в конце.
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QMap>
#include <QPainter>
#include <QDebug>
#include <QtMath>

#include <iterator>

#include "Domain/Tests/Option/Resolution/Limits.h"
#include "Domain/Tests/Option/Response/Limits.h"
#include "Report/Context.h"
#include "Report/Excel/Generator.h"
#include "Storage/ChartImageStorage.h"
#include "Storage/ChartSnapshotStorage.h"
#include "Storage/ChartSnapshot.h"
#include "Widgets/Chart/ChartType.h"

namespace {

// Заглушка вместо ChartView::grab() — превью не тянет за собой живой UI.
QImage placeholderChart(const QString& title, const QColor& lineColor)
{
    QImage img(1280, 640, QImage::Format_ARGB32);
    img.fill(Qt::white);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor("#B9C0CB"), 1));
    p.drawRect(0, 0, img.width() - 1, img.height() - 1);

    p.setPen(QPen(lineColor, 3));
    QPolygon poly;
    for (int i = 0; i <= 40; ++i)
        poly << QPoint(i * img.width() / 40,
                       img.height() / 2 - int(180 * qSin(i * 0.3)));
    p.drawPolyline(poly);

    p.setPen(Qt::black);
    QFont f = p.font();
    f.setPointSize(20);
    p.setFont(f);
    p.drawText(QRect(20, 20, img.width() - 40, 40), Qt::AlignLeft, title);
    return img;
}

// Тест по времени, как пишет его приложение: X — мс от начала, Y — %;
// «Задание» ступеньками по steps (по holdMs на ступеньку), «Датчик
// линейных перемещений» догоняет его с запаздыванием tauMs.
ChartSnapshot timeTest(const QVector<double>& steps, double holdMs, double tauMs)
{
    ChartSeries task{QObject::tr("Задание"), QColor(0, 0, 0), {}};
    ChartSeries sensor{QObject::tr("Датчик линейных перемещений"), QColor(255, 0, 0), {}};
    double position = steps.value(0);
    double t = 0.0;
    constexpr double dtMs = 50.0;
    for (double target : steps) {
        for (double elapsed = 0.0; elapsed < holdMs; elapsed += dtMs, t += dtMs) {
            position += (target - position) * (1.0 - qExp(-dtMs / tauMs));
            task.points.append({t, target});
            sensor.points.append({t, position + 0.05 * qSin(t / 170.0)});
        }
    }
    return ChartSnapshot{{task, sensor}};
}

struct Session {
    Telemetry telemetry;
    ObjectInfo object;
    ValveInfo valve;
    MaterialsOfComponentParts materials;
    OtherParameters params;
    ChartImageStorage chartImages;
    ChartSnapshotStorage chartData;
    QMap<Report::Html::ConclusionField, QString> conclusionAnswers;
    QString conclusionSummary;
    QMap<Report::Html::CommentSlot, QString> testComments;
    QStringList spareParts;
    Domain::Tests::PatternType pattern = Domain::Tests::PatternType::None;
    Report::EditChoices editChoices;

    bool save(const QString& path, Domain::Variant variant) const
    {
        const Report::Context ctx{
            telemetry, object, valve, materials, params, chartImages, chartData,
            conclusionAnswers, conclusionSummary, testComments, spareParts,
            pattern, editChoices,
        };
        const bool ok = Report::Excel::generate(ctx, variant, path);
        qInfo().noquote() << (ok ? "OK  " : "FAIL") << path;
        return ok;
    }
};

void fillFull(Session& s)
{
    s.object.object = "Установка первичной переработки нефти ЭЛОУ-АВТ-6, блок атмосферной перегонки";
    s.object.manufactory = "Цех №3";
    s.object.department = "Компрессорная станция";
    s.object.FIO = "Иванов И. И.";

    s.valve.positionNumber = "12-FV-034";
    s.valve.manufacturer = "Valtek";
    s.valve.valveModel = "V200";
    s.valve.serialNumber = "SN-2024-1187";
    s.valve.DN = "DN50";
    s.valve.PN = "PN40";
    s.valve.positionerModel = "SVI II AP";
    s.valve.dinamicErrorRecomend = "2.0";
    s.valve.valveStroke = "25.0";
    s.valve.driveModel = "39-71361";
    s.valve.driveRangeLow = 0.4;
    s.valve.driveRangeHigh = 1.2;
    s.valve.driveType = DriveType::SpringDiaphragm;

    s.materials.corpus = "Сталь 20";
    s.materials.cap = "Сталь 20";
    s.materials.saddle = "12Х18Н10Т";
    s.materials.CV = "12.4/15.5";
    s.materials.ball = "-";
    s.materials.disk = "-";
    s.materials.plunger = "12Х18Н10Т";
    s.materials.shaft = "-";
    s.materials.stock = "14Х17Н2";
    s.materials.guideSleeve = "PTFE";

    s.params.date = "04.11.2026";
    s.params.safePosition = "НО";
    s.params.strokeMovement = "Линейное";

    auto& t = s.telemetry;
    t.valveStrokeRecord.real = 24.8;
    t.supplyRecord.pressure_bar = 4.5;
    t.crossingStatus.dynamicError = CrossingStatus::State::Ok;
    t.crossingStatus.valveStroke = CrossingStatus::State::Fail;
    t.crossingStatus.spring = CrossingStatus::State::Ok;
    t.crossingStatus.frictionPercent = CrossingStatus::State::Fail;

    Domain::Tests::Main::Result main;
    main.dynamicErrorReal = 1.2;
    main.lowLimitPressure = 0.6;
    main.highLimitPressure = 3.8;
    main.springLow = 0.41;
    main.springHigh = 1.18;
    main.frictionPercent = 14.1;
    main.frictionForce = 186.0;
    t.testMain = main;

    Domain::Tests::Stroke::Result stroke;
    stroke.forwardTimeMs = 3200;
    stroke.backwardTimeMs = 3400;
    t.testStroke = stroke;

    Domain::Tests::Option::Step::Result step;
    step.testValue = 86;
    const int levels[] = {0, 25, 50, 75, 100, 75, 50, 25, 0};
    for (int i = 0; i + 1 < int(std::size(levels)); ++i) {
        Domain::Tests::Option::Step::StepResult r;
        r.from = quint16(levels[i]);
        r.to = quint16(levels[i + 1]);
        r.T_value = i == 7 ? 0 : quint32(1800 + 120 * i);
        r.overshoot = 0.35 * i;
        step.steps << r;
    }
    t.testStep = step;

    using Domain::Tests::Option::Response::allowedErrorForStep;
    Domain::Tests::Option::Response::Result response;
    response.baseTask = 50;
    response.baseActual = 50.1;
    response.baseActualTwo = 49.9;
    response.gis = 0.35;
    const double steps[] = {5, 2, 1, 0.5};
    double up = response.baseTask;
    double down = response.baseTask;
    for (int i = 0; i < 4; ++i) {
        up += steps[i];
        down -= steps[i];
        const double err = 3.0 + 9.0 * i;
        response.upward << Domain::Tests::Option::Response::ResultItem{
            up, up - steps[i] * err / 100, err, allowedErrorForStep(i), err <= allowedErrorForStep(i)};
        response.downward << Domain::Tests::Option::Response::ResultItem{
            down, down + steps[i] * err / 100, err, allowedErrorForStep(i), i != 3};
    }
    t.testResponse = response;

    Domain::Tests::Option::Resolution::Result resolution;
    resolution.baseTask = 50;
    resolution.baseActual = 50.05;
    namespace Reso = Domain::Tests::Option::Resolution;
    for (int i = 1; i <= 4; ++i) {
        const double upErr = 8.0 * i, downErr = 12.0 * i;
        const double allowed = Reso::allowedErrorForStep(i - 1);
        resolution.upward << Reso::ResultItem{
            50 + 0.25 * i, 50 + 0.25 * i - 0.02 * i, upErr, allowed, upErr <= allowed};
        resolution.downward << Reso::ResultItem{
            50 - 0.25 * i, 50 - 0.25 * i + 0.03 * i, downErr, allowed, downErr <= allowed};
    }
    t.testResolution = resolution;

    // Регулировочный цикл: 100 циклов по 5 точкам, почти 2 часа — проверка
    // часов в "Общее время теста". По два замера на диапазон — проверка
    // выбора максимума/минимума.
    Domain::Tests::Cyclic::Regulatory::Result regulatory;
    regulatory.numCycles = 100;
    regulatory.sequence = "0-25-50-75-100";
    regulatory.totalTimeSec = 5 * 100 * 14.0;
    for (const qint16 percent : {0, 25, 50, 75, 100}) {
        for (int k = 0; k < 2; ++k) {
            Domain::Tests::Cyclic::Regulatory::Range r;
            r.rangePercent = percent;
            r.maxForwardPosition = percent + 0.3 + 0.2 * k;
            r.maxForwardCycle = 10 + 30 * k + percent / 25;
            r.maxBackwardPosition = percent - 0.4 - 0.1 * k;
            r.maxBackwardCycle = 70 - 20 * k + percent / 25;
            regulatory.ranges << r;
        }
    }
    t.testCyclicRegulatory = regulatory;

    Domain::Tests::Cyclic::Shutoff::Result shutoff;
    shutoff.numCycles = 50;
    shutoff.sequence = "0-100";
    shutoff.totalTimeSec = 2 * 50 * 20.0;
    shutoff.doOnCounts = {50, 49};
    shutoff.doOffCounts = {50, 50};
    shutoff.switch3to0Count = 50;
    shutoff.switch0to3Count = 1;
    t.testCyclicShutoff = shutoff;

    using Widgets::Chart::ChartType;
    s.chartImages.set(ChartType::Task, placeholderChart("Задание", QColor("#2A689F")));
    s.chartImages.set(ChartType::Pressure, placeholderChart("Давление", QColor("#D62828")));
    s.chartImages.set(ChartType::Friction, placeholderChart("Трение", QColor("#457448")));
    s.chartImages.set(ChartType::Step, placeholderChart("Шаговая реакция", QColor("#7B4FA0")));
    s.chartImages.set(ChartType::Response, placeholderChart("Чувствительность", QColor("#C77700")));
    s.chartImages.set(ChartType::Resolution, placeholderChart("Разрешающая способность", QColor("#00838F")));
    // Точки — для настоящих графиков Excel (картинки выше остаются запасом).
    s.chartData.set(ChartType::Step, timeTest({0, 10, 25, 50, 75, 90, 100, 90, 75, 50, 25, 10, 0}, 8000, 900));
    s.chartData.set(ChartType::Response, timeTest({50, 50.5, 51, 50.5, 50, 49.5, 49, 49.5, 50}, 6000, 600));
    s.chartData.set(ChartType::Resolution, timeTest({50, 50.25, 50.5, 50.75, 51, 50.75, 50.5, 50.25, 50}, 6000, 600));

    using Report::Html::ConclusionField;
    s.conclusionAnswers = {
        {ConclusionField::SeatingZone, "без видимых дефектов"},
        {ConclusionField::PositionerAdaptation, "удовлетворительно"},
        {ConclusionField::AuxiliaryEquipment,
         "перечень неисправного навесного оборудования указан в \"Рекомендуемый ЗИП\""},
        {ConclusionField::FrictionGraph, "высокое трение при выходе плунжера/диска из седла"},
        {ConclusionField::GlandLeak, "видимая течь не наблюдается"},
    };
    s.conclusionSummary =
        "Работа позиционера неудовлетворительная. Произвести ремонт/замену позиционера/запчастей "
        "позиционера, калибровку позиционера. Работа механической части неудовлетворительная. "
        "Произвести ревизию клапана: выполнить разборку, чистку, мойку внутренних деталей, "
        "проверка рабочей поверхности плунжер/седло/шар, проверка штока/вала (при необходимости "
        "заменить). Произвести замену сальниковой набивки, уплотнений. Произвести ревизию привода. "
        "После ремонта произвести настройку позиционера.";
    // Образцы из листов "Заключение"/"ЗИП" старого Report.xlsx — в
    // приложении они приходят из комбобоксов MainWindow.ui.
    s.editChoices.conclusionFields = {
        {ConclusionField::SeatingZone, {"без видимых дефектов", "резкий отрыв плунжера от седла",
                                        "низкое значение зоны посадки плунжер/седло"}},
        {ConclusionField::PositionerAdaptation, {"удовлетворительно", "люфты адаптации/обратной связи"}},
        {ConclusionField::AuxiliaryEquipment, {"удовлетворительно", "позиционер неработоспособен"}},
        {ConclusionField::FrictionGraph, {"зоны повышенного трения отсутствуют",
                                          "высокое трение при выходе плунжера/диска из седла"}},
        {ConclusionField::GlandLeak, {"видимая течь не наблюдается", "наблюдается видимая течь"}},
    };
    s.editChoices.spareParts = {
        "сальниковое уплотнение (согласно исполнению)", "плунжер (при необходимости)",
        "диск/уплотнение диска", "комплект мягких уплотнений", "мембрана привода", "манометр",
    };

    s.spareParts = {
        "сальниковое уплотнение (согласно исполнению)",
        "комплект Metso модель ZX (cальниковое уплотнение (согласно исполнению), уплотнения седла)",
        "диск/уплотнение диска",
        "кольцо внутреннее/внешнее втулки сальника (для Камфлекс)",
    };
}

}

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    const QString outDir = QStringLiteral(MPIPLATFORM_SOURCE_DIR "/experiments/report-generator/out");
    QDir().mkpath(outDir);

    using Domain::Variant;

    Session full;
    fillFull(full);

    bool ok = full.save(outDir + "/full.xlsx", Variant::Ilim);
    ok &= Session().save(outDir + "/minimal.xlsx", Variant::Standard);

    const std::pair<Domain::Tests::PatternType, const char*> skiPatterns[] = {
        {Domain::Tests::PatternType::B_CVT, "B_CVT"},
        {Domain::Tests::PatternType::B_SACVT, "B_SACVT"},
        {Domain::Tests::PatternType::C_CVT, "C_CVT"},
        {Domain::Tests::PatternType::C_SACVT, "C_SACVT"},
        {Domain::Tests::PatternType::C_SOVT, "C_SOVT"},
    };
    for (const auto& [pattern, name] : skiPatterns) {
        Session ski = full;
        ski.pattern = pattern;
        ok &= ski.save(outDir + "/ski_" + name + ".xlsx", Variant::Ski);
    }

    Session skiEmpty;
    skiEmpty.pattern = Domain::Tests::PatternType::B_CVT;
    ok &= skiEmpty.save(outDir + "/ski_B_CVT_empty.xlsx", Variant::Ski);

    return ok ? 0 : 1;
}
