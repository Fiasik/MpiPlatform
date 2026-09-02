#pragma once

namespace Domain {

enum class Variant { Standard, Ilim, Ski };

inline constexpr Variant kVariant =
#if defined(MPISDK_VARIANT_ILIM)
    Variant::Ilim;
#elif defined(MPISDK_VARIANT_SKI)
    Variant::Ski;
#else
    Variant::Standard;
#endif

inline constexpr bool kIlimVariant = (kVariant == Variant::Ilim);
inline constexpr bool kSkiVariant = (kVariant == Variant::Ski);

}
