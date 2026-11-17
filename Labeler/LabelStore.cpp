#include "LabelStore.h"

#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include <algorithm>

#include "LabelSchema.h"

namespace Labeler {

QString nowUtcIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

namespace {

// Сколько первых карточек идут в чисто случайном порядке (честная частота
// дефектов), какая доля повторов и через сколько карточек повтор.
constexpr int kRandomHead = 100;
constexpr int kRepeatPercent = 10;
constexpr int kRepeatMinGap = 30;
constexpr int kRepeatMaxGap = 60;

QString chartsToJson(const QMap<QString, QStringList>& charts)
{
    QJsonObject o;
    for (auto it = charts.cbegin(); it != charts.cend(); ++it)
        o.insert(it.key(), QJsonArray::fromStringList(it.value()));
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

QMap<QString, QStringList> chartsFromJson(const QString& json)
{
    QMap<QString, QStringList> charts;
    const QJsonObject o = QJsonDocument::fromJson(json.toUtf8()).object();
    for (auto it = o.begin(); it != o.end(); ++it) {
        QStringList files;
        for (const QJsonValue& v : it.value().toArray())
            files << v.toString();
        charts.insert(it.key(), files);
    }
    return charts;
}

QString labelsToJson(const QStringList& labels)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(labels)).toJson(QJsonDocument::Compact));
}

QStringList labelsFromJson(const QString& json)
{
    QStringList labels;
    for (const QJsonValue& v : QJsonDocument::fromJson(json.toUtf8()).array())
        labels << v.toString();
    return labels;
}

ValveCard cardFromColumn(const QVariant& value)
{
    if (value.isNull())
        return {};
    return ValveCard::fromJson(QJsonDocument::fromJson(value.toString().toUtf8()).object());
}

QString csvField(const QString& value)
{
    if (!value.contains(QLatin1Char(';')) && !value.contains(QLatin1Char('"'))
        && !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r')))
        return value;
    QString escaped = value;
    escaped.replace(QStringLiteral("\""), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

}

LabelStore::LabelStore()
    : m_connectionName(QStringLiteral("labeler-") + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

LabelStore::~LabelStore()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool LabelStore::exec(const QString& sql)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    if (!q.exec(sql)) {
        m_lastError = q.lastError().text();
        return false;
    }
    return true;
}

bool LabelStore::open(const QString& path, QString* error)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(path);
    if (!db.open()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }
    m_path = path;

    const QStringList schema = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS label_sets("
                       " version INTEGER PRIMARY KEY, json TEXT NOT NULL, loaded_at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS datasets("
                       " name TEXT PRIMARY KEY, root_path TEXT NOT NULL, scanned_at TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS sessions("
                       " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       " dataset TEXT NOT NULL,"
                       " rel_path TEXT NOT NULL,"
                       " files_hash TEXT NOT NULL,"
                       " charts TEXT NOT NULL,"
                       " valve_card_file TEXT,"
                       " valve_card TEXT,"
                       " has_report INTEGER NOT NULL DEFAULT 0,"
                       " present INTEGER NOT NULL DEFAULT 1,"
                       " UNIQUE(dataset, rel_path))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS queue("
                       " dataset TEXT NOT NULL, pos INTEGER NOT NULL,"
                       " session_id INTEGER NOT NULL, is_repeat INTEGER NOT NULL DEFAULT 0,"
                       " PRIMARY KEY(dataset, pos))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS answers("
                       " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       " session_id INTEGER NOT NULL,"
                       " queue_pos INTEGER NOT NULL,"
                       " is_repeat INTEGER NOT NULL DEFAULT 0,"
                       " verdict TEXT NOT NULL,"
                       " labels TEXT NOT NULL,"
                       " comment TEXT NOT NULL DEFAULT '',"
                       " label_set_version INTEGER NOT NULL,"
                       " files_hash TEXT NOT NULL,"
                       " shown_at TEXT NOT NULL,"
                       " answered_at TEXT NOT NULL,"
                       " seconds_on_card REAL NOT NULL,"
                       " revoked_at TEXT)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS answers_session ON answers(session_id)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS queue_session ON queue(session_id)"),
    };
    for (const QString& sql : schema) {
        if (!exec(sql)) {
            if (error)
                *error = m_lastError;
            return false;
        }
    }

    if (meta(QStringLiteral("schema_version")).isEmpty())
        setMeta(QStringLiteral("schema_version"), QStringLiteral("1"));
    return true;
}

QString LabelStore::meta(const QString& key) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT value FROM meta WHERE key = ?"));
    q.addBindValue(key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return {};
}

void LabelStore::setMeta(const QString& key, const QString& value)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("INSERT INTO meta(key, value) VALUES(?, ?)"
                             " ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.addBindValue(key);
    q.addBindValue(value);
    if (!q.exec())
        m_lastError = q.lastError().text();
}

bool LabelStore::registerLabelSet(const LabelSchema& schema, QString* warning)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT json FROM label_sets WHERE version = ?"));
    q.addBindValue(schema.version());
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return false;
    }

    const QString json = QString::fromUtf8(schema.rawJson());
    if (q.next()) {
        if (q.value(0).toString() != json && warning) {
            *warning = QStringLiteral(
                "labels.json изменён, а \"version\" осталась %1. Если менялись ключи меток, "
                "поднимите version — иначе старые ответы будут читаться по новому списку.")
                .arg(schema.version());
        }
        return true;
    }

    QSqlQuery ins(QSqlDatabase::database(m_connectionName));
    ins.prepare(QStringLiteral("INSERT INTO label_sets(version, json, loaded_at) VALUES(?, ?, ?)"));
    ins.addBindValue(schema.version());
    ins.addBindValue(json);
    ins.addBindValue(nowUtcIso());
    if (!ins.exec()) {
        m_lastError = ins.lastError().text();
        return false;
    }
    return true;
}

int LabelStore::syncSessions(const QString& dataset, const QString& rootPath,
                             const QVector<SessionInfo>& sessions,
                             const QHash<QString, ValveCard>& cardsByRelPath)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();

    auto countSessions = [&]() {
        QSqlQuery c(db);
        c.prepare(QStringLiteral("SELECT COUNT(*) FROM sessions WHERE dataset = ?"));
        c.addBindValue(dataset);
        return (c.exec() && c.next()) ? c.value(0).toInt() : 0;
    };
    const int before = countSessions();

    QSqlQuery ds(db);
    ds.prepare(QStringLiteral("INSERT INTO datasets(name, root_path, scanned_at) VALUES(?, ?, ?)"
                              " ON CONFLICT(name) DO UPDATE SET root_path = excluded.root_path,"
                              " scanned_at = excluded.scanned_at"));
    ds.addBindValue(dataset);
    ds.addBindValue(rootPath);
    ds.addBindValue(nowUtcIso());
    ds.exec();

    QSqlQuery hide(db);
    hide.prepare(QStringLiteral("UPDATE sessions SET present = 0 WHERE dataset = ?"));
    hide.addBindValue(dataset);
    hide.exec();

    QSqlQuery up(db);
    up.prepare(QStringLiteral(
        "INSERT INTO sessions(dataset, rel_path, files_hash, charts, valve_card_file, valve_card,"
        " has_report, present) VALUES(?, ?, ?, ?, ?, ?, ?, 1)"
        " ON CONFLICT(dataset, rel_path) DO UPDATE SET files_hash = excluded.files_hash,"
        " charts = excluded.charts, valve_card_file = excluded.valve_card_file,"
        " valve_card = excluded.valve_card, has_report = excluded.has_report, present = 1"));

    for (const SessionInfo& s : sessions) {
        up.addBindValue(dataset);
        up.addBindValue(s.relPath);
        up.addBindValue(s.filesHash);
        up.addBindValue(chartsToJson(s.charts));
        up.addBindValue(s.valveCardFile.isEmpty() ? QVariant() : QVariant(s.valveCardFile));
        const auto card = cardsByRelPath.constFind(s.relPath);
        up.addBindValue(card == cardsByRelPath.cend()
            ? QVariant()
            : QVariant(QString::fromUtf8(QJsonDocument(card->toJson()).toJson(QJsonDocument::Compact))));
        up.addBindValue(s.hasReport ? 1 : 0);
        if (!up.exec())
            m_lastError = up.lastError().text();
    }

    db.commit();
    return countSessions() - before;
}

QString LabelStore::rootPath(const QString& dataset) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT root_path FROM datasets WHERE name = ?"));
    q.addBindValue(dataset);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return {};
}

QVector<SessionRecord> LabelStore::sessions(const QString& dataset) const
{
    QVector<SessionRecord> result;
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT id, rel_path, files_hash, charts, valve_card_file, valve_card, has_report"
                             " FROM sessions WHERE dataset = ? AND present = 1 ORDER BY rel_path"));
    q.addBindValue(dataset);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return result;
    }
    while (q.next()) {
        SessionRecord r;
        r.id = q.value(0).toLongLong();
        r.dataset = dataset;
        r.relPath = q.value(1).toString();
        r.filesHash = q.value(2).toString();
        r.charts = chartsFromJson(q.value(3).toString());
        r.valveCardFile = q.value(4).toString();
        r.card = cardFromColumn(q.value(5));
        r.hasReport = q.value(6).toInt() != 0;
        result << r;
    }
    return result;
}

int LabelStore::queueSize(const QString& dataset) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT COUNT(*) FROM queue WHERE dataset = ?"));
    q.addBindValue(dataset);
    return (q.exec() && q.next()) ? q.value(0).toInt() : 0;
}

int LabelStore::ensureQueue(const QString& dataset)
{
    if (queueSize(dataset) == 0)
        buildQueue(dataset);
    else
        appendNewSessionsToQueue(dataset);
    return queueSize(dataset);
}

void LabelStore::buildQueue(const QString& dataset)
{
    // Seed сохраняется: при сбое записи очередь можно восстановить той же.
    const QString seedKey = QStringLiteral("seed:") + dataset;
    QString seedText = meta(seedKey);
    if (seedText.isEmpty()) {
        seedText = QString::number(QRandomGenerator::global()->generate());
        setMeta(seedKey, seedText);
    }
    QRandomGenerator rng(seedText.toUInt());

    QVector<qint64> ids;
    QSet<qint64> flagged;
    for (const SessionRecord& s : sessions(dataset)) {
        ids << s.id;
        if (s.card.ok && s.card.operatorRemark())
            flagged.insert(s.id);
    }
    if (ids.isEmpty())
        return;

    std::shuffle(ids.begin(), ids.end(), rng);

    // Голова — чисто случайно; хвост — случайно, но сессии с замечанием
    // оператора идут чаще (~каждая третья, пока не кончатся), чтобы редкие
    // дефекты не утонули в ~80 % нормы. Позиция внутри хвоста случайная —
    // по месту в очереди не угадать, есть ли у карточки замечание.
    QVector<qint64> order = ids.mid(0, kRandomHead);
    QVector<qint64> restFlagged;
    QVector<qint64> restOther;
    for (int i = kRandomHead; i < ids.size(); ++i)
        (flagged.contains(ids.at(i)) ? restFlagged : restOther) << ids.at(i);

    int fi = 0;
    int oi = 0;
    while (fi < restFlagged.size() && oi < restOther.size())
        order << (rng.bounded(3) == 0 ? restFlagged.at(fi++) : restOther.at(oi++));
    while (fi < restFlagged.size())
        order << restFlagged.at(fi++);
    while (oi < restOther.size())
        order << restOther.at(oi++);

    // Повторы для проверки согласованности: ответ на повтор пишется отдельно
    // и не заменяет основной.
    QMap<int, QVector<qint64>> repeatsAfter;
    for (int i = 0; i < order.size(); ++i) {
        if (int(rng.bounded(100)) < kRepeatPercent) {
            const int target = i + kRepeatMinGap + int(rng.bounded(kRepeatMaxGap - kRepeatMinGap + 1));
            repeatsAfter[qMin(target, int(order.size()) - 1)] << order.at(i);
        }
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    db.transaction();
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral("INSERT INTO queue(dataset, pos, session_id, is_repeat) VALUES(?, ?, ?, ?)"));
    int pos = 0;
    auto push = [&](qint64 id, bool repeat) {
        ins.addBindValue(dataset);
        ins.addBindValue(pos++);
        ins.addBindValue(id);
        ins.addBindValue(repeat ? 1 : 0);
        if (!ins.exec())
            m_lastError = ins.lastError().text();
    };
    for (int i = 0; i < order.size(); ++i) {
        push(order.at(i), false);
        for (qint64 id : repeatsAfter.value(i))
            push(id, true);
    }
    db.commit();
}

void LabelStore::appendNewSessionsToQueue(const QString& dataset)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT s.id FROM sessions s WHERE s.dataset = ? AND s.present = 1 AND NOT EXISTS"
        " (SELECT 1 FROM queue q WHERE q.dataset = s.dataset AND q.session_id = s.id)"));
    q.addBindValue(dataset);
    if (!q.exec())
        return;

    QVector<qint64> ids;
    while (q.next())
        ids << q.value(0).toLongLong();
    if (ids.isEmpty())
        return;
    std::shuffle(ids.begin(), ids.end(), *QRandomGenerator::global());

    int pos = queueSize(dataset);
    db.transaction();
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral("INSERT INTO queue(dataset, pos, session_id, is_repeat) VALUES(?, ?, ?, 0)"));
    for (qint64 id : ids) {
        ins.addBindValue(dataset);
        ins.addBindValue(pos++);
        ins.addBindValue(id);
        ins.exec();
    }
    db.commit();
}

std::optional<QueueItem> LabelStore::queueItem(const QString& dataset, int pos) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT session_id, is_repeat FROM queue WHERE dataset = ? AND pos = ?"));
    q.addBindValue(dataset);
    q.addBindValue(pos);
    if (!q.exec() || !q.next())
        return std::nullopt;
    QueueItem item;
    item.pos = pos;
    item.sessionId = q.value(0).toLongLong();
    item.isRepeat = q.value(1).toInt() != 0;
    return item;
}

void LabelStore::requeueAtEnd(const QString& dataset, qint64 sessionId, int afterPos)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT 1 FROM queue WHERE dataset = ? AND session_id = ?"
                             " AND is_repeat = 0 AND pos > ?"));
    q.addBindValue(dataset);
    q.addBindValue(sessionId);
    q.addBindValue(afterPos);
    if (q.exec() && q.next())
        return;

    QSqlQuery ins(QSqlDatabase::database(m_connectionName));
    ins.prepare(QStringLiteral("INSERT INTO queue(dataset, pos, session_id, is_repeat) VALUES(?, ?, ?, 0)"));
    ins.addBindValue(dataset);
    ins.addBindValue(queueSize(dataset));
    ins.addBindValue(sessionId);
    if (!ins.exec())
        m_lastError = ins.lastError().text();
}

int LabelStore::cursor(const QString& dataset) const
{
    return meta(QStringLiteral("cursor:") + dataset).toInt();
}

void LabelStore::setCursor(const QString& dataset, int pos)
{
    setMeta(QStringLiteral("cursor:") + dataset, QString::number(pos));
}

std::optional<Answer> LabelStore::answerForCard(const QueueItem& item) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    if (item.isRepeat) {
        q.prepare(QStringLiteral("SELECT id, verdict, labels, comment FROM answers"
                                 " WHERE session_id = ? AND queue_pos = ? AND is_repeat = 1"
                                 " AND revoked_at IS NULL ORDER BY id DESC LIMIT 1"));
        q.addBindValue(item.sessionId);
        q.addBindValue(item.pos);
    } else {
        q.prepare(QStringLiteral("SELECT id, verdict, labels, comment FROM answers"
                                 " WHERE session_id = ? AND is_repeat = 0 AND verdict <> ?"
                                 " AND revoked_at IS NULL ORDER BY id DESC LIMIT 1"));
        q.addBindValue(item.sessionId);
        q.addBindValue(LabelSchema::skippedVerdictKey());
    }
    if (!q.exec() || !q.next())
        return std::nullopt;

    Answer a;
    a.id = q.value(0).toLongLong();
    a.verdict = q.value(1).toString();
    a.labels = labelsFromJson(q.value(2).toString());
    a.comment = q.value(3).toString();
    return a;
}

qint64 LabelStore::addAnswer(const NewAnswer& a)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    // Пустой QString() драйвер передаёт как NULL — для NOT NULL-столбцов
    // (пустой комментарий!) это отказ записи; COALESCE превращает его в ''.
    q.prepare(QStringLiteral(
        "INSERT INTO answers(session_id, queue_pos, is_repeat, verdict, labels, comment,"
        " label_set_version, files_hash, shown_at, answered_at, seconds_on_card)"
        " VALUES(?, ?, ?, ?, ?, COALESCE(?, ''), ?, COALESCE(?, ''), COALESCE(?, ''), ?, ?)"));
    q.addBindValue(a.sessionId);
    q.addBindValue(a.queuePos);
    q.addBindValue(a.isRepeat ? 1 : 0);
    q.addBindValue(a.verdict);
    q.addBindValue(labelsToJson(a.labels));
    q.addBindValue(a.comment);
    q.addBindValue(a.labelSetVersion);
    q.addBindValue(a.filesHash);
    q.addBindValue(a.shownAtUtc);
    q.addBindValue(nowUtcIso());
    q.addBindValue(a.secondsOnCard);
    if (!q.exec()) {
        m_lastError = q.lastError().text();
        return 0;
    }
    return q.lastInsertId().toLongLong();
}

std::optional<int> LabelStore::revokeLastAnswer(const QString& dataset)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT a.id, a.queue_pos FROM answers a JOIN sessions s ON s.id = a.session_id"
                             " WHERE s.dataset = ? AND a.revoked_at IS NULL ORDER BY a.id DESC LIMIT 1"));
    q.addBindValue(dataset);
    if (!q.exec() || !q.next())
        return std::nullopt;

    const qint64 id = q.value(0).toLongLong();
    const int pos = q.value(1).toInt();

    QSqlQuery up(QSqlDatabase::database(m_connectionName));
    up.prepare(QStringLiteral("UPDATE answers SET revoked_at = ? WHERE id = ?"));
    up.addBindValue(nowUtcIso());
    up.addBindValue(id);
    if (!up.exec()) {
        m_lastError = up.lastError().text();
        return std::nullopt;
    }
    return pos;
}

Progress LabelStore::progress(const QString& dataset) const
{
    Progress p;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);

    QSqlQuery total(db);
    total.prepare(QStringLiteral("SELECT COUNT(*) FROM sessions WHERE dataset = ? AND present = 1"));
    total.addBindValue(dataset);
    if (total.exec() && total.next())
        p.totalSessions = total.value(0).toInt();

    QSqlQuery labeled(db);
    labeled.prepare(QStringLiteral(
        "SELECT COUNT(DISTINCT a.session_id) FROM answers a JOIN sessions s ON s.id = a.session_id"
        " WHERE s.dataset = ? AND s.present = 1 AND a.revoked_at IS NULL AND a.is_repeat = 0"
        " AND a.verdict <> ?"));
    labeled.addBindValue(dataset);
    labeled.addBindValue(LabelSchema::skippedVerdictKey());
    if (labeled.exec() && labeled.next())
        p.labeledSessions = labeled.value(0).toInt();

    QSqlQuery repeats(db);
    repeats.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM answers a JOIN sessions s ON s.id = a.session_id"
        " WHERE s.dataset = ? AND a.revoked_at IS NULL AND a.is_repeat = 1"));
    repeats.addBindValue(dataset);
    if (repeats.exec() && repeats.next())
        p.repeatsAnswered = repeats.value(0).toInt();

    return p;
}

bool LabelStore::exportCsv(const QString& filePath, const LabelSchema& schema, QString* error) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);

    // Актуальный ответ по сессии — последний неотменённый, не повтор и не пропуск.
    QSet<qint64> currentIds;
    QSqlQuery cur(db);
    cur.prepare(QStringLiteral("SELECT MAX(id) FROM answers WHERE revoked_at IS NULL AND is_repeat = 0"
                               " AND verdict <> ? GROUP BY session_id"));
    cur.addBindValue(LabelSchema::skippedVerdictKey());
    if (cur.exec()) {
        while (cur.next())
            currentIds.insert(cur.value(0).toLongLong());
    }

    QSqlQuery q(db);
    if (!q.exec(QStringLiteral(
            "SELECT a.id, a.is_repeat, a.verdict, a.labels, a.comment, a.label_set_version,"
            " a.files_hash, a.shown_at, a.answered_at, a.seconds_on_card,"
            " s.dataset, s.rel_path, s.files_hash, s.charts, s.valve_card, s.has_report"
            " FROM answers a JOIN sessions s ON s.id = a.session_id"
            " WHERE a.revoked_at IS NULL ORDER BY s.dataset, s.rel_path, a.id"))) {
        if (error)
            *error = q.lastError().text();
        return false;
    }

    const QStringList labelKeys = schema.labelKeys();

    QStringList header = {
        QStringLiteral("dataset"), QStringLiteral("rel_path"), QStringLiteral("answer_id"),
        QStringLiteral("is_current"), QStringLiteral("is_repeat"), QStringLiteral("verdict"),
        QStringLiteral("label_set_version"),
    };
    header << labelKeys;
    header << QStringList{
        QStringLiteral("labels_raw"), QStringLiteral("comment"), QStringLiteral("seconds_on_card"),
        QStringLiteral("shown_at_utc"), QStringLiteral("answered_at_utc"),
        QStringLiteral("files_hash"), QStringLiteral("files_changed_since"),
        QStringLiteral("charts"), QStringLiteral("has_report"),
        QStringLiteral("card_position"), QStringLiteral("card_manufacturer"), QStringLiteral("card_model"),
        QStringLiteral("card_positioner"),
        QStringLiteral("card_fail_position"), QStringLiteral("card_actuator"),
        QStringLiteral("card_supply_pressure"),
        QStringLiteral("op_seat"), QStringLiteral("op_adaptation"), QStringLiteral("op_mounted"),
        QStringLiteral("op_friction_chart"), QStringLiteral("op_leak"), QStringLiteral("op_conclusion"),
        QStringLiteral("op_remark"),
        QStringLiteral("dynamic_error"), QStringLiteral("dynamic_error_rec"),
        QStringLiteral("stroke"), QStringLiteral("stroke_rec"),
        QStringLiteral("spring_range"), QStringLiteral("spring_range_rec"),
        QStringLiteral("actuator_pressure"),
        QStringLiteral("friction_coef"), QStringLiteral("friction_mean"),
        QStringLiteral("stroke_time_forward"), QStringLiteral("stroke_time_backward"),
    };

    QByteArray out("\xEF\xBB\xBF");  // BOM — чтобы Excel открыл UTF-8 с кириллицей
    auto writeRow = [&out](const QStringList& fields) {
        QStringList escaped;
        escaped.reserve(fields.size());
        for (const QString& f : fields)
            escaped << csvField(f);
        out += escaped.join(QLatin1Char(';')).toUtf8();
        out += "\r\n";
    };
    writeRow(header);

    while (q.next()) {
        const qint64 id = q.value(0).toLongLong();
        const QStringList rawLabels = labelsFromJson(q.value(3).toString());
        QSet<QString> resolved;
        for (const QString& key : rawLabels)
            resolved.insert(schema.resolveAlias(key));

        const ValveCard card = cardFromColumn(q.value(14));
        const QString hashAtAnswer = q.value(6).toString();

        QStringList row = {
            q.value(10).toString(),
            q.value(11).toString(),
            QString::number(id),
            currentIds.contains(id) ? QStringLiteral("1") : QStringLiteral("0"),
            q.value(1).toString(),
            q.value(2).toString(),
            q.value(5).toString(),
        };
        for (const QString& key : labelKeys)
            row << (resolved.contains(key) ? QStringLiteral("1") : QStringLiteral("0"));
        row << QStringList{
            rawLabels.join(QLatin1Char('|')),
            q.value(4).toString(),
            QString::number(q.value(9).toDouble(), 'f', 1),
            q.value(7).toString(),
            q.value(8).toString(),
            hashAtAnswer,
            hashAtAnswer != q.value(12).toString() ? QStringLiteral("1") : QStringLiteral("0"),
            q.value(13).toString(),
            q.value(15).toString(),
            card.position, card.manufacturer, card.model, card.positioner, card.failPositionShort(), card.actuatorModel,
            card.supplyPressure,
            card.seat, card.adaptation, card.mounted, card.frictionChart, card.leak, card.conclusion,
            card.ok ? (card.operatorRemark() ? QStringLiteral("1") : QStringLiteral("0")) : QString(),
            card.dynamicError, card.dynamicErrorRec, card.stroke, card.strokeRec,
            card.springRange, card.springRangeRec, card.actuatorPressure,
            card.frictionCoef, card.frictionMean, card.strokeTimeForward, card.strokeTimeBackward,
        };
        writeRow(row);
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(out);
    return true;
}

}
