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

}

void applyRoles(QWidget* root)
{
    static const QSet<QString> kLabelRoleSuffixes = {
        QStringLiteral("Title"), QStringLiteral("Value"), QStringLiteral("Unit"),
    };
    static const QSet<QString> kFrameRoleSuffixes = {
        QStringLiteral("Card"), QStringLiteral("Section"),
    };

    applyRoleBySuffix<QLabel>(root, kLabelRoleSuffixes);
    applyRoleBySuffix<QFrame>(root, kFrameRoleSuffixes);
}

}
