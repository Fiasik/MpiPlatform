#pragma once

#include <QString>

// Папка сессии (графики .png/.data, report.xlsx) создаётся Report::Saver
// рядом с exe: <объект>/<производство>/<цех>/<позиция>/дд_ММ_гггг[_N]. В БД
// (sessions.folder_path) путь хранится ОТНОСИТЕЛЬНО папки приложения, с "/"
// в качестве разделителя, — тогда базу и папки можно перенести на другой
// компьютер вместе; если папка выбрана вручную вне папки приложения,
// хранится абсолютный путь.
namespace SessionFolders {

// Корень, от которого считаются пути: папка с exe.
QString appRoot();

// Абсолютный путь папки -> вид для БД.
QString toStored(const QString& absolutePath, const QString& root = appRoot());

// Вид из БД -> абсолютный путь (абсолютные остаются как есть).
QString resolve(const QString& stored, const QString& root = appRoot());

// true — путь можно безопасно приклеить к корню: относительный, без ".."
// и без пустых сегментов. Чужая база (импорт) не должна уметь писать за
// пределы папки приложения.
bool isSafeRelative(const QString& stored);

}
