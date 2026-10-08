#include "HistoryDb.h"

#include <QDebug>
#include <QSqlError>

QString HistoryDb::databasePath()
{
    return QStringLiteral(MPIPLATFORM_SOURCE_DIR "/Database/mpiplatform.db");
}

bool HistoryDb::withConnection(const QString& connectionName, const std::function<void(QSqlDatabase&)>& fn)
{
    bool opened = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databasePath());

        if (db.open()) {
            opened = true;
            fn(db);
            db.close();
        } else {
            qWarning() << "[HistoryDb] Не удалось открыть БД истории:" << db.lastError().text();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return opened;
}

QSqlDatabase HistoryDb::openPersistent(const QString& connectionName)
{
    QSqlDatabase db = QSqlDatabase::contains(connectionName)
        ? QSqlDatabase::database(connectionName)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(databasePath());
    return db;
}

void HistoryDb::removePersistent(const QString& connectionName)
{
    if (QSqlDatabase::contains(connectionName))
        QSqlDatabase::removeDatabase(connectionName);
}
