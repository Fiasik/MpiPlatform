#include "ToleranceBar.h"

#include <QFontMetricsF>
#include <QPainter>

#include <algorithm>
#include <limits>

namespace Widgets {

namespace {
constexpr int kMarginX = 6;
// Не по центру виджета: сверху нужен запас под капсулу маркера промаха и
// подпись дельты над ней (см. paintEvent) — снизу столько не нужно
// (только тик/зона), поэтому центр сдвинут вниз, а не увеличена высота
// симметрично.
constexpr int kCenterY = 26;
constexpr int kTrackHeight = 4;
constexpr int kZoneHeight = 10;
constexpr int kTickHalfHeight = 7;
constexpr qreal kMarkerWidth = 6.0;
constexpr qreal kMarkerHalfHeight = 9.0;

// Трек/метки границ — нейтральные, перекрашиваются под тему через
// applyPalette() (m_trackColor/m_tickColor). Зона/маркер — нет: тот же
// зелёный/красный в обеих темах. kOkColor — не произвольный зелёный, а
// ровно тот же Gui::Style::passFillColor() ("Good"-заливка Excel), что
// уже используется в приложении для той же семантики — TelemetryUiMapper.cpp
// красит им ячейку "соответствует" (Report/Excel/Blocks/ResponseTable.cpp —
// тоже он, но своей копией: Report не может зависеть от Gui, см. комментарий
// у passFillColor()). Один цвет на обе роли (полупрозрачная заливка зоны и
// непрозрачный маркер факта) — чтобы не плодить второй, отдельный оттенок
// зелёного только ради этого виджета.
const QColor kOkColor = Gui::Style::passFillColor();
const QColor kFailColor = Gui::Style::statusColor(Gui::Style::StatusKind::Error);
// 0x33 (~20%) сливался с треком почти неотличимо на реальном экране —
// проверено по пикселям скриншота: зона рисовалась в правильном месте,
// просто без заметного контраста. 0x70 (~44%) уже видно с одного взгляда.
QColor zoneTintColor()
{
    QColor tint = kOkColor;
    tint.setAlpha(0x70);
    return tint;
}
// kOkColor сам по себе очень светлый — на тёмной теме полупрозрачная зона
// (см. выше) всё равно заметно темнее его, контраста маркеру хватает. На
// светлой теме зона на 44% альфы уже почти вплотную подходит к белизне
// самого kOkColor — проверено по пикселям (разница осталась 16/255 против
// 82/255 на тёмной), маркер сливался бы с зоной. Не заводим вторую,
// отдельную краску под маркер — просто чуть более насыщенный вариант того
// же C6EFCE, того же оттенка, что и зона.
QColor okMarkerColor()
{
    return kOkColor.darker(125);
}
}

ToleranceBar::ToleranceBar(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ToleranceBar::setZone(double lower, double upper, double value,
                            CrossingStatus::State state, double miss, bool boundedBelow)
{
    setZones({Zone{lower, upper, value, miss, state, boundedBelow}});
}

void ToleranceBar::setZones(const QVector<Zone>& zones)
{
    m_zones = zones;
    update();
}

void ToleranceBar::clear()
{
    setZones({});
}

void ToleranceBar::applyPalette(const Gui::Style::Palette& palette)
{
    m_trackColor = QColor(palette.borderSubtle);
    m_tickColor = QColor(palette.textStrong);
    update();
}

QSize ToleranceBar::sizeHint() const
{
    return QSize(160, 40);
}

QSize ToleranceBar::minimumSizeHint() const
{
    return QSize(60, 40);
}

void ToleranceBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const int trackLeft = kMarginX;
    const int trackRight = width() - kMarginX;
    const int trackWidth = trackRight - trackLeft;
    if (trackWidth <= 0)
        return;

    // Трек — сплошная линия во всю ширину всегда, даже без данных (иначе
    // выключенная проверка выглядела бы дырой в панели).
    p.setPen(Qt::NoPen);
    p.setBrush(m_trackColor);
    p.drawRoundedRect(
        QRectF(trackLeft, kCenterY - kTrackHeight / 2.0, trackWidth, kTrackHeight), 2, 2);

    if (m_zones.isEmpty())
        return;

    // Общая ось на все зоны сразу — нужно пружине (два независимых допуска
    // на одном треке). Раньше отступ по краям был фиксированным процентом
    // от диапазона всегда — из-за этого, например, у "Динамической ошибки"
    // (граница снизу жёстко 0, значение туда физически попасть не может,
    // std::abs() в Analyzer'е) слева появлялась пустая полоса без всякого
    // смысла: там нечему было бы поместиться. Теперь отступ большой только
    // с той стороны, где точка РЕАЛЬНО вышла за допуск (нужно место под
    // неё и подпись промаха) — с другой стороны только на глаз, не гоняясь
    // за данными, которых там нет.
    double rawMin = std::numeric_limits<double>::max();
    double rawMax = std::numeric_limits<double>::lowest();
    bool overflowsBelow = false;
    bool overflowsAbove = false;
    // Настоящая ли граница у той зоны, что задаёт rawMin по z.lower (не по
    // z.value — переполнение ниже уже покрыто overflowsBelow/bigPad ниже).
    // Нужно, чтобы решить, оставлять ли слева маленький отступ или прижимать
    // зону вплотную к краю трека — см. следующий блок.
    bool rawMinFromRealLowerBound = true;
    for (const Zone& z : std::as_const(m_zones)) {
        if (z.lower < rawMin) {
            rawMin = z.lower;
            rawMinFromRealLowerBound = z.boundedBelow;
        }
        rawMin = std::min(rawMin, z.value);
        rawMax = std::max({rawMax, z.upper, z.value});
        overflowsBelow = overflowsBelow || z.value < z.lower;
        overflowsAbove = overflowsAbove || z.value > z.upper;
    }
    if (!(rawMax > rawMin)) {
        rawMin -= 1.0;
        rawMax += 1.0;
    }
    const double range = rawMax - rawMin;
    const double bigPad = range * 0.12;
    const double smallPad = range * 0.02;
    // Нет тика — нечего отделять отступом: зона прижимается к самому краю
    // трека, а не "висит" в воздухе с пустым полем слева (та же идея, что
    // и у подавления тика ниже, см. z.boundedBelow).
    const double lowPad =
        overflowsBelow ? bigPad : (rawMinFromRealLowerBound ? smallPad : 0.0);
    const double axisMin = rawMin - lowPad;
    const double axisMax = rawMax + (overflowsAbove ? bigPad : smallPad);

    const auto toX = [&](double v) {
        return trackLeft + (v - axisMin) / (axisMax - axisMin) * trackWidth;
    };

    for (const Zone& z : std::as_const(m_zones)) {
        const double xLower = toX(z.lower);
        const double xUpper = toX(z.upper);

        // Зона — "островок" заметно выше самого трека (не вровень с ним),
        // как на исходном референсе: полупрозрачная плашка со скруглением
        // в половину высоты, а не узкая полоска по линии трека.
        p.setBrush(zoneTintColor());
        p.drawRoundedRect(
            QRectF(xLower, kCenterY - kZoneHeight / 2.0, xUpper - xLower, kZoneHeight),
            kZoneHeight / 2.0, kZoneHeight / 2.0);

        // Границы зоны — контрастные "палочки" (цвет — m_tickColor, из
        // applyPalette), а не одна метка по центру: именно границы
        // допуска, а не номинал, несут решение "прошло/не прошло". Нижняя
        // рисуется, только если это настоящий предел (z.boundedBelow) —
        // иначе (0 для дин. ошибки/линейности) обособлять там нечего.
        p.setBrush(m_tickColor);
        if (z.boundedBelow)
            p.drawRect(QRectF(xLower - 1, kCenterY - kTickHalfHeight, 2, kTickHalfHeight * 2));
        p.drawRect(QRectF(xUpper - 1, kCenterY - kTickHalfHeight, 2, kTickHalfHeight * 2));

        // Факт — скруглённая "капсула" (не кружок и не такая же тонкая
        // палочка, как у границ допуска: иначе промах вплотную к границе
        // визуально сливался бы с её меткой). Непрозрачная — на фоне
        // полупрозрачной зоны того же цвета это само по себе даёт
        // контраст, без второго, отдельного оттенка зелёного.
        const double xValue = toX(z.value);
        const QColor markerColor =
            z.state == CrossingStatus::State::Fail ? kFailColor : okMarkerColor();
        p.setBrush(markerColor);
        p.drawRoundedRect(
            QRectF(xValue - kMarkerWidth / 2.0, kCenterY - kMarkerHalfHeight,
                   kMarkerWidth, kMarkerHalfHeight * 2.0),
            kMarkerWidth / 2.0, kMarkerWidth / 2.0);

        if (z.state == CrossingStatus::State::Fail && !qIsNaN(z.miss)) {
            const QString text =
                (z.miss >= 0.0 ? QStringLiteral("+") : QString())
                + QString::number(z.miss, 'f', 2);

            QFont font = p.font();
            font.setPointSizeF(qMax(8.0, font.pointSizeF() - 1.0));

            const QFontMetricsF fm(font);
            const double labelWidth = fm.horizontalAdvance(text) + 4.0;
            double labelX = xValue - labelWidth / 2.0;
            labelX = qBound(0.0, labelX, double(width()) - labelWidth);

            const QRectF labelRect(
                labelX, kCenterY - kMarkerHalfHeight - 2.0 - fm.height(), labelWidth, fm.height());

            p.setFont(font);
            p.setPen(kFailColor);
            p.drawText(labelRect, Qt::AlignHCenter | Qt::AlignBottom, text);
            p.setPen(Qt::NoPen);
        }
    }
}

}
