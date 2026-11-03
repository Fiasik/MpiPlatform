#include "HistoryReader.h"
#include "HistoryDb.h"
#include "ValveSnapshotMapper.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

// HistoryDb::withConnection под именем этого файла — qWarning() на
// открытии БД раньше шёл с префиксом "[HistoryReader]"; HistoryDb::
// withConnection сам логирует как "[HistoryDb]", более общо. Оставлять
// отдельную обёртку только ради префикса лога не стали — не настолько
// ценная информация, чтобы ради неё держать лишний слой.
using HistoryDb::withConnection;

// site_id самой свежей сессии этой позиции — общий первый шаг для
// valveInfoAt()/materialsAt() (обеим нужен один и тот же session_id).
qint64 latestSessionId(QSqlDatabase& db, const QString& object, const QString& manufactory,
                       const QString& department, const QString& position)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT s.id FROM sessions s "
        "JOIN positions p ON p.id = s.position_id "
        "JOIN sites si ON si.id = p.site_id "
        "WHERE si.object = ? AND si.manufactory = ? AND si.department = ? "
        "AND p.position_number = ? "
        "ORDER BY s.ts DESC LIMIT 1"));
    q.addBindValue(object);
    q.addBindValue(manufactory);
    q.addBindValue(department);
    q.addBindValue(position);

    if (!q.exec() || !q.next())
        return -1;
    return q.value(0).toLongLong();
}

}

QStringList HistoryReader::objects()
{
    QStringList result;
    withConnection(QStringLiteral("mpiplatform_history_reader_objects"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "SELECT DISTINCT object FROM sites WHERE object != '' ORDER BY object"))) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[HistoryReader] objects():" << q.lastError().text();
        }
    });
    return result;
}

QStringList HistoryReader::manufactories(const QString& object)
{
    QStringList result;
    withConnection(QStringLiteral("mpiplatform_history_reader_manufactories"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT DISTINCT manufactory FROM sites "
            "WHERE object = ? AND manufactory != '' ORDER BY manufactory"));
        q.addBindValue(object);
        if (q.exec()) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[HistoryReader] manufactories():" << q.lastError().text();
        }
    });
    return result;
}

QStringList HistoryReader::departments(const QString& object, const QString& manufactory)
{
    QStringList result;
    withConnection(QStringLiteral("mpiplatform_history_reader_departments"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT DISTINCT department FROM sites "
            "WHERE object = ? AND manufactory = ? AND department != '' ORDER BY department"));
        q.addBindValue(object);
        q.addBindValue(manufactory);
        if (q.exec()) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[HistoryReader] departments():" << q.lastError().text();
        }
    });
    return result;
}

QStringList HistoryReader::positions(const QString& object, const QString& manufactory,
                                     const QString& department)
{
    QStringList result;
    withConnection(QStringLiteral("mpiplatform_history_reader_positions"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT DISTINCT p.position_number FROM positions p "
            "JOIN sites s ON s.id = p.site_id "
            "WHERE s.object = ? AND s.manufactory = ? AND s.department = ? "
            "ORDER BY p.position_number"));
        q.addBindValue(object);
        q.addBindValue(manufactory);
        q.addBindValue(department);
        if (q.exec()) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[HistoryReader] positions():" << q.lastError().text();
        }
    });
    return result;
}

bool HistoryReader::checkObject(const QString& object)
{
    bool exists = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_check_object"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT 1 FROM sites WHERE object = ? LIMIT 1"));
        q.addBindValue(object);
        exists = q.exec() && q.next();
    });
    return exists;
}

bool HistoryReader::checkManufactory(const QString& object, const QString& manufactory)
{
    bool exists = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_check_manufactory"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT 1 FROM sites WHERE object = ? AND manufactory = ? LIMIT 1"));
        q.addBindValue(object);
        q.addBindValue(manufactory);
        exists = q.exec() && q.next();
    });
    return exists;
}

bool HistoryReader::checkDepartment(const QString& object, const QString& manufactory,
                                    const QString& department)
{
    bool exists = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_check_department"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT 1 FROM sites WHERE object = ? AND manufactory = ? AND department = ? LIMIT 1"));
        q.addBindValue(object);
        q.addBindValue(manufactory);
        q.addBindValue(department);
        exists = q.exec() && q.next();
    });
    return exists;
}

bool HistoryReader::valveInfoAt(const QString& object, const QString& manufactory,
                                const QString& department, const QString& position,
                                ValveInfo& out)
{
    bool found = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_valve"), [&](QSqlDatabase& db) {
        const qint64 sessionId = latestSessionId(db, object, manufactory, department, position);
        if (sessionId < 0)
            return;

        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT * FROM valve_snapshots WHERE session_id = ?"));
        q.addBindValue(sessionId);
        if (!q.exec() || !q.next()) {
            qWarning() << "[HistoryReader] valveInfoAt():" << q.lastError().text();
            return;
        }

        ValveSnapshotMapper::read(q, out);
        out.positionNumber = position;

        found = true;
    });
    return found;
}

bool HistoryReader::materialsAt(const QString& object, const QString& manufactory,
                                const QString& department, const QString& position,
                                MaterialsOfComponentParts& out)
{
    bool found = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_materials"), [&](QSqlDatabase& db) {
        const qint64 sessionId = latestSessionId(db, object, manufactory, department, position);
        if (sessionId < 0)
            return;

        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT * FROM valve_snapshot_materials WHERE session_id = ?"));
        q.addBindValue(sessionId);
        if (!q.exec() || !q.next()) {
            // Не варнинг: материалы у старых/чужих сессий могут отсутствовать
            // (таблица добавлена позже valve_snapshots) — это ожидаемо, не
            // ошибка запроса.
            return;
        }

        out.plunger = q.value(QStringLiteral("plunger")).toString();
        out.saddle = q.value(QStringLiteral("saddle")).toString();
        out.cap = q.value(QStringLiteral("cap")).toString();
        out.corpus = q.value(QStringLiteral("corpus")).toString();
        out.ball = q.value(QStringLiteral("ball")).toString();
        out.disk = q.value(QStringLiteral("disk")).toString();
        out.shaft = q.value(QStringLiteral("shaft")).toString();
        out.stock = q.value(QStringLiteral("stock")).toString();
        out.guideSleeve = q.value(QStringLiteral("guide_sleeve")).toString();
        out.stuffingBoxSeal = q.value(QStringLiteral("stuffing_box_seal")).toString();
        out.CV = q.value(QStringLiteral("cv")).toString();

        found = true;
    });
    return found;
}
