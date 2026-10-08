#include "TimeUtils.h"

#include <QTime>

namespace TimeUtils {

QString msToText(qint64 ms)
{
    return QTime(0, 0).addMSecs(static_cast<int>(ms)).toString(QStringLiteral("mm:ss.zzz"));
}

quint32 textToMs(const QString& text)
{
    const QTime time = QTime::fromString(text.trimmed(), QStringLiteral("mm:ss.zzz"));

    if (!time.isValid())
        return 0;

    return static_cast<quint32>(QTime(0, 0).msecsTo(time));
}

}
