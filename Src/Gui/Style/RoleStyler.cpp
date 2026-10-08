#include "RoleStyler.h"

#include <QFrame>
#include <QLabel>
#include <QSet>
#include <QString>
#include <QStyle>
#include <QWidget>

namespace Gui::Style {

namespace {

// unpolish()+polish() обязательны: Qt не перечитывает QSS для нового
// динамического свойства сам по себе, только по явному сигналу стилю.
template<typename Widget>
void applyRoleBySuffix(QWidget* root, const QSet<QString>& suffixes)
{
    for (Widget* widget : root->findChildren<Widget*>()) {
        const QString suffix = widget->objectName().section(QLatin1Char('_'), -1);
        if (!suffixes.contains(suffix))
            continue;

        widget->setProperty("role", suffix.toLower());
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
}

// "_Value"-лейблы, которые обновляются на каждый sample с устройства (см.
// TelemetryUiMapper::updateSensorReadings) — текущие показания датчиков,
// не результат. Выделять их для копирования бессмысленно: значение
// перезапишется на следующем тике, выделение всё равно пропадёт.
const QSet<QString>& liveSensorValueNames()
{
    static const QSet<QString> names = {
        QStringLiteral("label_sensorLinear_Value"),
        QStringLiteral("label_sensorLinearPercent_Value"),
        QStringLiteral("label_pressureSensor_1_Value"),
        QStringLiteral("label_pressureSensor_2_Value"),
        QStringLiteral("label_pressureSensor_3_Value"),
        QStringLiteral("label_calcPressureBalancePercent_Value"),
        QStringLiteral("label_sensorFeedback_Value"),
    };
    return names;
}

// Промахи crossing-limits (label_..._Miss, см. CrossingIndicatorsPresenter)
// — тоже результат, но по суффиксу не "Value", отдельным списком.
const QStringList& extraSelectableResultNames()
{
    static const QStringList names = {
        QStringLiteral("label_crossingLimits_coefficientFriction_Miss"),
        QStringLiteral("label_crossingLimits_linearCharacteristic_Miss"),
        QStringLiteral("label_crossingLimits_dynamicError_Miss"),
        QStringLiteral("label_crossingLimits_spring_Miss"),
        QStringLiteral("label_crossingLimits_range_Miss"),
    };
    return names;
}

// Результаты (не текущие показания датчиков — см. liveSensorValueNames())
// можно выделить и скопировать: число из отчёта/лога часто нужно вставить
// куда-то ещё, а не просто посмотреть.
void makeResultLabelsSelectable(QWidget* root)
{
    for (QLabel* label : root->findChildren<QLabel*>()) {
        const bool isValueLabel =
            label->objectName().section(QLatin1Char('_'), -1) == QStringLiteral("Value");
        const bool isExtra = extraSelectableResultNames().contains(label->objectName());

        if (!isValueLabel && !isExtra)
            continue;

        if (liveSensorValueNames().contains(label->objectName()))
            continue;

        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    }
}

}

void applyRoles(QWidget* root)
{
    static const QSet<QString> kLabelRoleSuffixes = {
        QStringLiteral("Title"), QStringLiteral("Value"), QStringLiteral("Unit"),
        QStringLiteral("DeltaSign"), QStringLiteral("DeltaValue"), QStringLiteral("DateMono"),
        QStringLiteral("Norm"),
    };
    static const QSet<QString> kFrameRoleSuffixes = {
        QStringLiteral("Card"), QStringLiteral("Section"),
    };

    applyRoleBySuffix<QLabel>(root, kLabelRoleSuffixes);
    applyRoleBySuffix<QFrame>(root, kFrameRoleSuffixes);
    makeResultLabelsSelectable(root);
}

}
