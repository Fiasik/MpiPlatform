#include "DataTransfer.h"
#include "HistoryDb.h"
#include "SessionFolders.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPair>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>

namespace {

constexpr int kFormatVersion = 1;
const QString kDbName = QStringLiteral("mpiplatform.db");
const QString kDataDir = QStringLiteral("data");
const QString kManifestName = QStringLiteral("manifest.json");
const QString kExternalDir = QStringLiteral("Внешние папки");

// ---------------------------------------------------------------- файлы

// Рекурсивная копия каталога; символические ссылки пропускаются. to не
// должен лежать внутри from (иначе копия копировала бы саму себя).
bool copyDirectory(const QString& from, const QString& to, QString* error)
{
    const QString fromAbs = QDir::cleanPath(QFileInfo(from).absoluteFilePath());
    const QString toAbs = QDir::cleanPath(QFileInfo(to).absoluteFilePath());
    if (toAbs == fromAbs || toAbs.startsWith(fromAbs + QLatin1Char('/'))) {
        *error = QStringLiteral("Папка назначения лежит внутри копируемой: %1").arg(fromAbs);
        return false;
    }

    if (!QDir().mkpath(toAbs)) {
        *error = QStringLiteral("Не удалось создать папку %1").arg(toAbs);
        return false;
    }

    const QFileInfoList entries = QDir(fromAbs).entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& entry : entries) {
        if (entry.isSymLink())
            continue;

        const QString target = toAbs + QLatin1Char('/') + entry.fileName();
        if (entry.isDir()) {
            if (!copyDirectory(entry.absoluteFilePath(), target, error))
                return false;
        } else if (!QFile::copy(entry.absoluteFilePath(), target)) {
            *error = QStringLiteral("Не удалось скопировать файл %1").arg(entry.absoluteFilePath());
            return false;
        }
    }
    return true;
}

// Самый верхний ещё не существующий каталог на пути к path: то, что
// mkpath() создаст целиком. Откат импорта удаляет именно его, чтобы не
// оставлять пустые цепочки родительских папок.
QString topmostMissing(const QString& path)
{
    QString current = QDir::cleanPath(path);
    for (;;) {
        const QString parent = QFileInfo(current).absolutePath();
        if (parent == current || QFileInfo::exists(parent))
            return current;
        current = parent;
    }
}

// -------------------------------------------------------------------- SQL

struct Column {
    QString name;
    int pk = 0;   // 0 — не в первичном ключе, иначе позиция в ключе
};

QVector<Column> columnsOf(QSqlDatabase& db, const QString& schema, const QString& table)
{
    QVector<Column> columns;
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("PRAGMA %1.table_info(\"%2\")").arg(schema, table)))
        return columns;
    while (q.next())
        columns.append({q.value(1).toString(), q.value(5).toInt()});
    return columns;
}

QString quoted(const QString& name)
{
    return QLatin1Char('"') + name + QLatin1Char('"');
}

QString join(const QStringList& list, const QString& prefix = QString())
{
    QStringList out;
    for (const QString& item : list)
        out << prefix + quoted(item);
    return out.join(QStringLiteral(", "));
}

bool run(QSqlQuery& q, const QString& sql, QString* error)
{
    if (q.exec(sql))
        return true;
    *error = QStringLiteral("%1 (запрос: %2)").arg(q.lastError().text(), sql.left(100));
    return false;
}

// --------------------------------------------------------------- импорт

struct SessionRow {
    qint64 srcId = 0;
    qint64 destPositionId = 0;
    QString ts;
    QString folder;
    QString source;
};

bool doMerge(QSqlDatabase& db, const QString& dataRoot, const QString& appRoot,
             DataTransfer::ImportResult& r, QStringList& createdDirs)
{
    QString& error = r.error;
    QSqlQuery q(db);

    if (!run(q, QStringLiteral("BEGIN IMMEDIATE"), &error))
        return false;

    const auto fail = [&]() {
        QSqlQuery rollback(db);
        rollback.exec(QStringLiteral("ROLLBACK"));
        return false;
    };

    if (!run(q, QStringLiteral(
            "CREATE TEMP TABLE import_map(src_id INTEGER PRIMARY KEY, new_id INTEGER NOT NULL)"), &error))
        return fail();

    // --- sites / positions: совпадение по естественному ключу -------------
    {
        QStringList cols;
        for (const Column& c : columnsOf(db, QStringLiteral("main"), QStringLiteral("sites")))
            if (c.name != QLatin1String("id"))
                cols << c.name;

        if (!run(q, QStringLiteral(
                "INSERT INTO main.sites(%1) SELECT %2 FROM src.sites s "
                "WHERE NOT EXISTS (SELECT 1 FROM main.sites d WHERE d.object = s.object "
                "AND d.manufactory = s.manufactory AND d.department = s.department)")
                .arg(join(cols), join(cols, QStringLiteral("s."))), &error))
            return fail();
        r.sitesAdded = q.numRowsAffected();
    }
    {
        QStringList cols;
        for (const Column& c : columnsOf(db, QStringLiteral("main"), QStringLiteral("positions")))
            if (c.name != QLatin1String("id") && c.name != QLatin1String("site_id"))
                cols << c.name;

        if (!run(q, QStringLiteral(
                "INSERT INTO main.positions(site_id, %1) SELECT ds.id, %2 FROM src.positions p "
                "JOIN src.sites ss ON ss.id = p.site_id "
                "JOIN main.sites ds ON ds.object = ss.object AND ds.manufactory = ss.manufactory "
                "AND ds.department = ss.department "
                "WHERE NOT EXISTS (SELECT 1 FROM main.positions d WHERE d.site_id = ds.id "
                "AND d.position_number = p.position_number)")
                .arg(join(cols), join(cols, QStringLiteral("p."))), &error))
            return fail();
        r.positionsAdded = q.numRowsAffected();
    }

    // --- sessions ---------------------------------------------------------
    QVector<SessionRow> rows;
    if (!run(q, QStringLiteral(
            "SELECT s.id, dp.id, s.ts, s.folder_path, s.source FROM src.sessions s "
            "JOIN src.positions sp ON sp.id = s.position_id "
            "JOIN src.sites ss ON ss.id = sp.site_id "
            "JOIN main.sites ds ON ds.object = ss.object AND ds.manufactory = ss.manufactory "
            "AND ds.department = ss.department "
            "JOIN main.positions dp ON dp.site_id = ds.id AND dp.position_number = sp.position_number "
            "ORDER BY s.id"), &error))
        return fail();
    while (q.next()) {
        SessionRow row;
        row.srcId = q.value(0).toLongLong();
        row.destPositionId = q.value(1).toLongLong();
        row.ts = q.value(2).toString();
        row.folder = q.value(3).toString();
        row.source = q.value(4).toString();
        rows.append(row);
    }
    q.finish();

    QStringList sessionCols;
    for (const Column& c : columnsOf(db, QStringLiteral("main"), QStringLiteral("sessions")))
        if (c.name != QLatin1String("id") && c.name != QLatin1String("position_id")
            && c.name != QLatin1String("folder_path"))
            sessionCols << c.name;

    const QString insertSession = QStringLiteral(
        "INSERT INTO main.sessions(position_id, folder_path, %1) "
        "SELECT ?, ?, %2 FROM src.sessions WHERE id = ?")
        .arg(join(sessionCols), join(sessionCols));

    QHash<QString, QString> copiedFolders;   // исходный путь -> путь в базе после копирования
    QSet<QString> reservedNames;

    // Сколько сессий с такой "позиция + время" в рабочей базе УЖЕ было до
    // импорта. Сравниваем счётчиками, а не "есть/нет": у архивных сессий время
    // — только дата, и в одной базе бывает несколько сессий с одинаковой
    // парой (в текущей их 38) — все они должны переехать в пустую базу, а при
    // повторном импорте того же файла — не задвоиться.
    QHash<QPair<qint64, QString>, int> existing;
    if (!run(q, QStringLiteral("SELECT position_id, ts, COUNT(*) FROM main.sessions GROUP BY position_id, ts"),
             &error))
        return fail();
    while (q.next())
        existing.insert({q.value(0).toLongLong(), q.value(1).toString()}, q.value(2).toInt());
    q.finish();

    for (const SessionRow& row : rows) {
        int& present = existing[{row.destPositionId, row.ts}];
        if (present > 0) {
            --present;
            ++r.sessionsSkipped;
            continue;
        }

        QString newFolder = row.folder;
        if (!row.folder.isEmpty()) {
            const QString sourceDir = dataRoot.isEmpty()
                ? QString()
                : QDir(dataRoot).filePath(row.folder);

            if (copiedFolders.contains(row.folder)) {
                newFolder = copiedFolders.value(row.folder);
            } else if (SessionFolders::isSafeRelative(row.folder) && !sourceDir.isEmpty()
                       && QFileInfo(sourceDir).isDir()) {
                QString candidate = row.folder;
                int n = 1;
                while (QFileInfo::exists(SessionFolders::resolve(candidate, appRoot))
                       || reservedNames.contains(candidate))
                    candidate = QStringLiteral("%1_%2").arg(row.folder).arg(++n);

                const QString destDir = SessionFolders::resolve(candidate, appRoot);
                createdDirs << topmostMissing(destDir);
                if (!copyDirectory(sourceDir, destDir, &error))
                    return fail();

                reservedNames.insert(candidate);
                copiedFolders.insert(row.folder, candidate);
                newFolder = candidate;
                ++r.foldersCopied;
                if (candidate != row.folder)
                    ++r.foldersRenamed;
            } else if (row.source != QLatin1String("harvest")) {
                ++r.foldersMissing;
            }
        }

        QSqlQuery ins(db);
        ins.prepare(insertSession);
        ins.addBindValue(row.destPositionId);
        ins.addBindValue(row.folder.isNull() ? QVariant() : QVariant(newFolder));
        ins.addBindValue(row.srcId);
        if (!ins.exec()) {
            error = ins.lastError().text();
            return fail();
        }
        const qint64 newId = ins.lastInsertId().toLongLong();

        QSqlQuery map(db);
        map.prepare(QStringLiteral("INSERT INTO temp.import_map(src_id, new_id) VALUES (?, ?)"));
        map.addBindValue(row.srcId);
        map.addBindValue(newId);
        if (!map.exec()) {
            error = map.lastError().text();
            return fail();
        }
        ++r.sessionsAdded;
    }

    // --- всё, что висит на session_id --------------------------------------
    // Таблицы находим по схеме, а не списком: новая таблица с session_id,
    // добавленная будущей миграцией, переносится без правок этого кода.
    QStringList tables;
    if (!run(q, QStringLiteral(
            "SELECT name FROM main.sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'"), &error))
        return fail();
    while (q.next())
        tables << q.value(0).toString();
    q.finish();

    static const QStringList kHandledElsewhere = {
        QStringLiteral("sites"), QStringLiteral("positions"), QStringLiteral("sessions"),
        QStringLiteral("app_state"),
        QStringLiteral("catalog_manufacturers"), QStringLiteral("catalog_models")};

    for (const QString& table : tables) {
        if (kHandledElsewhere.contains(table))
            continue;

        const QVector<Column> cols = columnsOf(db, QStringLiteral("main"), table);
        const QVector<Column> srcCols = columnsOf(db, QStringLiteral("src"), table);
        bool hasSessionId = false;
        int pkCount = 0;
        for (const Column& c : cols) {
            hasSessionId = hasSessionId || c.name == QLatin1String("session_id");
            pkCount += c.pk > 0 ? 1 : 0;
        }
        if (!hasSessionId || srcCols.isEmpty())
            continue;

        QStringList insertCols;
        QStringList selectExprs;
        for (const Column& c : cols) {
            // Суррогатный ключ строки (id INTEGER PRIMARY KEY) выдаёт сама база.
            if (c.name == QLatin1String("id") && c.pk == 1 && pkCount == 1)
                continue;
            insertCols << quoted(c.name);
            selectExprs << (c.name == QLatin1String("session_id")
                                ? QStringLiteral("m.new_id")
                                : QStringLiteral("s.") + quoted(c.name));
        }

        if (!run(q, QStringLiteral(
                "INSERT INTO main.%1(%2) SELECT %3 FROM src.%1 s "
                "JOIN temp.import_map m ON m.src_id = s.session_id")
                .arg(quoted(table), insertCols.join(QStringLiteral(", ")),
                     selectExprs.join(QStringLiteral(", "))), &error))
            return fail();
    }

    // --- справочник оборудования (миграция 005) ------------------------------
    // Ключи естественные, так что слияние — просто "добавить, чего нет".
    // Исходные записи у обеих баз одинаковые (из одной миграции) и
    // пропускаются; переезжает то, что на другом стенде ввели вручную.
    if (!run(q, QStringLiteral(
            "INSERT OR IGNORE INTO main.catalog_manufacturers(name, sort_order, source) "
            "SELECT name, sort_order, source FROM src.catalog_manufacturers"), &error))
        return fail();
    if (!run(q, QStringLiteral(
            "INSERT OR IGNORE INTO main.catalog_models"
            "(kind, manufacturer, dynamic_error, name, sort_order, source) "
            "SELECT kind, manufacturer, dynamic_error, name, sort_order, source "
            "FROM src.catalog_models"), &error))
        return fail();

    if (!run(q, QStringLiteral("COMMIT"), &error))
        return fail();

    QSqlQuery cleanup(db);
    cleanup.exec(QStringLiteral("DROP TABLE IF EXISTS temp.import_map"));
    return true;
}

// Где лежит база и где — папки сессий, по тому, что выбрал пользователь.
bool resolveSource(const QString& path, QString& dbFile, QString& dataRoot, QString* error)
{
    const QFileInfo info(path);

    const auto installRootOf = [](QDir database) {
        database.cdUp();
        return database.absolutePath();
    };

    if (info.isFile()) {
        dbFile = info.absoluteFilePath();
        const QDir parent = info.dir();
        if (parent.exists(kDataDir))
            dataRoot = parent.filePath(kDataDir);
        else if (parent.dirName() == QLatin1String("Database"))
            dataRoot = installRootOf(parent);
        return true;
    }

    if (info.isDir()) {
        const QDir dir(info.absoluteFilePath());
        if (dir.exists(kDbName)) {
            dbFile = dir.filePath(kDbName);
            if (dir.exists(kDataDir))
                dataRoot = dir.filePath(kDataDir);
            else if (dir.dirName() == QLatin1String("Database"))
                dataRoot = installRootOf(dir);
            return true;
        }
        if (dir.exists(QStringLiteral("Database/") + kDbName)) {
            dbFile = dir.filePath(QStringLiteral("Database/") + kDbName);
            dataRoot = dir.absolutePath();
            return true;
        }
        *error = QStringLiteral("В выбранной папке нет файла %1 (ни в ней самой, ни в подпапке Database).")
                     .arg(kDbName);
        return false;
    }

    *error = QStringLiteral("Путь не найден: %1").arg(path);
    return false;
}

}

// ============================================================== публичное

QString DataTransfer::ExportResult::summary() const
{
    if (!ok)
        return QStringLiteral("Экспорт не выполнен: %1").arg(error);

    QString text = QStringLiteral("Экспорт готов.\n\nПапка: %1\nСессий в базе: %2\nПапок с графиками и отчётами скопировано: %3")
                       .arg(QDir::toNativeSeparators(packagePath)).arg(sessions).arg(foldersCopied);
    if (foldersMissing > 0)
        text += QStringLiteral("\nПапок, указанных в базе, но не найденных на диске: %1").arg(foldersMissing);
    text += QStringLiteral("\n\nЧтобы перенести данные на другой компьютер, скопируйте эту папку целиком и "
                           "выберите её в «Импорт данных…».");
    return text;
}

QString DataTransfer::ImportResult::summary() const
{
    if (!ok)
        return QStringLiteral("Импорт не выполнен: %1").arg(error);

    if (sessionsAdded == 0) {
        return QStringLiteral("Новых данных нет: все %1 сессий из выбранной базы уже есть в вашей.")
            .arg(sessionsSkipped);
    }

    QString text = QStringLiteral("Импорт завершён.\n\nДобавлено сессий: %1\nУже были в базе (пропущено): %2")
                       .arg(sessionsAdded).arg(sessionsSkipped);
    text += QStringLiteral("\nНовых позиций: %1, новых связок объект/цех/отделение: %2")
                .arg(positionsAdded).arg(sitesAdded);
    text += QStringLiteral("\nПапок с графиками и отчётами скопировано: %1").arg(foldersCopied);
    if (foldersRenamed > 0)
        text += QStringLiteral(" (из них переименовано из-за совпадения имён: %1)").arg(foldersRenamed);
    if (foldersMissing > 0)
        text += QStringLiteral("\nПапок, указанных в базе, но не найденных в источнике: %1").arg(foldersMissing);
    if (!backupPath.isEmpty())
        text += QStringLiteral("\n\nКопия вашей базы до импорта:\n%1").arg(QDir::toNativeSeparators(backupPath));
    return text;
}

DataTransfer::ExportResult DataTransfer::exportAll(const QString& targetParentDir, const QString& appRootArg)
{
    ExportResult r;
    const QString root = appRootArg.isEmpty() ? SessionFolders::appRoot() : appRootArg;

    if (!HistoryDb::isUsable(HistoryDb::ensureSchema().status)) {
        r.error = QStringLiteral("база данных недоступна");
        return r;
    }

    QDir parent(targetParentDir);
    if (targetParentDir.isEmpty() || !parent.exists()) {
        r.error = QStringLiteral("папка назначения не найдена: %1").arg(targetParentDir);
        return r;
    }

    const QString name = QStringLiteral("MpiPlatform_export_%1")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    if (!parent.mkdir(name)) {
        r.error = QStringLiteral("не удалось создать папку %1 в %2").arg(name, targetParentDir);
        return r;
    }

    const QDir package(parent.filePath(name));
    r.packagePath = package.absolutePath();

    const auto abort = [&](const QString& message) {
        r.error = message;
        QDir(r.packagePath).removeRecursively();
        r.packagePath.clear();
        return r;
    };

    // 1. Целостная копия базы: VACUUM INTO не требует остановки записи и не
    //    тащит за собой журнал.
    const QString dbTarget = package.filePath(kDbName);
    bool copied = false;
    QString copyError;
    HistoryDb::withConnection(QStringLiteral("mpiplatform_export_vacuum"), [&](QSqlDatabase& db) {
        QString literal = QDir::cleanPath(dbTarget);
        literal.replace(QLatin1Char('\''), QStringLiteral("''"));
        QSqlQuery q(db);
        copied = q.exec(QStringLiteral("VACUUM INTO '%1'").arg(literal));
        if (!copied)
            copyError = q.lastError().text();
    });
    if (!copied)
        return abort(QStringLiteral("не удалось сохранить копию базы: %1").arg(copyError));

    // 2. Папки сессий и перепись путей в копии базы.
    bool ok = true;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    QStringLiteral("mpiplatform_export_package"));
        db.setDatabaseName(dbTarget);
        if (!db.open()) {
            ok = false;
            r.error = db.lastError().text();
        } else {
            QSqlQuery q(db);
            if (q.exec(QStringLiteral("SELECT COUNT(*) FROM sessions")) && q.next())
                r.sessions = q.value(0).toInt();

            struct Row { qint64 id; QString folder; QString source; };
            QVector<Row> rows;
            if (q.exec(QStringLiteral(
                    "SELECT id, folder_path, source FROM sessions "
                    "WHERE folder_path IS NOT NULL AND folder_path <> '' ORDER BY id"))) {
                while (q.next())
                    rows.append({q.value(0).toLongLong(), q.value(1).toString(), q.value(2).toString()});
            }
            q.finish();

            QSet<QString> done;
            db.transaction();
            for (const Row& row : rows) {
                const QString source = SessionFolders::resolve(row.folder, root);
                if (!QFileInfo(source).isDir()) {
                    if (row.source != QLatin1String("harvest"))
                        ++r.foldersMissing;
                    continue;
                }

                const bool relative = SessionFolders::isSafeRelative(row.folder);
                const QString rel = relative
                    ? row.folder
                    : QStringLiteral("%1/%2_%3").arg(kExternalDir).arg(row.id)
                          .arg(QFileInfo(source).fileName());

                if (!done.contains(rel)) {
                    QString error;
                    if (!copyDirectory(source, package.filePath(kDataDir + QLatin1Char('/') + rel), &error)) {
                        ok = false;
                        r.error = error;
                        break;
                    }
                    done.insert(rel);
                    ++r.foldersCopied;
                }

                if (rel != row.folder) {
                    QSqlQuery up(db);
                    up.prepare(QStringLiteral("UPDATE sessions SET folder_path = ? WHERE id = ?"));
                    up.addBindValue(rel);
                    up.addBindValue(row.id);
                    up.exec();
                }
            }
            if (ok)
                ok = db.commit();
            else
                db.rollback();
            q.finish();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(QStringLiteral("mpiplatform_export_package"));
    if (!ok)
        return abort(r.error.isEmpty() ? QStringLiteral("не удалось подготовить пакет") : r.error);

    // 3. Паспорт пакета.
    QJsonObject manifest;
    manifest.insert(QStringLiteral("format"), kFormatVersion);
    manifest.insert(QStringLiteral("schemaVersion"), HistoryDb::latestSchemaVersion());
    manifest.insert(QStringLiteral("exportedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    manifest.insert(QStringLiteral("sessions"), r.sessions);
    manifest.insert(QStringLiteral("folders"), r.foldersCopied);
    QFile manifestFile(package.filePath(kManifestName));
    if (manifestFile.open(QIODevice::WriteOnly))
        manifestFile.write(QJsonDocument(manifest).toJson());

    r.ok = true;
    return r;
}

DataTransfer::ImportResult DataTransfer::importFrom(const QString& sourcePath, const QString& appRootArg)
{
    ImportResult r;
    const QString root = appRootArg.isEmpty() ? SessionFolders::appRoot() : appRootArg;

    if (!HistoryDb::isUsable(HistoryDb::ensureSchema().status)) {
        r.error = QStringLiteral("база данных недоступна");
        return r;
    }

    QString srcDb;
    QString dataRoot;
    if (!resolveSource(sourcePath, srcDb, dataRoot, &r.error))
        return r;

    if (QFileInfo(srcDb).canonicalFilePath() == QFileInfo(HistoryDb::databasePath()).canonicalFilePath()) {
        r.error = QStringLiteral("выбрана текущая база данных — импортировать её саму в себя нельзя");
        return r;
    }

    const QString manifestPath = QFileInfo(srcDb).dir().filePath(kManifestName);
    if (QFileInfo::exists(manifestPath)) {
        QFile f(manifestPath);
        if (f.open(QIODevice::ReadOnly)) {
            const int format = QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("format")).toInt(1);
            if (format > kFormatVersion) {
                r.error = QStringLiteral("пакет создан более новой версией приложения (формат %1, эта версия "
                                         "понимает до %2). Обновите приложение.").arg(format).arg(kFormatVersion);
                return r;
            }
        }
    }

    // Чужую базу не трогаем: миграции идут в её временной копии.
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        r.error = QStringLiteral("не удалось создать временную папку");
        return r;
    }
    const QString work = tmp.filePath(QStringLiteral("source.db"));
    if (!QFile::copy(srcDb, work)) {
        r.error = QStringLiteral("не удалось прочитать файл %1").arg(srcDb);
        return r;
    }
    QFile::setPermissions(work, QFile::ReadOwner | QFile::WriteOwner);

    const HistoryDb::SchemaResult upgraded = HistoryDb::upgradeFile(work);
    if (upgraded.status == HistoryDb::SchemaStatus::TooNew) {
        r.error = QStringLiteral("выбранная база создана более новой версией приложения (версия схемы %1, "
                                 "эта версия поддерживает до %2). Обновите приложение.")
                      .arg(upgraded.fromVersion).arg(HistoryDb::latestSchemaVersion());
        return r;
    }
    if (!HistoryDb::isUsable(upgraded.status)) {
        r.error = QStringLiteral("выбранный файл не удалось прочитать как базу MpiPlatform");
        qWarning().noquote() << "[DataTransfer]" << upgraded.message;
        return r;
    }

    r.backupPath = HistoryDb::backupDatabase();
    if (r.backupPath.isEmpty()) {
        r.error = QStringLiteral("не удалось сделать резервную копию текущей базы — импорт отменён");
        return r;
    }

    QStringList createdDirs;
    bool merged = false;
    const bool opened = HistoryDb::withConnection(QStringLiteral("mpiplatform_import"), [&](QSqlDatabase& db) {
        QSqlQuery attach(db);
        attach.prepare(QStringLiteral("ATTACH DATABASE ? AS src"));
        attach.addBindValue(work);
        if (!attach.exec()) {
            r.error = attach.lastError().text();
            return;
        }

        merged = doMerge(db, dataRoot, root, r, createdDirs);

        QSqlQuery detach(db);
        detach.exec(QStringLiteral("DETACH DATABASE src"));
    });

    if (!opened && r.error.isEmpty())
        r.error = QStringLiteral("база данных недоступна");

    if (!merged) {
        // Транзакция откатана — убираем и то, что успели скопировать на диск.
        for (const QString& dir : createdDirs)
            QDir(dir).removeRecursively();
        r.sessionsAdded = r.sessionsSkipped = r.positionsAdded = r.sitesAdded = 0;
        r.foldersCopied = r.foldersRenamed = r.foldersMissing = 0;
        return r;
    }

    r.ok = true;
    return r;
}
