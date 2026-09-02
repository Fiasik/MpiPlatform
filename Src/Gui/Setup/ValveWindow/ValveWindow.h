#pragma once

#include <QDialog>
#include <QList>
#include <QStringList>
#include "Storage/Registry.h"
#include "Gui/Setup/SelectTests.h"
#include "Storage/AppSettings.h"

class Mapper;

namespace Ui {
class ValveWindow;
}

class ValveWindow : public QDialog
{
    Q_OBJECT

    friend class Mapper;

public:
    explicit ValveWindow(QWidget *parent = nullptr);
    ~ValveWindow() = default;

    void setRegistry(Registry *registry);
    void setPatternType(SelectTests::PatternType pattern);

private:
    Ui::ValveWindow *ui;

    Registry *m_registry;
    AppSettings m_settings;
    ValveInfo m_local;

    MaterialsOfComponentParts m_materialsLocal;
    void readMaterialsFromUi(MaterialsOfComponentParts& m);
    void loadMaterialsToUi(const MaterialsOfComponentParts& m);

    void onDriveTypeChanged(int index);

    void loadLinearRange();
    void saveLinearRange();

    void updateManufacturerUi();
    void setupWindowGeometry();
    void bindTabShortcut(int key, QWidget* tab);
    void setupTabs();
    void setupValidators();
    void setupConnections();
    void setupInitialUi();

    // Цвета кривых/датчиков (tab_colors) — видна только в Ilim-варианте
    // (Domain::kIlimVariant), но сам код не под if constexpr: логика
    // безвредна и в Standard, просто живёт на скрытой вкладке — тот же
    // подход, что и к response/resolution вкладкам в MainWindow.
    void setupColorPickers();
    void updateColorSwatches();

    static constexpr const char* kManualInput = "Ручной ввод";
    QList<QString> m_diameter = {
        QStringLiteral("50.0"),
        QStringLiteral("86.0"),
        QStringLiteral("108.0"),
        QStringLiteral("125.0")
    };

    SelectTests::PatternType m_patternType = SelectTests::Pattern_None;

    void saveValveInfo();
    void applyPatternVisibility();
    void applyFrictionLimitsFromStuffingBoxSeal();

    // Диаметр привода/шкива, диапазон пружины, ход клапана — поля, без
    // которых часть результатов теста считается неверно (0 вместо
    // реального значения), но никак не видно оператору, что именно не
    // заполнено (см. случай с driveDiameter=0 -> frictionForce=0.000).
    // Красная подсветка лейбла + подсказка обновляется вживую по мере
    // ввода, а не только при попытке закрыть окно.
    void updateCriticalFieldWarnings();
    QStringList missingCriticalFields() const;

private slots:
    void onPositionerTypeChanged(quint8 index);
    void positionChanged(const QString &position);
    void strokeChanged(quint16 n);
    void toolChanged(quint16 n);
    void on_pushButton_netWindow_clicked();
    void on_pushButton_clear_clicked();
};
