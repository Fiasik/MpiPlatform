#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QShortcut>
#include <QHash>

class TabBinder
{
public:
    static void bindNumbers(QWidget* parent, QTabWidget* tw)
    {
        for (int i = 0; i < tw->count() && i < 9; ++i) {
            auto* sc = new QShortcut(QKeySequence(Qt::Key_1 + i), parent);
            sc->setContext(Qt::ApplicationShortcut);

            QObject::connect(sc, &QShortcut::activated, parent, [tw, i] {
                if (tw && tw->isTabEnabled(i))
                    tw->setCurrentIndex(i);
            });
        }
    }

    static void bindArrowNavigation(QWidget* parent, QTabWidget* mainTabs,
                                    const QHash<QWidget*, QTabWidget*>& innerTabs)
    {
        auto* left = new QShortcut(QKeySequence(Qt::Key_Left), parent);
        auto* right = new QShortcut(QKeySequence(Qt::Key_Right), parent);

        left->setContext(Qt::ApplicationShortcut);
        right->setContext(Qt::ApplicationShortcut);

        QObject::connect(left, &QShortcut::activated, parent, [=] {
            if (auto* tw = currentTabWidget(mainTabs, innerTabs))
                switchTab(tw, -1);
        });

        QObject::connect(right, &QShortcut::activated, parent, [=] {
            if (auto* tw = currentTabWidget(mainTabs, innerTabs))
                switchTab(tw, +1);
        });
    }

private:
    static QTabWidget*
    currentTabWidget(QTabWidget* mainTabs, const QHash<QWidget*, QTabWidget*>& innerTabs)
    {
        QWidget* top = mainTabs->currentWidget();

        if (innerTabs.contains(top))
            return innerTabs.value(top);

        return mainTabs;
    }

    static void switchTab(QTabWidget* tw, int dir)
    {
        if (!tw) return;

        int i = tw->currentIndex();
        int n = tw->count();

        tw->setCurrentIndex((i + dir + n) % n);
    }
};