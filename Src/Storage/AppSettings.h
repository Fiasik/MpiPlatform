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

private:
    void setValue(const QString& key, const QVariant& val);

    QSettings m_settings;
};