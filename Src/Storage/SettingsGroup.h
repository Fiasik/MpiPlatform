#pragma once

#include <QSettings>

class SettingsGroup
{
public:

    SettingsGroup(QSettings& settings, const QString& group)
        : m_settings(settings)
    {
        m_settings.beginGroup(group);
    }

    ~SettingsGroup()
    {
        m_settings.endGroup();
    }

private:

    QSettings& m_settings;
};