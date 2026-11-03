#include "SessionFolders.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

QString SessionFolders::appRoot()
{
    return QCoreApplication::applicationDirPath();
}

QString SessionFolders::toStored(const QString& absolutePath, const QString& root)
{
    const QString clean = QDir::cleanPath(QFileInfo(absolutePath).absoluteFilePath());
    const QString relative = QDir(root).relativeFilePath(clean);

    // relativeFilePath() для другого диска/корня возвращает абсолютный
    // путь, для папки снаружи — путь с "..": в обоих случаях хранить
    // относительный вид нельзя.
    if (QDir::isAbsolutePath(relative) || relative == QLatin1String("..")
        || relative.startsWith(QLatin1String("../")))
        return clean;

    return relative;
}

QString SessionFolders::resolve(const QString& stored, const QString& root)
{
    if (stored.isEmpty())
        return QString();
    if (QDir::isAbsolutePath(stored))
        return QDir::cleanPath(stored);
    return QDir::cleanPath(QDir(root).filePath(stored));
}

bool SessionFolders::isSafeRelative(const QString& stored)
{
    if (stored.isEmpty() || QDir::isAbsolutePath(stored) || stored.contains(QLatin1Char('\\'))
        || stored.contains(QLatin1Char(':')))
        return false;

    const QStringList parts = stored.split(QLatin1Char('/'));
    for (const QString& part : parts) {
        if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String(".."))
            return false;
    }
    return true;
}
