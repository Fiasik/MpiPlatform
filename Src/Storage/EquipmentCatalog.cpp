#include "EquipmentCatalog.h"
#include "HistoryDb.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace EquipmentCatalog {

namespace {

QString kindKey(Kind kind)
{
    return kind == Kind::Positioner ? QStringLiteral("positioner")
                                    : QStringLiteral("drive");
}

// У приводов коэффициента нет — ключ всегда '', что бы ни передали.
// Пустой QString может быть null и тогда привяжется к запросу как NULL,
// поэтому в SQL такие параметры обёрнуты в COALESCE(?, '').
QString dynamicErrorKey(Kind kind, const QString& dynamicError)
{
    return kind == Kind::Positioner ? dynamicError.trimmed() : QString();
}

}

QStringList manufacturers()
{
    QStringList result;
    HistoryDb::withConnection(QStringLiteral("mpiplatform_catalog_manufacturers"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "SELECT name FROM catalog_manufacturers ORDER BY sort_order, name"))) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[EquipmentCatalog] manufacturers():" << q.lastError().text();
        }
    });
    return result;
}

QStringList models(Kind kind, const QString& manufacturer, const QString& dynamicError)
{
    QStringList result;
    HistoryDb::withConnection(QStringLiteral("mpiplatform_catalog_models"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        // Сравнение manufacturer — по COLLATE NOCASE из схемы. Моделей без
        // производителя (manufacturer = '') после миграции 007 нет; пустой
        // производитель ("Ручной ввод") даёт пустой список.
        q.prepare(QStringLiteral(
            "SELECT name FROM catalog_models "
            "WHERE kind = ? AND dynamic_error = COALESCE(?, '') "
            "AND manufacturer = COALESCE(?, '') AND manufacturer <> '' "
            "ORDER BY sort_order, name"));
        q.addBindValue(kindKey(kind));
        q.addBindValue(dynamicErrorKey(kind, dynamicError));
        q.addBindValue(manufacturer.trimmed());
        if (q.exec()) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[EquipmentCatalog] models():" << q.lastError().text();
        }
    });
    return result;
}

QList<Entry> entries(Kind kind)
{
    QList<Entry> result;
    HistoryDb::withConnection(QStringLiteral("mpiplatform_catalog_entries"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT manufacturer, name FROM catalog_models "
            "WHERE kind = ? AND manufacturer <> '' "
            "GROUP BY manufacturer, name ORDER BY manufacturer, name"));
        q.addBindValue(kindKey(kind));
        if (q.exec()) {
            while (q.next())
                result.append(Entry{q.value(0).toString(), q.value(1).toString()});
        } else {
            qWarning() << "[EquipmentCatalog] entries():" << q.lastError().text();
        }
    });
    return result;
}

bool addManufacturer(const QString& name)
{
    const QString value = name.trimmed();
    if (value.isEmpty())
        return false;

    bool ok = false;
    HistoryDb::withConnection(QStringLiteral("mpiplatform_catalog_add_manufacturer"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO catalog_manufacturers(name, sort_order, source) "
            "SELECT ?, COALESCE(MAX(sort_order), 0) + 1, 'user' FROM catalog_manufacturers"));
        q.addBindValue(value);
        ok = q.exec();
        if (!ok)
            qWarning() << "[EquipmentCatalog] addManufacturer():" << q.lastError().text();
    });
    return ok;
}

bool addModel(Kind kind, const QString& manufacturer, const QString& dynamicError,
              const QString& name)
{
    const QString value = name.trimmed();
    const QString maker = manufacturer.trimmed();
    if (value.isEmpty() || maker.isEmpty())
        return false;

    bool ok = false;
    HistoryDb::withConnection(QStringLiteral("mpiplatform_catalog_add_model"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO catalog_models"
            "(kind, manufacturer, dynamic_error, name, sort_order, source) "
            "SELECT ?, ?, COALESCE(?, ''), ?, COALESCE(MAX(sort_order), 0) + 1, 'user' FROM catalog_models"));
        q.addBindValue(kindKey(kind));
        q.addBindValue(maker);
        q.addBindValue(dynamicErrorKey(kind, dynamicError));
        q.addBindValue(value);
        ok = q.exec();
        if (!ok)
            qWarning() << "[EquipmentCatalog] addModel():" << q.lastError().text();
    });
    return ok;
}

}
