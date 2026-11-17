#pragma once

// Сведения о программе для окна справки (F1 / «?» в статус-баре): имя,
// версия, разработчик и контакты поддержки. Контакты — ЗАГЛУШКИ: подставьте
// настоящие здесь, больше нигде они не зашиты.

#include <QString>

namespace AppInfo {

inline QString name() { return QStringLiteral("MpiPlatform"); }

// Версия — из project(MpiPlatform VERSION ...) в корневом CMakeLists.txt
// (Src/CMakeLists.txt передаёт её библиотеке Gui как MPIPLATFORM_VERSION).
inline QString version()
{
#ifdef MPIPLATFORM_VERSION
    return QStringLiteral(MPIPLATFORM_VERSION);
#else
    return QStringLiteral("—");
#endif
}

// Та же строка, что в подвале PDF-актов.
inline QString company() { return QStringLiteral("ООО «РДК Системс»"); }

// ---- Поддержка (заглушки) ----
inline QString supportName() { return QStringLiteral("Фамилия Имя Отчество"); }
inline QString supportPhone() { return QStringLiteral("+7 (000) 000-00-00"); }
inline QString supportEmail() { return QStringLiteral("support@example.com"); }

}
