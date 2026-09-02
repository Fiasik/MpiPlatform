#include "Writer.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace Report::Csv {

namespace {

QString csvField(const QString& value)
{
    if (!value.contains(',') && !value.contains('"') && !value.contains('\n'))
        return value;

    QString escaped = value;
    escaped.replace('"', QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

// 'g' с 15 значащими цифрами — без потери точности double при печати
// (round-trip safe), но и без научной нотации на типичных для теста
// значениях (мА/мм/бар/% — все в разумном диапазоне).
QString csvNumber(qreal v)
{
    return QString::number(v, 'g', 15);
}

}

bool writeChartCsv(const QString& path, const ChartSnapshot& snapshot)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        qWarning() << "Report::Csv::writeChartCsv: не удалось открыть на запись" << path;
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    out << "series,x,y\n";
    for (const ChartSeries& series : snapshot.series) {
        const QString name = csvField(series.name);
        for (const QPointF& point : series.points)
            out << name << ',' << csvNumber(point.x()) << ',' << csvNumber(point.y()) << '\n';
    }

    return true;
}

}
