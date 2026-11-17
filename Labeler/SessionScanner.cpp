#include "SessionScanner.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>

namespace Labeler {

const QStringList& chartKinds()
{
    static const QStringList kinds = {
        QStringLiteral("Task"),
        QStringLiteral("Friction"),
        QStringLiteral("Pressure"),
    };
    return kinds;
}

QString SessionInfo::chartsSummary() const
{
    QStringList parts;
    for (const QString& kind : chartKinds()) {
        if (charts.contains(kind))
            parts << QStringLiteral("%1:%2").arg(kind).arg(charts.value(kind).size());
    }
    return parts.join(QLatin1Char(','));
}

namespace {

struct ChartFileName
{
    QString kind;
    int index = 0;
    QString fileName;
};

// "Task_12.data" -> {Task, 12}. Остальное карточка не показывает: тест
// полного хода (Stroke) и Main/Step/Response/Resolution — не основные тесты,
// нестандартные имена (климатика "02 -40.data", старые "График задания
// (1).data") — нет единого смысла.
bool parseChartFileName(const QString& fileName, ChartFileName* out)
{
    static const QRegularExpression re(
        QStringLiteral("^(Task|Friction|Pressure)_(\\d+)\\.data$"));
    const QRegularExpressionMatch m = re.match(fileName);
    if (!m.hasMatch())
        return false;
    out->kind = m.captured(1);
    out->index = m.captured(2).toInt();
    out->fileName = fileName;
    return true;
}

SessionInfo buildSession(const QDir& rootDir, const QDir& dir, QVector<ChartFileName> files)
{
    SessionInfo info;
    info.relPath = rootDir.relativeFilePath(dir.absolutePath());
    if (info.relPath == QStringLiteral("."))
        info.relPath.clear();

    std::sort(files.begin(), files.end(), [](const ChartFileName& a, const ChartFileName& b) {
        return a.kind != b.kind ? a.kind < b.kind : a.index < b.index;
    });

    QCryptographicHash sessionHash(QCryptographicHash::Sha1);
    QMap<QString, QList<QByteArray>> seenPerKind;

    for (const ChartFileName& f : files) {
        QFile file(dir.filePath(f.fileName));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QByteArray bytes = file.readAll();

        sessionHash.addData(f.fileName.toUtf8());
        sessionHash.addData(QByteArrayView("\0", 1));
        sessionHash.addData(bytes);

        // Дубль по содержимому (оператор сохранил тот же график второй раз)
        // в карточку не идёт — переключатель прогонов показывал бы два
        // одинаковых графика.
        const QByteArray digest = QCryptographicHash::hash(bytes, QCryptographicHash::Sha1);
        QList<QByteArray>& seen = seenPerKind[f.kind];
        if (seen.contains(digest))
            continue;
        seen << digest;
        info.charts[f.kind] << f.fileName;
    }

    info.filesHash = QString::fromLatin1(sessionHash.result().toHex());

    const QStringList xlsx = dir.entryList({QStringLiteral("*.xlsx")}, QDir::Files, QDir::Name);
    for (const QString& name : xlsx) {
        if (name.compare(QStringLiteral("report.xlsx"), Qt::CaseInsensitive) == 0) {
            info.hasReport = true;
            continue;
        }
        // "~$..." — временный файл открытого в Excel документа.
        if (name.startsWith(QStringLiteral("~$")))
            continue;
        if (info.valveCardFile.isEmpty())
            info.valveCardFile = name;
    }

    return info;
}

}

QVector<SessionInfo> scanSessions(const QString& rootPath, const std::function<void(int)>& progress)
{
    const QDir rootDir(rootPath);

    // Один проход по всем .data архива, группировка по папке.
    QMap<QString, QVector<ChartFileName>> filesByDir;
    QDirIterator it(rootPath, {QStringLiteral("*.data")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ChartFileName parsed;
        if (parseChartFileName(it.fileName(), &parsed))
            filesByDir[it.fileInfo().absolutePath()] << parsed;
    }

    QVector<SessionInfo> sessions;
    sessions.reserve(filesByDir.size());
    for (auto i = filesByDir.cbegin(); i != filesByDir.cend(); ++i) {
        SessionInfo info = buildSession(rootDir, QDir(i.key()), i.value());
        if (!info.charts.isEmpty())
            sessions << info;
        if (progress)
            progress(int(sessions.size()));
    }

    std::sort(sessions.begin(), sessions.end(), [](const SessionInfo& a, const SessionInfo& b) {
        return a.relPath < b.relPath;
    });
    return sessions;
}

}
