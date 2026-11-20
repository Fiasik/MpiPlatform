#include "Utils/NumberUtils.h"
#include "SessionReportLoader.h"

#include <QDir>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

#include <type_traits>

#include "Domain/DeviceConfig.h"
#include "Gui/MainWindow/ArchivedTest.h"
#include "Gui/Setup/SelectTests.h"
#include "Gui/Setup/ValveWindow/Logic.h"
#include "Report/Html/ConclusionField.h"
#include "Storage/AppSettings.h"
#include "Storage/FieldOverrides.h"
#include "Storage/HistoryDb.h"
#include "Storage/SessionFolders.h"
#include "Storage/ValveSnapshotMapper.h"
#include "Widgets/Chart/ChartType.h"

using Widgets::Chart::ChartType;

namespace ReportCompare {

namespace {

const QString kConnection = QStringLiteral("mpiplatform_report_compare");

// Папка сессии без диалогов: как в базе (от папки приложения), иначе — от
// корня архива, который оператор уже указал для «Открыть сессию»
// (AppSettings «History/archiveRoot»).
QString resolveFolder(const QString& stored)
{
    if (stored.isEmpty())
        return QString();
    const QString direct = SessionFolders::resolve(stored, SessionFolders::appRoot());
    if (ArchivedTest::pickRun(direct) > 0)
        return direct;
    if (QDir::isAbsolutePath(stored) || !SessionFolders::isSafeRelative(stored))
        return direct;
    const QString root = AppSettings().historyArchiveRoot();
    return root.isEmpty() ? direct : SessionFolders::resolve(stored, root);
}

// Как MainWindow::setRegistry(): допуски и параметры — из карточки сессии.

Domain::Tests::PatternType patternFromKey(const QString& key)
{
    for (auto p : {Domain::Tests::PatternType::C_SOVT, Domain::Tests::PatternType::B_SACVT, Domain::Tests::PatternType::C_SACVT,
                   Domain::Tests::PatternType::B_CVT, Domain::Tests::PatternType::C_CVT}) {
        if (SelectTests::patternKey(p) == key)
            return p;
    }
    return Domain::Tests::PatternType::None;
}

// Всё из базы, кроме графиков. false — сессии нет.
struct DbPart {
    ReportGenerationWorker::Input input;
    double measuredStroke = qQNaN();
    QMap<QString, double> overrides; // FieldOverrides::k… -> исправленное оператором
};

bool readDb(QSqlDatabase& db, qint64 sessionId, DbPart& out)
{
    ReportGenerationWorker::Input& in = out.input;

    QSqlQuery s(db);
    s.prepare(QStringLiteral("SELECT * FROM sessions WHERE id = ?"));
    s.addBindValue(sessionId);
    if (!s.exec() || !s.next())
        return false;

    in.pattern = patternFromKey(s.value(QStringLiteral("pattern")).toString());
    in.params.date = QDateTime::fromString(s.value(QStringLiteral("ts")).toString(), Qt::ISODate)
                         .toString(QStringLiteral("dd.MM.yyyy"));
    // Пустое поле в живом отчёте печатается как 0.00 (collectInput()), NULL — так же.
    const QVariant supply = s.value(QStringLiteral("supply_pressure_bar"));
    in.telemetry.supplyRecord.pressure_bar = supply.isNull() ? qQNaN() : supply.toDouble();
    const qint64 positionId = s.value(QStringLiteral("position_id")).toLongLong();

    QSqlQuery pos(db);
    pos.prepare(QStringLiteral("SELECT p.position_number, si.object, si.manufactory, si.department "
                               "FROM positions p JOIN sites si ON si.id = p.site_id WHERE p.id = ?"));
    pos.addBindValue(positionId);
    if (pos.exec() && pos.next()) {
        in.valve.positionNumber = pos.value(0).toString();
        in.object.object = pos.value(1).toString();
        in.object.manufactory = pos.value(2).toString();
        in.object.department = pos.value(3).toString();
    }

    QSqlQuery v(db);
    v.prepare(QStringLiteral("SELECT * FROM valve_snapshots WHERE session_id = ?"));
    v.addBindValue(sessionId);
    if (v.exec() && v.next()) {
        const QString position = in.valve.positionNumber;
        ValveSnapshotMapper::read(v, in.valve);
        in.valve.positionNumber = position;
    }
    in.params.safePosition = Logic::toString(in.valve.safePosition);
    in.params.strokeMovement = Logic::toString(in.valve.strokeMovement);

    QSqlQuery m(db);
    m.prepare(QStringLiteral("SELECT * FROM valve_snapshot_materials WHERE session_id = ?"));
    m.addBindValue(sessionId);
    if (m.exec() && m.next()) {
        const auto text = [&m](const char* column) { return m.value(QLatin1String(column)).toString(); };
        in.materials.plunger = text("plunger");
        in.materials.saddle = text("saddle");
        in.materials.cap = text("cap");
        in.materials.corpus = text("corpus");
        in.materials.ball = text("ball");
        in.materials.disk = text("disk");
        in.materials.shaft = text("shaft");
        in.materials.stock = text("stock");
        in.materials.guideSleeve = text("guide_sleeve");
        in.materials.stuffingBoxSeal = text("stuffing_box_seal");
        in.materials.CV = text("cv");
    }

    QSqlQuery c(db);
    c.prepare(QStringLiteral("SELECT valve_stroke_value FROM crossing_results WHERE session_id = ?"));
    c.addBindValue(sessionId);
    if (c.exec() && c.next() && !c.value(0).isNull())
        out.measuredStroke = c.value(0).toDouble();

    QSqlQuery st(db);
    st.prepare(QStringLiteral("SELECT forward_time_ms, backward_time_ms, forward_signal_delay_ms, "
                              "backward_signal_delay_ms FROM stroke_results WHERE session_id = ?"));
    st.addBindValue(sessionId);
    if (st.exec() && st.next()) {
        Domain::Tests::Stroke::Result r;
        r.forwardTimeMs = st.value(0).toULongLong();
        r.backwardTimeMs = st.value(1).toULongLong();
        r.forwardSignalDelayMs = st.value(2).toULongLong();
        r.backwardSignalDelayMs = st.value(3).toULongLong();
        in.telemetry.testStroke = r;
    }

    using Report::Html::ConclusionField;
    QSqlQuery cn(db);
    cn.prepare(QStringLiteral("SELECT seating_zone_key, positioner_adaptation_key, auxiliary_equipment_key, "
                              "friction_graph_key, gland_leak_key, additional_notes "
                              "FROM conclusions WHERE session_id = ?"));
    cn.addBindValue(sessionId);
    if (cn.exec() && cn.next()) {
        const ConclusionField fields[] = {ConclusionField::SeatingZone, ConclusionField::PositionerAdaptation,
                                          ConclusionField::AuxiliaryEquipment, ConclusionField::FrictionGraph,
                                          ConclusionField::GlandLeak};
        for (int i = 0; i < 5; ++i) {
            const QString answer = cn.value(i).toString();
            if (!answer.isEmpty())
                in.conclusionAnswers.insert(fields[i], answer);
        }
        in.conclusionSummary = cn.value(5).toString();
    }

    QSqlQuery sp(db);
    sp.prepare(QStringLiteral("SELECT part_code, is_free_text, free_text FROM session_spare_parts "
                              "WHERE session_id = ? ORDER BY id"));
    sp.addBindValue(sessionId);
    if (sp.exec()) {
        while (sp.next())
            in.spareParts << (sp.value(1).toInt() != 0 ? sp.value(2).toString() : sp.value(0).toString());
    }

    QSqlQuery fo(db);
    fo.prepare(QStringLiteral("SELECT field_name, corrected_value FROM field_overrides WHERE session_id = ?"));
    fo.addBindValue(sessionId);
    if (fo.exec()) {
        while (fo.next()) {
            if (!fo.value(1).isNull())
                out.overrides.insert(fo.value(0).toString(), fo.value(1).toDouble());
        }
    }
    return true;
}

// Правки оператора перед печатью (field_overrides) — поверх посчитанного,
// как ReportGenerationPresenter::collectReportOverrides(): в выданном акте
// стояли именно они. Pass|Fail при этом не пересчитывается — как и там.
void applyOverrides(const QMap<QString, double>& overrides, Telemetry& t)
{
    using namespace FieldOverrides;
    const auto apply = [&overrides](const QString& key, auto& target) {
        const auto it = overrides.constFind(key);
        if (it != overrides.constEnd())
            target = static_cast<std::remove_reference_t<decltype(target)>>(*it);
    };
    if (t.testMain) {
        apply(kFrictionForce, t.testMain->frictionForce);
        apply(kFrictionPercent, t.testMain->frictionPercent);
        apply(kDynamicErrorReal, t.testMain->dynamicErrorReal);
        apply(kPressureLow, t.testMain->lowLimitPressure);
        apply(kPressureHigh, t.testMain->highLimitPressure);
        apply(kSpringLow, t.testMain->springLow);
        apply(kSpringHigh, t.testMain->springHigh);
    }
    apply(kValveStrokeReal, t.valveStrokeRecord.real);
    if (t.testStroke) {
        apply(kStrokeForwardMs, t.testStroke->forwardTimeMs);
        apply(kStrokeBackwardMs, t.testStroke->backwardTimeMs);
    }
}

}

Domain::DeviceConfig deviceConfig(const ValveInfo& valve)
{
    Domain::DeviceConfig cfg;
    cfg.safePosition = valve.safePosition;
    cfg.positionerScale = valve.positionerScale;
    cfg.driveDiameter = valve.driveDiameter;
    cfg.valveStroke = valve.valveStroke;
    cfg.crossingLimits = valve.crossingLimits;
    cfg.strokeMovement = valve.strokeMovement;
    cfg.diameterPulley = valve.diameterPulley;
    cfg.dinamicErrorRecomend = NumberUtils::toDoubleOrNaN(valve.dinamicErrorRecomend);
    cfg.driveRangeLow = valve.driveRangeLow;
    cfg.driveRangeHigh = valve.driveRangeHigh;
    return cfg;
}

QVector<PastSession> pastSessions(const ObjectInfo& object, const QString& positionNumber,
                                  qint64 excludeId, int* skipped)
{
    struct Row { qint64 id; QDateTime ts; QString stored; };
    QVector<Row> rows;
    HistoryDb::withConnection(kConnection, [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        // legacy — карточки до перехода на базу, тестов у них нет.
        q.prepare(QStringLiteral(
            "SELECT s.id, s.ts, s.folder_path FROM sessions s "
            "JOIN positions p ON p.id = s.position_id "
            "JOIN sites si ON si.id = p.site_id "
            "WHERE si.object = ? AND si.manufactory = ? AND si.department = ? "
            "AND p.position_number = ? AND s.source <> 'legacy' "
            "ORDER BY s.ts DESC"));
        q.addBindValue(object.object);
        q.addBindValue(object.manufactory);
        q.addBindValue(object.department);
        q.addBindValue(positionNumber);
        if (!q.exec())
            return;
        while (q.next()) {
            rows.push_back({q.value(0).toLongLong(),
                            QDateTime::fromString(q.value(1).toString(), Qt::ISODate),
                            q.value(2).toString()});
        }
    });

    QVector<PastSession> out;
    int dropped = 0;
    for (const Row& row : std::as_const(rows)) {
        if (row.id == excludeId)
            continue;
        PastSession s;
        s.id = row.id;
        s.ts = row.ts;
        s.folder = resolveFolder(row.stored);
        s.run = s.folder.isEmpty() ? 0 : ArchivedTest::pickRun(s.folder);
        if (s.run > 0)
            out.push_back(s);
        else
            ++dropped;
    }
    if (skipped)
        *skipped = dropped;
    return out;
}

std::optional<SessionCard> loadCard(qint64 sessionId)
{
    DbPart part;
    SessionCard card;
    bool found = false;
    const bool opened = HistoryDb::withConnection(kConnection, [&](QSqlDatabase& db) {
        found = readDb(db, sessionId, part);
        if (!found)
            return;
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT test_value FROM step_results WHERE session_id = ?"));
        q.addBindValue(sessionId);
        if (q.exec() && q.next() && !q.value(0).isNull())
            card.stepTestValue = q.value(0).toDouble();
    });
    if (!opened || !found)
        return std::nullopt;
    card.valve = std::move(part.input.valve);
    return card;
}

std::optional<ReportGenerationWorker::Input> loadInput(const PastSession& session, QString* error)
{
    const auto fail = [error](const QString& text) {
        if (error)
            *error = text;
        return std::optional<ReportGenerationWorker::Input>();
    };

    DbPart part;
    bool found = false;
    const bool opened = HistoryDb::withConnection(kConnection, [&](QSqlDatabase& db) {
        found = readDb(db, session.id, part);
    });
    if (!opened)
        return fail(QObject::tr("База недоступна"));
    if (!found)
        return fail(QObject::tr("Сессии нет в базе"));

    const auto task = ArchivedTest::readDataFile(ArchivedTest::chartFile(session.folder, ChartType::Task, session.run));
    const auto pressure = ArchivedTest::readDataFile(ArchivedTest::chartFile(session.folder, ChartType::Pressure, session.run));
    if (!task || !pressure)
        return fail(QObject::tr("Не прочитать графики в %1").arg(QDir::toNativeSeparators(session.folder)));

    ReportGenerationWorker::Input in = std::move(part.input);
    const auto main = ArchivedTest::analyze(*task, *pressure, in.valve.driveDiameter);
    if (!main)
        return fail(QObject::tr("В графиках сессии нет обоих ходов — акт не собрать"));

    in.chartData.set(ChartType::Task, ArchivedTest::toSnapshot(*task));
    in.chartData.set(ChartType::Pressure, ArchivedTest::toSnapshot(*pressure));
    const QString frictionPath = ArchivedTest::chartFile(session.folder, ChartType::Friction, session.run);
    if (!frictionPath.isEmpty()) {
        if (const auto friction = ArchivedTest::readDataFile(frictionPath))
            in.chartData.set(ChartType::Friction, ArchivedTest::toSnapshot(*friction));
    }

    in.telemetry.testMain = *main;
    in.telemetry.valveStrokeRecord.real = part.measuredStroke;
    in.telemetry.crossingStatus = ArchivedTest::crossingStatus(*main, deviceConfig(in.valve), part.measuredStroke);
    applyOverrides(part.overrides, in.telemetry);
    in.buildTechnicalInspectionPdf = true;
    return in;
}

}
