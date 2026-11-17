#include "AppSettings.h"

#include <QCoreApplication>

#include <algorithm>

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

QString AppSettings::historyArchiveRoot() const
{
    return m_settings.value("History/archiveRoot").toString();
}

void AppSettings::setHistoryArchiveRoot(const QString& path)
{
    setValue("History/archiveRoot", path);
}

QString AppSettings::chartDisplayOption(const QString& chart, const QString& name) const
{
    return m_settings.value(QStringLiteral("Charts/%1/%2").arg(chart, name)).toString();
}

void AppSettings::setChartDisplayOption(const QString& chart, const QString& name, const QString& value)
{
    setValue(QStringLiteral("Charts/%1/%2").arg(chart, name), value);
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

QDate AppSettings::sensorsCheckedDate() const
{
    return QDate::fromString(m_settings.value("Maintenance/sensorsCheckedDate").toString(), Qt::ISODate);
}

void AppSettings::setSensorsCheckedDate(const QDate& date)
{
    setValue("Maintenance/sensorsCheckedDate", date.toString(Qt::ISODate));
}

QDate AppSettings::sensorsRemindAfter() const
{
    return QDate::fromString(m_settings.value("Maintenance/sensorsRemindAfter").toString(), Qt::ISODate);
}

void AppSettings::setSensorsRemindAfter(const QDate& date)
{
    setValue("Maintenance/sensorsRemindAfter", date.toString(Qt::ISODate));
}

int AppSettings::sensorsCheckIntervalDays() const
{
    return std::max(1, m_settings.value("Maintenance/sensorsCheckIntervalDays", 60).toInt());
}
