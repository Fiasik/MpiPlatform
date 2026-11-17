#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QShortcut>
#include <QHash>

// Горячие клавиши вкладок. Скрытые вкладки (setTabVisible(false) — например,
// «Циклический тест» вне сборки Ski) в нумерацию и в переход стрелками не
// попадают: цифра N — N-я ВИДИМАЯ вкладка, как их видит оператор.
class TabBinder
{
public:
    static void bindNumbers(QWidget* parent, QTabWidget* tw)
    {
        // Видимость может поменяться после привязки, поэтому вкладка
        // ищется в момент нажатия, а клавиши заводятся все девять.
        for (int n = 0; n < 9; ++n) {
            auto* sc = new QShortcut(QKeySequence(Qt::Key_1 + n), parent);
            sc->setContext(Qt::ApplicationShortcut);

            QObject::connect(sc, &QShortcut::activated, parent, [tw, n] {
                if (!tw)
                    return;
                const int i = visibleTabIndex(tw, n);
                if (i >= 0 && tw->isTabEnabled(i))
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

    // Индекс n-й (с нуля) видимой вкладки; -1 — столько видимых нет.
    static int visibleTabIndex(const QTabWidget* tw, int n)
    {
        for (int i = 0; i < tw->count(); ++i) {
            if (!tw->isTabVisible(i))
                continue;
            if (n-- == 0)
                return i;
        }
        return -1;
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

    // Следующая по кругу вкладка, на которую можно перейти: скрытые и
    // заблокированные (до инициализации) пропускаются, как и при щелчке.
    static void switchTab(QTabWidget* tw, int dir)
    {
        if (!tw) return;

        const int n = tw->count();
        int i = tw->currentIndex();

        for (int step = 0; step < n; ++step) {
            i = (i + dir + n) % n;
            if (tw->isTabVisible(i) && tw->isTabEnabled(i)) {
                tw->setCurrentIndex(i);
                return;
            }
        }
    }
};
