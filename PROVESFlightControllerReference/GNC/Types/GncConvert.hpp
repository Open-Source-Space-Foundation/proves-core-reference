/**
 * \file GncConvert.hpp
 * \brief  Conversions between Astrolib and Fprime types used by GNC.
 *
 * \details AstroLib is FPrime free, and FPP generates its own types.
 * Conversions between the two are frequent and necessary in GNC components,
 * so they are shared here.
 */
#ifndef Gnc_Types_GncConvert_HPP
#define Gnc_Types_GncConvert_HPP

#include "PROVESFlightControllerReference/GNC/AstroLib/AstroLib.hpp"
#include "PROVESFlightControllerReference/GNC/Types/IlluminationStateEnumAc.hpp"
#include "PROVESFlightControllerReference/GNC/Types/Vec3dSerializableAc.hpp"
#include "PROVESFlightControllerReference/GNC/Types/Vec3fSerializableAc.hpp"

namespace Gnc {

/**
 * Astrolib F64 vector -> FPP F64 vector. No loss of precision.
 */
inline Gnc::Vec3d toVec3d(const Astro::Vec3& v) {
    return Gnc::Vec3d(v.x, v.y, v.z);
}

/**
 *  FPP F64 vector -> Astrolib F64 vector. No loss of precision.
 */
inline Astro::Vec3 toAstro(const Gnc::Vec3d& v) {
    return Astro::Vec3{v.get_x(), v.get_y(), v.get_z()};
}

/**
 * Astrolib F64 vector -> FPP F32 vector.
 *
 * The F64 -> F32 narrowing point for the GNC subsystem.
 */
inline Gnc::Vec3f toVec3f(const Astro::Vec3& v) {
    return Gnc::Vec3f(static_cast<F32>(v.x), static_cast<F32>(v.y), static_cast<F32>(v.z));
}

/**
 * AstroLib illumination enum -> FPP illumination enum.
 */
inline Gnc::IlluminationState toFpp(Astro::Illumination i) {
    switch (i) {
        case Astro::Illumination::UMBRA:
            return Gnc::IlluminationState::UMBRA;
        case Astro::Illumination::PENUMBRA:
            return Gnc::IlluminationState::PENUMBRA;
        default:
            return Gnc::IlluminationState::SUNLIT;
    }
}

}  // namespace Gnc

#endif  // Gnc_Types_GncConvert_HPP
