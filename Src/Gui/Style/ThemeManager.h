#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <Qt>

#include "Gui/Style/Palette.h"
#include "Storage/AppSettings.h"

class QApplication;

namespace Gui::Style {

// Заменяет собой прежнюю локальную loadTheme() из main.cpp: грузит
// Src/Style/theme.qss (шаблон с токенами @{...}) с диска, подставляет
// палитру нужной схемы (Palette::forScheme()) и вызывает
// QApplication::setStyleSheet(). Сохраняет прежний hot-reload на
// сохранение файла (QFileSystemWatcher, re-add-path — некоторые редакторы
// сохраняют через атомарную замену).
//
// Схема резолвится из ThemePreference (System/Light/Dark, персистится
// через AppSettings): System подхватывает системную QGuiApplication::
// styleHints()->colorScheme() и живо следит за её сменой; явный выбор
// (toggle()/setPreference(Light|Dark)) перебивает и с этого момента
// игнорирует смену системной темы — до следующего запуска с тем же
// сохранённым выбором.
class ThemeManager : public QObject
{
    Q_OBJECT
public:
    explicit ThemeManager(QApplication& app, QObject* parent = nullptr);

    ColorScheme currentScheme() const { return m_resolvedScheme; }
    const Palette& currentPalette() const { return Palette::forScheme(m_resolvedScheme); }
    ThemePreference preference() const { return m_preference; }

    // Чистая функция без I/O — системная схема передаётся параметром, а не
    // читается изнутри через styleHints(), специально чтобы её можно было
    // прогнать с синтетическими значениями (диагностический харнесс/тесты)
    // без реального переключения ОС.
    static ColorScheme resolve(ThemePreference pref, Qt::ColorScheme osScheme);

public slots:
    // Сохраняет через AppSettings и переприменяет тему немедленно.
    void setPreference(ThemePreference pref);
    // Явный Light<->Dark от текущей схемы — никогда не возвращает System.
    void toggle();

signals:
    void schemeChanged(Gui::Style::ColorScheme newScheme);

private:
    void resolveAndApply();
    void applyStylesheet();
    // QSS красит только виджеты, за которые "отвечает" явное правило —
    // нативно рисующиеся элементы (кастомные QWidget-наследники вроде
    // Widgets::Slider::SliderView, которые берут цвет из palette(), плюс
    // системные вещи типа нативных диалогов/скроллбаров) читают
    // QApplication::palette(), которую QSS не трогает вообще. Без этого
    // они остаются на светлой Fusion-палитре по умолчанию в тёмной теме.
    void applyWidgetPalette();
    static QString renderTemplate(const QString& qssTemplate, const Palette& palette);

    QApplication& m_app;
    AppSettings m_settings;
    ThemePreference m_preference = ThemePreference::System;
    ColorScheme m_resolvedScheme = ColorScheme::Light;
    QFileSystemWatcher m_watcher;
};

}
