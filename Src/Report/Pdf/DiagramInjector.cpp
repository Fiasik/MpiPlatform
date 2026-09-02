#include "DiagramInjector.h"

#include <QDebug>

namespace Report::Pdf {

namespace {

QDomElement findFieldByName(const QDomElement& root, const QString& fieldName)
{
    if (root.tagName() == QLatin1String("TContainerField") && root.attribute("name") == fieldName)
        return root;

    QDomNode child = root.firstChild();
    while (!child.isNull()) {
        const QDomElement childElement = child.toElement();
        if (!childElement.isNull()) {
            const QDomElement found = findFieldByName(childElement, fieldName);
            if (!found.isNull())
                return found;
        }
        child = child.nextSibling();
    }
    return QDomElement();
}

// (1,1,1), а не буквально (0,0,0): движок молча заменяет ряд на цвет из
// своей темы, если QPen ряда совпадает с QPen() по умолчанию (а он по
// умолчанию чёрный) — тот же обход, что и в report_common.py::APP_COLORS
// ("setpoint"), уже проверенный десятками рендеров в спайке.
QString toGraphColor(const QColor& color)
{
    int r = color.red();
    int g = color.green();
    int b = color.blue();
    if (r == 0 && g == 0 && b == 0)
        r = g = b = 1;
    return QStringLiteral("rgba(%1,%2,%3,%4)").arg(r).arg(g).arg(b).arg(color.alpha());
}

}

void injectSeries(QDomDocument& doc, const QString& fieldName, const QVector<ChartSeries>& series)
{
    QDomElement field = findFieldByName(doc.documentElement(), fieldName);
    if (field.isNull()) {
        qWarning() << "Report::Pdf::injectSeries: diagram-поле не найдено в шаблоне:" << fieldName;
        return;
    }

    QDomNode child = field.firstChild();
    while (!child.isNull()) {
        const QDomNode next = child.nextSibling();
        if (child.toElement().tagName() == QLatin1String("graph"))
            field.removeChild(child);
        child = next;
    }

    for (int i = 0; i < series.size(); ++i) {
        const ChartSeries& s = series.at(i);
        QDomElement graph = doc.createElement(QStringLiteral("graph"));
        graph.setAttribute(QStringLiteral("caption"), s.name);
        graph.setAttribute(QStringLiteral("color"), toGraphColor(s.color));
        graph.setAttribute(QStringLiteral("graphDS"), QStringLiteral("ds%1").arg(i + 1));

        for (const QPointF& point : s.points) {
            QDomElement value = doc.createElement(QStringLiteral("value"));
            value.setAttribute(QStringLiteral("x"), point.x());
            value.setAttribute(QStringLiteral("y"), point.y());
            graph.appendChild(value);
        }

        field.appendChild(graph);
    }
}

}
