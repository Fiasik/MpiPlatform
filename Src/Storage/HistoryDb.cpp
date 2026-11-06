#include "HistoryDb.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <mutex>

namespace {

struct Migration {
    int version;
    const char* resource;
};

// Номера и порядок не менять после выпуска: версия файла БД на машине
// заказчика — это номер последней применённой записи отсюда. Новое
// изменение схемы — новая запись в конце + новый .sql в database.qrc.
constexpr Migration kMigrations[] = {
    {1, ":/Database/migrations/001_baseline.sql"},
    {2, ":/Database/migrations/002_valve_card_and_passfail.sql"},
    {3, ":/Database/migrations/003_legacy_source_and_app_state.sql"},
    {4, ":/Database/migrations/004_session_supply_pressure.sql"},
};

constexpr int kLatestVersion =
    static_cast<int>(sizeof(kMigrations) / sizeof(kMigrations[0]));

constexpr int kBackupsToKeep = 5;
constexpr const char* kBusyTimeout = "QSQLITE_BUSY_TIMEOUT=5000";

QString resolveDatabasePath()
{
    const QByteArray fromEnv = qgetenv("MPIPLATFORM_DB");
    if (!fromEnv.isEmpty())
        return QString::fromLocal8Bit(fromEnv);

    const QString nextToExe =
        QCoreApplication::applicationDirPath() + QStringLiteral("/Database/mpiplatform.db");
    if (QFileInfo::exists(nextToExe))
        return nextToExe;

    // MPIPLATFORM_SOURCE_DIR — абсолютный путь сборочной машины, зашитый при
    // компиляции: у разработчика он существует и там лежит полная БД, у
    // заказчика — нет (та же логика, что у theme.qss в ThemeManager).
    const QString devTree = QStringLiteral(MPIPLATFORM_SOURCE_DIR "/Database/mpiplatform.db");
    if (QFileInfo::exists(devTree))
        return devTree;

    return nextToExe;
}

// Миграции — обычный SQL-текст с комментариями "-- ..." и операторами через
// ";". Qt-драйвер SQLite выполняет за один exec() только один оператор,
// поэтому режем сами. В миграциях нет ни ";", ни "--" внутри строковых
// литералов — это условие их написания.
QStringList splitStatements(const QString& script)
{
    QString stripped;
    const QStringList lines = script.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        QString line = raw;
        const int comment = line.indexOf(QStringLiteral("--"));
        if (comment >= 0)
            line.truncate(comment);
        stripped += line;
        stripped += QLatin1Char('\n');
    }

    QStringList statements;
    const QStringList parts = stripped.split(QLatin1Char(';'));
    for (const QString& part : parts) {
        const QString statement = part.trimmed();
        if (!statement.isEmpty())
            statements << statement;
    }
    return statements;
}

bool readVersion(QSqlQuery& q, int& version)
{
    if (!q.exec(QStringLiteral("PRAGMA user_version")) || !q.next())
        return false;
    version = q.value(0).toInt();
    q.finish();
    return true;
}

// Только чтение: версия файла и есть ли в нём вообще таблицы.
bool probe(const QString& path, int& version, bool& hasTables, QString* error)
{
    bool ok = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    QStringLiteral("mpiplatform_schema_probe"));
        db.setDatabaseName(path);
        db.setConnectOptions(QString::fromLatin1(kBusyTimeout));

        if (!db.open()) {
            *error = db.lastError().text();
        } else {
            QSqlQuery q(db);
            if (readVersion(q, version)
                && q.exec(QStringLiteral(
                       "SELECT COUNT(*) FROM sqlite_master "
                       "WHERE type = 'table' AND name NOT LIKE 'sqlite_%'"))
                && q.next()) {
                hasTables = q.value(0).toInt() > 0;
                ok = true;
            } else {
                *error = q.lastError().text();
            }
            q.finish();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(QStringLiteral("mpiplatform_schema_probe"));
    return ok;
}

// Копия файла в Database/backup/ рядом с самой БД; хранятся последние
// kBackupsToKeep. Пусто — копию сделать не удалось.
QString makeBackup(const QString& dbPath, int fromVersion)
{
    const QFileInfo info(dbPath);
    QDir dir(info.absolutePath());
    if (!dir.mkpath(QStringLiteral("backup")))
        return QString();

    const QString name = QStringLiteral("%1_v%2_%3.db")
        .arg(info.completeBaseName())
        .arg(fromVersion)
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    // Две копии в одну секунду (импорт сразу после миграции) не должны
    // конфликтовать по имени.
    QString target = dir.filePath(QStringLiteral("backup/") + name);
    for (int n = 2; QFileInfo::exists(target) && n < 100; ++n)
        target = dir.filePath(QStringLiteral("backup/%1_%2.db").arg(name.chopped(3)).arg(n));

    if (!QFile::copy(dbPath, target))
        return QString();

    QDir backups(dir.filePath(QStringLiteral("backup")));
    const QFileInfoList all =
        backups.entryInfoList({QStringLiteral("*.db")}, QDir::Files, QDir::Time);
    for (int i = kBackupsToKeep; i < all.size(); ++i)
        QFile::remove(all.at(i).absoluteFilePath());

    return target;
}

bool rollback(QSqlQuery& q, const QString& reason, QString* error)
{
    *error = reason;
    q.exec(QStringLiteral("ROLLBACK"));
    return false;
}

// BEGIN IMMEDIATE + повторная проверка версии уже под блокировкой: если
// другой процесс успел применить эту же миграцию раньше (две копии
// приложения стартуют одновременно), просто пропускаем её.
bool applyStep(QSqlDatabase& db, int version, const QStringList& statements, QString* error)
{
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("BEGIN IMMEDIATE"))) {
        *error = q.lastError().text();
        return false;
    }

    int current = 0;
    if (!readVersion(q, current))
        return rollback(q, q.lastError().text(), error);
    if (current >= version) {
        q.exec(QStringLiteral("ROLLBACK"));
        return true;
    }

    for (const QString& statement : statements) {
        if (!q.exec(statement)) {
            return rollback(
                q, QStringLiteral("%1 (оператор: %2)").arg(q.lastError().text(), statement.left(80)),
                error);
        }
    }

    if (!q.exec(QStringLiteral("PRAGMA user_version = %1").arg(version)))
        return rollback(q, q.lastError().text(), error);

    if (!q.exec(QStringLiteral("COMMIT")))
        return rollback(q, q.lastError().text(), error);

    return true;
}

bool applyMigration(QSqlDatabase& db, const Migration& migration, QString* error)
{
    QFile file(QString::fromLatin1(migration.resource));
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("не найден вшитый файл миграции %1").arg(QString::fromLatin1(migration.resource));
        return false;
    }
    const QStringList statements = splitStatements(QString::fromUtf8(file.readAll()));
    return applyStep(db, migration.version, statements, error);
}

// Приводит файл БД по path к актуальной версии схемы. backup — делать ли
// копию в <папка файла>/backup/ перед миграцией существующей базы (для
// рабочей БД да, для временной копии чужой базы при импорте нет).
HistoryDb::SchemaResult migrateFile(const QString& path, bool backup)
{
    using HistoryDb::SchemaStatus;

    HistoryDb::SchemaResult result;

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        result.message = QStringLiteral("Не удалось создать папку для базы данных: %1")
            .arg(QFileInfo(path).absolutePath());
        return result;
    }

    int stored = 0;
    bool hasTables = false;
    QString error;
    if (!probe(path, stored, hasTables, &error)) {
        result.message = QStringLiteral("Не удалось открыть базу данных %1: %2").arg(path, error);
        return result;
    }

    result.fromVersion = stored;
    result.toVersion = stored;

    if (stored > kLatestVersion) {
        result.status = SchemaStatus::TooNew;
        result.message = QStringLiteral(
            "База данных %1 создана более новой версией приложения (версия схемы %2, эта версия "
            "приложения поддерживает до %3). Обновите приложение; файл не тронут.")
            .arg(path).arg(stored).arg(kLatestVersion);
        return result;
    }

    // Файл с таблицами, но без версии — это база, созданная до появления
    // миграций: по схеме она равна baseline (версия 1).
    const int effective = (hasTables && stored == 0) ? 1 : stored;
    const bool needWork = !hasTables || stored < kLatestVersion;

    if (!needWork) {
        result.status = SchemaStatus::Ok;
        return result;
    }

    if (hasTables && backup) {
        result.backupPath = makeBackup(path, stored);
        if (result.backupPath.isEmpty())
            qWarning() << "[HistoryDb] Не удалось сделать копию БД перед миграцией:" << path;
    }

    bool ok = true;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    QStringLiteral("mpiplatform_schema"));
        db.setDatabaseName(path);
        db.setConnectOptions(QString::fromLatin1(kBusyTimeout));

        if (!db.open()) {
            error = db.lastError().text();
            ok = false;
        } else {
            int version = effective;

            if (!hasTables) {
                ok = applyMigration(db, kMigrations[0], &error);
                version = 1;
            } else if (stored == 0) {
                QSqlQuery q(db);
                ok = q.exec(QStringLiteral("PRAGMA user_version = 1"));
                if (!ok)
                    error = q.lastError().text();
            }

            for (const Migration& migration : kMigrations) {
                if (!ok)
                    break;
                if (migration.version > version)
                    ok = applyMigration(db, migration, &error);
            }

            db.close();
        }
    }
    QSqlDatabase::removeDatabase(QStringLiteral("mpiplatform_schema"));

    if (!ok) {
        result.status = SchemaStatus::Failed;
        result.message = QStringLiteral("Не удалось обновить базу данных %1: %2. Изменения откатаны.")
            .arg(path, error);
        if (!result.backupPath.isEmpty())
            result.message += QStringLiteral(" Копия до обновления: %1").arg(result.backupPath);
        return result;
    }

    result.status = hasTables ? SchemaStatus::Migrated : SchemaStatus::Created;
    result.toVersion = kLatestVersion;
    return result;
}

}

QString HistoryDb::databasePath()
{
    static const QString path = resolveDatabasePath();
    return path;
}

int HistoryDb::latestSchemaVersion()
{
    return kLatestVersion;
}

HistoryDb::SchemaResult HistoryDb::upgradeFile(const QString& path)
{
    return migrateFile(path, false);
}

QString HistoryDb::backupDatabase()
{
    if (!isUsable(ensureSchema().status))
        return QString();
    return makeBackup(databasePath(), kLatestVersion);
}

const HistoryDb::SchemaResult& HistoryDb::ensureSchema()
{
    static std::once_flag once;
    static SchemaResult result;

    std::call_once(once, [] {
        result = migrateFile(databasePath(), true);

        switch (result.status) {
        case SchemaStatus::Created:
            qInfo() << "[HistoryDb] База создана:" << databasePath() << "версия" << result.toVersion;
            break;
        case SchemaStatus::Migrated:
            qInfo() << "[HistoryDb] База обновлена" << result.fromVersion << "->" << result.toVersion
                    << "копия:" << result.backupPath;
            break;
        case SchemaStatus::TooNew:
        case SchemaStatus::Failed:
            qWarning().noquote() << "[HistoryDb]" << result.message;
            break;
        case SchemaStatus::Ok:
            break;
        }
    });

    return result;
}

bool HistoryDb::withConnection(const QString& connectionName, const std::function<void(QSqlDatabase&)>& fn)
{
    if (!isUsable(ensureSchema().status))
        return false;

    bool opened = false;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(databasePath());
        db.setConnectOptions(QString::fromLatin1(kBusyTimeout));

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
    if (!isUsable(ensureSchema().status))
        return QSqlDatabase();

    QSqlDatabase db = QSqlDatabase::contains(connectionName)
        ? QSqlDatabase::database(connectionName)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(databasePath());
    db.setConnectOptions(QString::fromLatin1(kBusyTimeout));
    return db;
}

void HistoryDb::removePersistent(const QString& connectionName)
{
    if (QSqlDatabase::contains(connectionName))
        QSqlDatabase::removeDatabase(connectionName);
}
