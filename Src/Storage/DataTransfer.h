#pragma once

#include <QString>

// Перенос данных между компьютерами/установками: всё содержимое БД истории
// плюс папки сессий с графиками (.png/.data) и отчётами.
//
// Пакет экспорта — обычная папка MpiPlatform_export_<дата>/:
//     mpiplatform.db   целостная копия базы (VACUUM INTO)
//     data/...         папки сессий с теми же относительными путями, что в
//                      sessions.folder_path (корень — папка приложения)
//     manifest.json    версия формата и схемы, время, счётчики
//
// Импорт принимает пакет экспорта ИЛИ целую установку приложения (папка с
// Database/mpiplatform.db и папками сессий рядом) ИЛИ одиночный .db: чужая
// база сначала приводится к актуальной версии схемы во временной копии,
// затем сливается в рабочую (новые id; сессия, у которой "позиция + время"
// уже есть в рабочей базе, пропускается — с учётом количества одинаковых).
// Рабочая база перед этим копируется в Database/backup/; импорт целиком в
// одной транзакции — при ошибке база и скопированные папки откатываются.
namespace DataTransfer {

struct ExportResult {
    bool ok = false;
    QString error;
    QString packagePath;      // созданная папка пакета
    int sessions = 0;         // сессий в базе пакета
    int foldersCopied = 0;
    int foldersMissing = 0;   // у "живых" сессий папка указана, но на диске её нет

    QString summary() const;
};

struct ImportResult {
    bool ok = false;
    QString error;
    QString backupPath;
    int sitesAdded = 0;       // новых связок объект/цех/отделение
    int positionsAdded = 0;
    int sessionsAdded = 0;
    int sessionsSkipped = 0;  // такая позиция + время уже есть в базе
    int foldersCopied = 0;
    int foldersRenamed = 0;   // имя папки совпало с существующей -> добавлен суффикс _N
    int foldersMissing = 0;

    QString summary() const;
};

// targetParentDir — куда положить папку пакета. appRoot — корень, от которого
// считаются пути папок сессий (по умолчанию папка приложения; параметр для
// тестов).
ExportResult exportAll(const QString& targetParentDir, const QString& appRoot = QString());

ImportResult importFrom(const QString& sourcePath, const QString& appRoot = QString());

}
