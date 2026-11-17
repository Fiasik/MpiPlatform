#include "Sheet.h"

#include <QColor>
#include <QImage>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <optional>
#include <tuple>

#include "xlsxcellrange.h"
#include "xlsxchart.h"
#include "xlsxdatavalidation.h"
#include "xlsxdocument.h"
#include "xlsxworksheet.h"

#include "Storage/ChartSnapshot.h"

using namespace QXlsx;

namespace Report::Excel {

namespace {

constexpr double kMarginColumnWidth = 1.5;  // A
constexpr double kGridColumnWidth = 8.5;    // B..P

// Excel (Calibri 11, максимальная ширина цифры 7px) переводит ширину
// столбца в символах в пиксели так — ECMA-376 §18.3.1.13.
int columnPx(double width)
{
    return int(((256.0 * width + int(128.0 / 7.0)) / 256.0) * 7.0);
}

// 96 DPI — физический размер картинки в Excel не должен зависеть от
// масштабирования экрана, с которого снят grab() графика.
constexpr int kDotsPerMeter96Dpi = 3780;
constexpr double kPxToPt = 72.0 / 96.0;
constexpr double kMaxRowHeightPt = 409.0; // предел Excel

// Ось графика с «круглыми» делениями: шаг 1, 2, 2.5 или 5 × 10^k на 4–7
// делений — как подбирает сам Excel, только без его привычки тянуть ось к
// нулю (3..21 мА -> 0..25). Начало — кратно шагу (от него идут подписи),
// конец — кратно половине шага: данные до 100,05 % дают ось до 110, а не
// 120, и выброс (перерегулирование ТШР) не обрезается. Отрицательный
// «хвост» шума датчика в пределах 1 % диапазона ноль не сдвигает (обрежется
// у края — невидимо).
struct ChartAxis {
    double min = 0.0;
    double max = 0.0;
    double step = 0.0;
    QString format;
};

std::optional<ChartAxis> chartAxis(double lo, double hi)
{
    if (!(hi > lo) || !std::isfinite(lo) || !std::isfinite(hi))
        return std::nullopt;

    const double noise = 0.01 * (hi - lo);
    if (lo < 0.0 && lo >= -noise)
        lo = 0.0;

    const double raw = (hi - lo) / 7.0;
    const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
    double step = 10.0 * magnitude;
    for (double m : {1.0, 2.0, 2.5, 5.0}) {
        if (m * magnitude >= raw) {
            step = m * magnitude;
            break;
        }
    }

    ChartAxis axis;
    axis.step = step;
    axis.min = std::floor(lo / step + 1e-9) * step;
    axis.max = std::ceil(hi / (step / 2.0) - 1e-9) * (step / 2.0);

    // Знаков после запятой — сколько нужно самому шагу (2.5 -> 1, 0.25 -> 2).
    int decimals = 0;
    while (decimals < 4 && std::abs(step * std::pow(10.0, decimals) - std::round(step * std::pow(10.0, decimals))) > 1e-6)
        ++decimals;
    axis.format = decimals == 0 ? QStringLiteral("0") : QStringLiteral("0.") + QString(decimals, QLatin1Char('0'));
    return axis;
}

QString escapeFooter(QString text)
{
    return text.replace(QLatin1Char('&'), QStringLiteral("&&"));
}

Sheet::Styles makeStyles()
{
    const QColor labelColor("#3F3F3F");

    // Размер у каждого стиля явно: шрифт без <sz> каждая программа
    // дорисовывает своим размером по умолчанию.
    Format base;
    base.setFontSize(11);
    base.setVerticalAlignment(Format::AlignVCenter);

    // Ячейка таблицы: рамка + перенос по словам.
    Format boxed = base;
    boxed.setTextWrap(true);
    boxed.setBorderStyle(Format::BorderThin);

    Sheet::Styles s;

    s.title = base;
    s.title.setFontSize(14);
    s.title.setFontBold(true);
    s.title.setTextWrap(true);

    s.titleRight = s.title;
    s.titleRight.setHorizontalAlignment(Format::AlignRight);

    s.header = boxed;
    s.header.setFontBold(true);
    s.header.setHorizontalAlignment(Format::AlignHCenter);

    s.heading = base;
    s.heading.setFontBold(true);
    s.heading.setHorizontalAlignment(Format::AlignHCenter);

    s.label = boxed;
    s.label.setFontColor(labelColor);
    s.label.setIndent(1);

    s.value = boxed;
    s.value.setIndent(1);

    s.cell = boxed;
    s.cell.setHorizontalAlignment(Format::AlignHCenter);

    // Те же цвета "уложились/не уложились в допуск", что и у шаблонного
    // Writer::coloredCell() (стандартная пара Excel "хорошо/плохо").
    s.cellOk = s.cell;
    s.cellOk.setFillPattern(Format::PatternSolid);
    s.cellOk.setPatternBackgroundColor(QColor(198, 239, 206));

    s.cellFail = s.cell;
    s.cellFail.setFillPattern(Format::PatternSolid);
    s.cellFail.setPatternBackgroundColor(QColor(255, 199, 206));

    s.text = boxed;
    s.text.setIndent(1);
    s.text.setVerticalAlignment(Format::AlignTop);

    s.caption = base;
    s.caption.setFontBold(true);

    s.underline = base;
    s.underline.setIndent(1);
    s.underline.setBottomBorderStyle(Format::BorderThin);

    return s;
}

}

const QString Sheet::kChartDataSheet = QStringLiteral("Данные графиков");

Sheet::Sheet(Document& doc, const QString& name)
    : m_styles(makeStyles())
    , m_doc(doc)
{
    doc.addSheet(name);
    doc.selectSheet(name);
    m_ws = doc.currentWorksheet();
    Q_ASSERT(m_ws);

    m_ws->setColumnWidth(1, 1, kMarginColumnWidth);
    m_ws->setColumnWidth(column(1), column(kColumns), kGridColumnWidth);

    // Масштаб не фиксированный: "вписать в 1 страницу по ширине" — Excel/
    // LibreOffice сами подбирают его под реальные ширины столбцов. Высоту
    // не ограничиваем, страницы режем сами через pageBreak().
    m_ws->setPageOrientation(QStringLiteral("portrait"));
    m_ws->setPaperSize(9); // A4
    m_ws->setFitToPage(1, 0);
    m_ws->setPageMargins(0.5, 0.5, 0.6, 0.7, 0.3, 0.3);
}

void Sheet::pageBreak()
{
    m_ws->addRowPageBreak(m_row - 1);
}

void Sheet::put(int rowFirst, int rowLast, int first, int last,
                const QVariant& value, const Format& format)
{
    const int c1 = column(first);
    const int c2 = column(last);

    if (rowFirst != rowLast || c1 != c2)
        m_ws->mergeCells(CellRange(rowFirst, c1, rowLast, c2), format);

    m_ws->write(rowFirst, c1, value, format);
}

void Sheet::dropdown(int rowFirst, int rowLast, int gridColumn, const QString& formula)
{
    if (formula.isEmpty())
        return;

    DataValidation validation(DataValidation::List, DataValidation::Equal, formula);
    validation.setErrorMessageVisible(false);
    validation.addRange(rowFirst, column(gridColumn), rowLast, column(gridColumn));
    m_ws->addDataValidation(validation);
}

double Sheet::widthPx(int first, int last) const
{
    return double(columnPx(kGridColumnWidth)) * (last - first + 1);
}

int Sheet::lineCount(int first, int last, const QString& text, double fontPt) const
{
    // Ширина символа ~ ширина цифры Calibri 11pt (7px); минус отступ
    // (indent) и поля ячейки. Ширина столбца в Excel/LibreOffice тоже
    // считается от цифры шрифта, так что оценка держится и при подмене
    // Calibri другим шрифтом.
    const double charPx = 7.0 * fontPt / 11.0;
    const int charsPerLine = std::max(1, int((widthPx(first, last) - 12.0) / charPx));

    // Перенос — по словам, как у Excel: слово, не влезающее в остаток
    // строки, целиком уходит на следующую ("выключатель/датчик" не рвётся
    // на "/"), а слово длиннее строки рвётся посимвольно.
    int lines = 0;
    for (const QString& paragraph : text.split(QLatin1Char('\n'))) {
        int count = 1;
        int used = 0;
        for (const QString& word : paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            int length = int(word.size());
            if (used > 0 && used + 1 + length <= charsPerLine) {
                used += 1 + length;
                continue;
            }
            if (used > 0)
                ++count;
            count += (length - 1) / charsPerLine;
            used = length - (length - 1) / charsPerLine * charsPerLine;
        }
        lines += count;
    }
    return std::max(1, lines);
}

void Sheet::fitHeight(int row, int first, int last, const QString& text, double fontPt)
{
    const double lineHeightPt = fontPt * 1.36;
    setMinHeight(row, lineCount(first, last, text, fontPt) * lineHeightPt + 4.0);
}

void Sheet::setMinHeight(int row, double pt)
{
    const double h = std::min(kMaxRowHeightPt, std::max(m_heights.value(row, 0.0), pt));
    m_heights[row] = h;
    m_ws->setRowHeight(row, row, h);
}

double Sheet::height(int row) const
{
    return m_heights.value(row, 15.0);
}

void Sheet::image(const QImage& source, int first, int last, double aspect)
{
    if (source.isNull())
        return;

    const int widthPx = int(Sheet::widthPx(first, last));
    const int heightPx = int(widthPx * aspect);

    QImage scaled = source.scaled(widthPx, heightPx,
                                  Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    scaled.setDotsPerMeterX(kDotsPerMeter96Dpi);
    scaled.setDotsPerMeterY(kDotsPerMeter96Dpi);

    // QXlsx кладёт row/column как есть в 0-based маркер <xdr:from> — без
    // "-1" картинка уезжает на строку и столбец ниже/правее.
    m_ws->insertImage(m_row - 1, column(first) - 1, scaled);

    // setRowHeight() молча не работает на строке без единой ячейки, а
    // картинка — оверлей, не ячейка.
    m_ws->writeBlank(m_row, column(first));
    setMinHeight(m_row, heightPx * kPxToPt);
    ++m_row;
}

bool Sheet::chart(const QVector<ChartSeries>& series, const ChartAxes& axes,
                  int first, int last, double aspect)
{
    QVector<const ChartSeries*> drawn;
    for (const ChartSeries& s : series) {
        if (!s.points.isEmpty())
            drawn.append(&s);
    }
    if (drawn.isEmpty())
        return false;

    // Лист точек — один на книгу, создаётся первым графиком. addSheet()
    // делает его текущим, но пишем мы через свои указатели, а первый лист
    // книги перед сохранением выбирает Generator.
    if (!m_doc.sheetNames().contains(kChartDataSheet)) {
        m_doc.addSheet(kChartDataSheet);
        m_doc.sheet(kChartDataSheet)->setSheetState(AbstractSheet::SS_VeryHidden);
    }
    auto* data = static_cast<Worksheet*>(m_doc.sheet(kChartDataSheet));
    const CellRange used = data->dimension();
    int col = used.isValid() ? used.lastColumn() + 1 : 1;

    const int heightPx = int(Sheet::widthPx(first, last) * aspect);
    QXlsx::Chart* chart = m_ws->insertChart(CellRange(m_row, column(first), m_row, column(last)));
    chart->setChartType(QXlsx::Chart::CT_ScatterChart);

    double xMin = qInf(), xMax = -qInf(), yMin = qInf(), yMax = -qInf();
    for (int i = 0; i < drawn.size(); ++i) {
        const ChartSeries& s = *drawn.at(i);
        // Пара столбцов на серию: X, Y; в строке 1 над Y — имя серии (из
        // него Excel берёт подпись в легенде).
        data->write(1, col, QStringLiteral("x"));
        data->write(1, col + 1, s.name);
        int row = 2;
        for (const QPointF& p : s.points) {
            const double x = p.x() * axes.xScale;
            data->write(row, col, x);
            data->write(row, col + 1, p.y());
            ++row;
            xMin = std::min(xMin, x);
            xMax = std::max(xMax, x);
            yMin = std::min(yMin, p.y());
            yMax = std::max(yMax, p.y());
        }
        chart->addSeries(CellRange(1, col, row - 1, col + 1), data,
                         /*headerH=*/true, /*headerV=*/false, /*swapHeaders=*/true);
        if (s.color.isValid())
            chart->setSeriesColor(i, s.color);
        col += 2;
    }

    chart->setSeriesLineStyle(/*showMarker=*/false, /*lineWidthPt=*/1.0);
    chart->setChartLegend(QXlsx::Chart::Top);
    chart->setGridlinesEnable(true);
    chart->setAxisTitle(QXlsx::Chart::Bottom, axes.x);
    chart->setAxisTitle(QXlsx::Chart::Left, axes.y);
    for (const auto& [pos, lo, hi] : {std::tuple{QXlsx::Chart::Bottom, xMin, xMax},
                                      std::tuple{QXlsx::Chart::Left, yMin, yMax}}) {
        if (const auto axis = chartAxis(lo, hi)) {
            chart->setAxisRange(pos, axis->min, axis->max);
            chart->setAxisMajorUnit(pos, axis->step);
            chart->setAxisNumberFormat(pos, axis->format);
        }
    }

    // Как в image(): высота строки — по графику (он оверлей, не ячейка).
    m_ws->writeBlank(m_row, column(first));
    setMinHeight(m_row, heightPx * kPxToPt);
    ++m_row;
    return true;
}

void Sheet::setFooter(const QString& left, const QString& right)
{
    // Пробел после кода кегля обязателен: "&9" + текст с цифры читается
    // как другой кегль ("&912...").
    const QString font = QStringLiteral("&\"Calibri,Regular\"&9 ");
    m_ws->setFooterText(QStringLiteral("&L%1%2&C%1Стр. &P из &N&R%1%3")
                            .arg(font, escapeFooter(left), escapeFooter(right)));
}

}
