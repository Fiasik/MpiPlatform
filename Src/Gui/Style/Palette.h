#pragma once

#include <QColor>
#include <QHash>
#include <QString>

namespace Gui::Style {

enum class ColorScheme { Light, Dark };

// Семантические статус-цвета (не завязаны на светлую/тёмную схему — те же
// насыщенные акцентные тона читаются на обоих фонах, в отличие от базовых
// цветов интерфейса). Единый источник вместо копий одних и тех же RGB в
// разных местах (было отдельно в ToastWidget и в MainWindow —
// индикатор баланса давления использовал буквально те же три цвета).
enum class StatusKind { Info, Success, Warning, Error };
QColor statusColor(StatusKind kind);

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
