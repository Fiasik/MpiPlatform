#pragma once

#include <QColor>
#include <QHash>
#include <QString>

#include "Storage/Telemetry.h"

namespace Gui::Style {

enum class ColorScheme { Light, Dark };

// Семантические статус-цвета (не завязаны на светлую/тёмную схему — те же
// насыщенные акцентные тона читаются на обоих фонах, в отличие от базовых
// цветов интерфейса). Единый источник вместо копий одних и тех же RGB в
// разных местах (было отдельно в ToastWidget и в MainWindow —
// индикатор баланса давления использовал буквально те же три цвета).
enum class StatusKind { Info, Success, Warning, Error };
QColor statusColor(StatusKind kind);

// То же самое, но для CrossingStatus::State (допуск пройден/не пройден/не
// проверялся) — другой enum (не StatusKind: "не проверялся" это не уровень
// важности тоста, а отдельный визуальный статус, смешивать не стали), но
// Ok/Fail внутри буквально statusColor(Success)/statusColor(Error) — один
// источник. Раньше были независимые копии в CrossingIndicatorsPresenter.cpp
// и HistoryPresenter.cpp (с разошедшимся цветом для Unknown: "#333333" vs
// "#6B7280" — ни тот, ни другой файл сам по себе не был неправ, но здесь
// наконец один источник правды).
QColor crossingStateColor(CrossingStatus::State state);
QString crossingStateLabel(CrossingStatus::State state);

// Светлая пастельная "заливка ячейки" — Excel-стиль conditional formatting
// "Good"/"Bad" (не завязаны на тему, те же пастельные тона что в Excel,
// умышленно другие оттенки, чем statusColor() выше — это фон, а не текст/
// маркер). Было по независимой копии этих же RGB в TelemetryUiMapper.cpp и
// Widgets::ToleranceBar.cpp; Report/Excel/Blocks/ResponseTable.cpp — тоже
// копия тех же чисел, но её сюда перевести не вышло: Report не может
// зависеть от Gui (Gui и так зависит от Report, см. CMakeLists.txt) — там
// и осталась отдельным литералом, с явной отсылкой на эту пару в комментарии.
QColor passFillColor();
QColor failFillColor();

// Значения для каждого токена @{...}, используемого в Src/Style/theme.qss.
// Светлые значения — 1:1 то, что раньше было литералами прямо в файле
// (обязаны остаться визуально идентичными). Тёмные подобраны с тем же
// относительным порядком контрастности, что и светлые, просто на тёмной
// шкале — see ThemeManager::renderTemplate() для механики подстановки.
struct Palette {
    QString bgDialog;
    QString bgRecessed;
    QString bgSurface;
    QString bgHover;
    QString bgDisabled;

    QString borderDefault;
    QString borderSubtle;
    QString borderHover;

    QString textBase;
    QString textStrong;
    QString textMuted;
    QString textMuted2;
    QString textDisabledTab;

    QString accentPrimary;
    QString accentHover;
    QString accentPressed;
    QString accentSelectedInactive;

    QString iconChevronDown;
    QString iconChevronUp;
    QString iconChevronDownActive;
    QString iconChevronUpActive;
    QString iconChevronLeft;
    QString iconChevronRight;

    static const Palette& forScheme(ColorScheme scheme);

    // Имя токена (без "@{}") -> значение, для подстановки в шаблон.
    QHash<QString, QString> tokens() const;
};

}
