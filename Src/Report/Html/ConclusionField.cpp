#include "ConclusionField.h"

namespace Report::Html {

QString titleFor(ConclusionField field)
{
    switch (field) {
    case ConclusionField::SeatingZone:
        return QStringLiteral("Зона посадки плунжер/седло");
    case ConclusionField::PositionerAdaptation:
        return QStringLiteral("Адаптация/детали адаптации позиционера");
    case ConclusionField::AuxiliaryEquipment:
        return QStringLiteral("Состояние навесного оборудования");
    case ConclusionField::FrictionGraph:
        return QStringLiteral("График трения хода клапана");
    case ConclusionField::GlandLeak:
        return QStringLiteral("Наличие течи по сальнику");
    }
    return {};
}

QString dbKeyFor(ConclusionField field)
{
    switch (field) {
    case ConclusionField::SeatingZone:
        return QStringLiteral("seatingZone");
    case ConclusionField::PositionerAdaptation:
        return QStringLiteral("positionerAdaptation");
    case ConclusionField::AuxiliaryEquipment:
        return QStringLiteral("auxiliaryEquipment");
    case ConclusionField::FrictionGraph:
        return QStringLiteral("frictionGraph");
    case ConclusionField::GlandLeak:
        return QStringLiteral("glandLeak");
    }
    return {};
}

}
