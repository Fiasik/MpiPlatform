#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHash>

struct TabActions
{
    QPushButton* primary = nullptr;
    QPushButton* secondary = nullptr;
};

class TabActionRouter
{
public:

    void bindPrimary(QWidget* tab, QPushButton* button)
    {
        m_actions[tab].primary = button;
    }

    void bindSecondary(QWidget* tab, QPushButton* button)
    {
        m_actions[tab].secondary = button;
    }

    void triggerPrimary(QWidget* tab)
    {
        if (!tab) return;

        auto it = m_actions.find(tab);
        if (it != m_actions.end() && it.value().primary)
            it.value().primary->click();
    }

    void triggerSecondary(QWidget* tab)
    {
        if (!tab) return;

        auto it = m_actions.find(tab);
        if (it != m_actions.end() && it.value().secondary)
            it.value().secondary->click();
    }

private:

    QHash<QWidget*, TabActions> m_actions;
};