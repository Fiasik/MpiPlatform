#pragma once

#include <QString>
#include <QSqlDatabase>
#include <functional>

// Единая точка доступа к БД истории (Database/mpiplatform.db): путь к файлу,
// версия схемы с вшитыми миграциями, открытие соединений. Раньше
// databasePath() и обвязка открытия были продублированы по HistoryWriter/
// HistoryReader/Gui::HistoryPresenter независимо.
namespace HistoryDb {

enum class SchemaStatus {
    Ok,        // версия схемы уже актуальна
    Created,   // файла не было (или он пустой) — создан с нуля
    Migrated,  // существующая база доведена до актуальной версии
    TooNew,    // база создана более новым приложением — не трогаем
    Failed     // не удалось открыть/обновить — база осталась как была
};

struct SchemaResult {
    SchemaStatus status = SchemaStatus::Failed;
    int fromVersion = 0;   // версия в файле до запуска (0 — пустой файл или база без версии)
    int toVersion = 0;     // версия после
    QString message;       // человекочитаемо, для диалога при TooNew/Failed
    QString backupPath;    // пусто, если копия не делалась
};

inline bool isUsable(SchemaStatus s)
{
    return s == SchemaStatus::Ok || s == SchemaStatus::Created || s == SchemaStatus::Migrated;
}

// Порядок поиска файла:
//  1) переменная окружения MPIPLATFORM_DB — явный путь (тесты, разработка);
//  2) <папка exe>/Database/mpiplatform.db, если уже есть;
//  3) Database/mpiplatform.db в дереве исходников, если оно существует на
//     этой машине (рабочий процесс разработчика: полная БД лежит там);
//  4) иначе <папка exe>/Database/mpiplatform.db — будет создан при первом
//     запуске из вшитой схемы.
// Считается один раз за процесс.
QString databasePath();

// Создаёт файл БД из вшитой схемы, если его нет, и доводит существующий до
// актуальной версии (PRAGMA user_version + миграции из database.qrc). Перед
// миграцией существующей базы делает копию в Database/backup/ (хранятся
// последние несколько). Выполняется один раз за процесс, результат
// кэшируется; withConnection()/openPersistent() зовут её сами, так что любой
// доступ к БД защищён, а main.cpp вызывает её явно в начале, чтобы показать
// пользователю причину, если TooNew/Failed.
const SchemaResult& ensureSchema();

// Актуальная версия схемы (номер последней вшитой миграции).
int latestSchemaVersion();

// Доводит ПРОИЗВОЛЬНЫЙ файл БД (например, временную копию чужой базы при
// импорте) до актуальной версии — той же цепочкой миграций, но без копии в
// backup/ и без кэширования. Чужие ограничения те же: файл новее приложения
// -> TooNew, файл не тронут.
SchemaResult upgradeFile(const QString& path);

// Копия рабочей БД в Database/backup/ (хранятся последние несколько) —
// перед рискованными операциями вроде импорта. Пустая строка — не удалось.
QString backupDatabase();

// Для одноразовых запросов (HistoryWriter/HistoryReader: открыть — сделать
// один запрос или транзакцию — закрыть). connectionName должен быть
// уникальным на вызов — withConnection сам гарантирует db.close() +
// QSqlDatabase::removeDatabase(). Возвращает false, если схема непригодна
// (TooNew/Failed) или соединение не открылось (fn() в этом случае не
// вызывается); иначе — true (сам факт открытия, не успех запроса внутри
// fn — тот результат caller забирает через захваченную по ссылке переменную).
bool withConnection(const QString& connectionName, const std::function<void(QSqlDatabase&)>& fn);

// Для долгоживущего соединения (Gui::HistoryPresenter — один connectionName
// на весь презентер). Не открывает и не закрывает db — только находит/заводит
// именованное соединение и выставляет databaseName. Если схема непригодна —
// возвращает невалидный QSqlDatabase, и db.open() у вызывающего просто
// не удастся.
QSqlDatabase openPersistent(const QString& connectionName);

// Пара к openPersistent() — используется в деструкторе владельца
// долгоживущего соединения.
void removePersistent(const QString& connectionName);

}
