#pragma once

#include <QString>

namespace Domain {

enum class Variant { Standard, Ilim, Ski };

inline constexpr Variant kVariant =
#if defined(MPIPLATFORM_VARIANT_ILIM)
    Variant::Ilim;
#elif defined(MPIPLATFORM_VARIANT_SKI)
    Variant::Ski;
#else
    Variant::Standard;
#endif

inline constexpr bool kIlimVariant = (kVariant == Variant::Ilim);
inline constexpr bool kSkiVariant = (kVariant == Variant::Ski);

inline QString VariantToString(Variant v)
{
    switch (v) {
    case Variant::Ilim: return QStringLiteral("Ilim");
    case Variant::Ski: return QStringLiteral("Ski");
    case Variant::Standard: return QStringLiteral("Standard");
    }
    return QStringLiteral("Standard");
}

}
