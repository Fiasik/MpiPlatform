#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace Labeler {

// Три основных теста, которые показывает карточка: задание, трение,
// давление (в порядке рамок окна). Имена — ровно префиксы файлов
// "<Тип>_<номер>.data", которые пишет Report::Saver (и старый MpiSki —
// формат тот же).
const QStringList& chartKinds();

// Одна сессия архива = одна папка, в которой лежит хотя бы один график
// стандартного типа (см. chartKinds()). report.xlsx не обязателен: повторные
// прогоны того же дня (папки "дд_мм_гггг_2" и т.п.) часто без него.
struct SessionInfo
{
    // Путь папки сессии относительно корня, через '/'.
    QString relPath;
    // Тип графика -> имена .data-файлов этого типа БЕЗ побайтных дублей
    // (Task_2 часто точная копия Task_1), по возрастанию номера. Карточка
    // по умолчанию показывает последний — последний прогон.
    QMap<QString, QStringList> charts;
    // sha1 по всем стандартным .data папки (имя + содержимое): по нему видно,
    // что графики сессии поменялись после разметки.
    QString filesHash;
    // Имя файла карточки клапана "<позиция>.xlsx" в той же папке или пусто.
    QString valveCardFile;
    bool hasReport = false;

    // "Task:2,Friction:1,Pressure:1" — сколько разных прогонов каждого типа.
    QString chartsSummary() const;
};

// Обходит корень архива (только чтение) и собирает сессии. Читает все
// стандартные .data ради хэша и поиска дублей — на архиве ~3,6 ГБ это
// секунды-десятки секунд, поэтому окно зовёт его из фонового потока.
// progress(n) — сколько папок-сессий уже собрано.
QVector<SessionInfo> scanSessions(const QString& rootPath,
                                  const std::function<void(int)>& progress = {});

}
