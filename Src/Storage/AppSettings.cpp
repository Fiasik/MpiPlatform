#include "AppSettings.h"

#include <QCoreApplication>

AppSettings::AppSettings()
    : m_settings(
          QCoreApplication::applicationDirPath() + "/settings.ini",
          QSettings::IniFormat) {}

void AppSettings::setValue(const QString& key, const QVariant& val) {
    m_settings.setValue(key,val);
}

std::pair<double, double> AppSettings::linearRange() const
{
    double min = m_settings.value("Sensors/Linear/min", 0).toDouble();
    double max = m_settings.value("Sensors/Linear/max", 50).toDouble();

    return {min, max};
}

void AppSettings::setLinearRange(double min, double max)
{
    setValue("Sensors/Linear/min", min);
    setValue("Sensors/Linear/max", max);
}

// Строкой, а не int — чтобы порядок enum'а можно было менять, не рискуя
// молча съехать на другое значение в уже сохранённых settings.ini.
ThemePreference AppSettings::themePreference() const
{
    const QString value = m_settings.value("Appearance/theme", "System").toString();
    if (value == QLatin1String("Light"))
        return ThemePreference::Light;
    if (value == QLatin1String("Dark"))
        return ThemePreference::Dark;
    return ThemePreference::System;
}

void AppSettings::setThemePreference(ThemePreference pref)
{
    const char* value = "System";
    if (pref == ThemePreference::Light)
        value = "Light";
    else if (pref == ThemePreference::Dark)
        value = "Dark";
    setValue("Appearance/theme", QString::fromLatin1(value));
}
