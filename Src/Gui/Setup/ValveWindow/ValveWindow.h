#pragma once

#include <QDialog>
#include <QList>
#include <QStringList>
#include "Storage/Registry.h"
#include "Domain/Tests/Pattern.h"
#include "Storage/AppSettings.h"

class Mapper;
class QComboBox;
class QLineEdit;

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
    void setPatternType(Domain::Tests::PatternType pattern);

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

    // Справочник оборудования (Storage/EquipmentCatalog, таблицы catalog_*):
    // производители ЗРА и модели позиционеров/приводов. Модели — строго
    // выбранного производителя, у позиционеров ещё и по коэффициенту из
    // comboBox_dinamicError. В каждом
    // списке последним пунктом "Ручной ввод" с полем справа, как у
    // производителя.
    void reloadManufacturers();
    void reloadPositionerModels();
    void reloadDriveModels();
    // Производитель для поиска моделей: при "Ручном вводе" — пусто
    // (введённого вручную производителя в справочнике ещё нет, моделей у
    // него нет — списки сразу встают на "Ручной ввод").
    QString catalogManufacturer() const;
    void fillModelCombo(QComboBox* combo, QLineEdit* manual, const QStringList& models);
    void updateModelManualUi(QComboBox* combo, QLineEdit* manual);
    void setModelValue(QComboBox* combo, QLineEdit* manual, const QString& value);
    static QString modelValue(const QComboBox* combo, const QLineEdit* manual);
    // Пополняет справочник тем, что оператор сам впечатал в поля "Ручной
    // ввод" в этом окне (QLineEdit::isModified()). Значения, подставленные
    // из старой карточки, не добавляются — иначе в списки разом попал бы
    // весь накопленный в истории свободный текст.
    void saveManualEntriesToCatalog();

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

    Domain::Tests::PatternType m_patternType = Domain::Tests::PatternType::None;

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
