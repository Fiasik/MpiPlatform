#pragma once

#include <QDialog>
#include "Storage/Registry.h"

class QShowEvent;

namespace Ui {
class ObjectWindow;
}

class ObjectWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ObjectWindow(QWidget *parent = nullptr);
    void LoadFromReg(Registry *registry);
    ~ObjectWindow() override;

protected:
    void showEvent(QShowEvent *event) override;

private:
    Ui::ObjectWindow *ui = nullptr;
    Registry *m_registry = nullptr;

    // Гасит реакцию на currentIndexChanged, пока список пересобирается
    // программно (иначе каскад ушёл бы в рекурсию).
    bool m_repopulating = false;

    // На весь экран разворачиваем только при первом показе — иначе
    // повторные showEvent() (напр. после сворачивания/восстановления)
    // маскировали бы то, что пользователь сам уменьшил окно.
    bool m_fullScreenOnce = false;

    void saveToRegistry();

    void reloadManufactories();
    void reloadDepartments();

    void refreshHistoryPanel();
    void showPositionCard(const QString &position);

private slots:
    void ButtonClick();
};
