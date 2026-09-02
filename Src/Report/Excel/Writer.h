#pragma once

#include <QString>
#include <QVariant>
#include <QColor>

#include "Data.h"

namespace Report::Excel {
    class Writer {
    public:
        explicit Writer(ReportData& report);

        void cell(const QString& sheet, quint16 row, quint16 col, const QVariant& value);
        void image(const QString& sheet, quint16 row, quint16 col, const QImage& img);

        // Временно не вызывается (см. ImageCell/ChartCell в Data.h) — оставлен
        // рабочим на будущее.
        void chart(const QString& sheet, quint16 anchorRow, quint16 anchorCol,
                   const QString& title, const QString& xTitle, const QString& yTitle,
                   const QVector<ChartSeries>& series);
        void validation(const QString& sheet, const QString& formula, const QString& range);

        // Как cell(), но с заливкой фона — используется только в таблицах
        // Response/Resolution (подсветка "уложились/не уложились в допуск"
        // по каждому шагу).
        void coloredCell(const QString& sheet, quint16 row, quint16 col,
                         const QVariant& value, const QColor& backgroundColor);

    private:
        ReportData& m_report;
    };
}
