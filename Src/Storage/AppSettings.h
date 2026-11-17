#pragma once

#include <QSettings>

enum class ThemePreference { System, Light, Dark };

class AppSettings
{
public:
    AppSettings();

    std::pair<double,double> linearRange() const;
    void setLinearRange(double min,double max);

    ThemePreference themePreference() const;
    void setThemePreference(ThemePreference pref);

    // Корень архива испытаний на ЭТОЙ машине (папка, где лежат
    // <объект>/<производство>/...): от него «История» ищет графики сессий,
    // чей путь в БД записан относительно другой папки (перенесённый архив,
    // импорт). Пусто — не задан.
    QString historyArchiveRoot() const;
    void setHistoryArchiveRoot(const QString& path);

    // Настройки отображения графика (меню ⚙ на графике): ключ
    // "Charts/<chart>/<name>", значение — строкой (что именно — решает
    // Widgets::Chart::Manager; здесь только хранение, без типов Widgets).
    // Пусто — не сохранено, берётся значение по умолчанию.
    QString chartDisplayOption(const QString& chart, const QString& name) const;
    void setChartDisplayOption(const QString& chart, const QString& name, const QString& value);

private:
    void setValue(const QString& key, const QVariant& val);

    QSettings m_settings;
};