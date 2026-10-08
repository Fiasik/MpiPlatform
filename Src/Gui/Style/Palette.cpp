#include "Palette.h"

#include <QObject>

namespace Gui::Style {

namespace {

Palette lightPalette()
{
    Palette p;
    p.bgDialog = QStringLiteral("#E2E7EB");
    p.bgRecessed = QStringLiteral("#EDF1F4");
    p.bgSurface = QStringLiteral("#FFFFFF");
    p.bgHover = QStringLiteral("#F0F2F5");
    p.bgDisabled = QStringLiteral("#F7F8FA");

    p.borderDefault = QStringLiteral("#B9C0CB");
    p.borderSubtle = QStringLiteral("#E2E6EC");
    p.borderHover = QStringLiteral("#94A0B1");

    p.textBase = QStringLiteral("#333333");
    p.textStrong = QStringLiteral("#161A20");
    p.textMuted = QStringLiteral("#6B7280");
    p.textMuted2 = QStringLiteral("#5B6472");
    p.textDisabledTab = QStringLiteral("#C7CDD6");

    p.accentPrimary = QStringLiteral("#2563EB");
    p.accentHover = QStringLiteral("#1D4ED8");
    p.accentPressed = QStringLiteral("#1E40AF");
    p.accentSelectedInactive = QStringLiteral("#D9E0EF");

    p.iconChevronDown = QStringLiteral(":/Src/Img/ChevronDown.svg");
    p.iconChevronUp = QStringLiteral(":/Src/Img/ChevronUp.svg");
    p.iconChevronDownActive = QStringLiteral(":/Src/Img/ChevronDownActive.svg");
    p.iconChevronUpActive = QStringLiteral(":/Src/Img/ChevronUpActive.svg");
    p.iconChevronLeft = QStringLiteral(":/Src/Img/ChevronLeft.svg");
    p.iconChevronRight = QStringLiteral(":/Src/Img/ChevronRight.svg");
    return p;
}

// Направление hover/pressed у акцента развёрнуто относительно светлой темы
// (светлее по наведению/нажатию, а не темнее) — это не рассинхрон, а
// правильное направление для читаемости на тёмном фоне.
Palette darkPalette()
{
    Palette p;
    p.bgDialog = QStringLiteral("#15171B");
    p.bgRecessed = QStringLiteral("#1C1F24");
    p.bgSurface = QStringLiteral("#2E333C");
    p.bgHover = QStringLiteral("#242830");
    p.bgDisabled = QStringLiteral("#282C34");

    p.borderDefault = QStringLiteral("#454C59");
    p.borderSubtle = QStringLiteral("#333944");
    p.borderHover = QStringLiteral("#6B7280");

    p.textBase = QStringLiteral("#D6D9DE");
    p.textStrong = QStringLiteral("#F3F4F6");
    p.textMuted = QStringLiteral("#8B93A0");
    p.textMuted2 = QStringLiteral("#9AA3B0");
    p.textDisabledTab = QStringLiteral("#4B5160");

    p.accentPrimary = QStringLiteral("#3B82F6");
    p.accentHover = QStringLiteral("#60A5FA");
    p.accentPressed = QStringLiteral("#93C5FD");
    p.accentSelectedInactive = QStringLiteral("#1E2A44");

    p.iconChevronDown = QStringLiteral(":/Src/Img/ChevronDownDark.svg");
    p.iconChevronUp = QStringLiteral(":/Src/Img/ChevronUpDark.svg");
    p.iconChevronDownActive = QStringLiteral(":/Src/Img/ChevronDownActiveDark.svg");
    p.iconChevronUpActive = QStringLiteral(":/Src/Img/ChevronUpActiveDark.svg");
    p.iconChevronLeft = QStringLiteral(":/Src/Img/ChevronLeftDark.svg");
    p.iconChevronRight = QStringLiteral(":/Src/Img/ChevronRightDark.svg");
    return p;
}

}

const Palette& Palette::forScheme(ColorScheme scheme)
{
    static const Palette kLight = lightPalette();
    static const Palette kDark = darkPalette();
    return scheme == ColorScheme::Dark ? kDark : kLight;
}

QHash<QString, QString> Palette::tokens() const
{
    return {
        {QStringLiteral("bgDialog"), bgDialog},
        {QStringLiteral("bgRecessed"), bgRecessed},
        {QStringLiteral("bgSurface"), bgSurface},
        {QStringLiteral("bgHover"), bgHover},
        {QStringLiteral("bgDisabled"), bgDisabled},

        {QStringLiteral("borderDefault"), borderDefault},
        {QStringLiteral("borderSubtle"), borderSubtle},
        {QStringLiteral("borderHover"), borderHover},

        {QStringLiteral("textBase"), textBase},
        {QStringLiteral("textStrong"), textStrong},
        {QStringLiteral("textMuted"), textMuted},
        {QStringLiteral("textMuted2"), textMuted2},
        {QStringLiteral("textDisabledTab"), textDisabledTab},

        {QStringLiteral("accentPrimary"), accentPrimary},
        {QStringLiteral("accentHover"), accentHover},
        {QStringLiteral("accentPressed"), accentPressed},
        {QStringLiteral("accentSelectedInactive"), accentSelectedInactive},

        {QStringLiteral("iconChevronDown"), iconChevronDown},
        {QStringLiteral("iconChevronUp"), iconChevronUp},
        {QStringLiteral("iconChevronDownActive"), iconChevronDownActive},
        {QStringLiteral("iconChevronUpActive"), iconChevronUpActive},
        {QStringLiteral("iconChevronLeft"), iconChevronLeft},
        {QStringLiteral("iconChevronRight"), iconChevronRight},
    };
}

QColor statusColor(StatusKind kind)
{
    switch (kind) {
    case StatusKind::Info:    return QColor(70, 130, 220);
    case StatusKind::Success: return QColor(56, 174, 96);
    case StatusKind::Warning: return QColor(230, 160, 40);
    case StatusKind::Error:   return QColor(214, 69, 69);
    }
    return QColor(120, 120, 120);
}

QColor crossingStateColor(CrossingStatus::State state)
{
    switch (state) {
    case CrossingStatus::State::Ok:      return statusColor(StatusKind::Success);
    case CrossingStatus::State::Fail:    return statusColor(StatusKind::Error);
    case CrossingStatus::State::Unknown: return QColor(0x6B, 0x72, 0x80); // = textMuted выше
    }
    return QColor(0x6B, 0x72, 0x80);
}

QString crossingStateLabel(CrossingStatus::State state)
{
    switch (state) {
    case CrossingStatus::State::Ok:      return QObject::tr("соответствует");
    case CrossingStatus::State::Fail:    return QObject::tr("не соответствует");
    case CrossingStatus::State::Unknown: return QString();
    }
    return QString();
}

QColor passFillColor() { return QColor(198, 239, 206); }
QColor failFillColor() { return QColor(255, 199, 206); }

}
