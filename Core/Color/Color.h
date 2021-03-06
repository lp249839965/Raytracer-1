#pragma once

#include "../RayLib.h"
#include "../Config.h"
#include "../Math/Vector8.h"

namespace rt {

namespace math
{
    class Random;
}


// Represents ray wavelength(s), randomized for primary rays
struct Wavelength
{
    static constexpr float Lower = 0.380e-6f;
    static constexpr float Higher = 0.720e-6f;

#ifdef RT_ENABLE_SPECTRAL_RENDERING
    static constexpr Uint32 NumComponents = 8;
    using ValueType = math::Vector8;
#else
    static constexpr Uint32 NumComponents = 4;
    using ValueType = math::Vector4;
#endif

    ValueType value;

    bool isSingle = false;

    RAYLIB_API void Randomize(math::Random& rng);

    RT_FORCE_INLINE float GetBase() const
    {
        return value[0];
    }
};


// Represents a ray color/weight during raytracing
// The color values corresponds to wavelength values.
struct Color
{
    Wavelength::ValueType value;

    RT_FORCE_INLINE Color() = default;
    RT_FORCE_INLINE Color(const Color& other) = default;
    RT_FORCE_INLINE Color& operator = (const Color& other) = default;

    RT_FORCE_INLINE explicit Color(const float val) : value(val) { }
    RT_FORCE_INLINE explicit Color(const Wavelength::ValueType& val) : value(val) { }

    RT_FORCE_INLINE static const Color Zero()
    {
        return Color{ Wavelength::ValueType::Zero() };
    }

    RT_FORCE_INLINE static const Color One()
    {
#ifdef RT_ENABLE_SPECTRAL_RENDERING
        return Color{ math::VECTOR8_ONE };
#else
        return Color{ math::VECTOR_ONE };
#endif
    }

    RT_FORCE_INLINE static const Color SingleWavelengthFallback()
    {
#ifdef RT_ENABLE_SPECTRAL_RENDERING
        return Color{ math::Vector8(8.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f) };
#else
        return Color{ math::VECTOR_ONE };
#endif
    }

    RT_FORCE_INLINE const Color operator + (const Color& other) const
    {
        return Color{ value + other.value };
    }

    RT_FORCE_INLINE const Color operator * (const Color& other) const
    {
        return Color{ value * other.value };
    }

    RT_FORCE_INLINE const Color operator * (const float factor) const
    {
        return Color{ value * factor };
    }

    RT_FORCE_INLINE const Color operator / (const float factor) const
    {
        return Color{ value / factor };
    }

    RT_FORCE_INLINE Color& operator += (const Color& other)
    {
        value += other.value;
        return *this;
    }

    RT_FORCE_INLINE Color& operator *= (const Color& other)
    {
        value *= other.value;
        return *this;
    }

    RT_FORCE_INLINE Color& operator *= (const float factor)
    {
        value *= factor;
        return *this;
    }

    RT_FORCE_INLINE bool AlmostZero() const
    {
        return Wavelength::ValueType::AlmostEqual(value, Wavelength::ValueType::Zero());
    }

    RT_FORCE_INLINE Float Max() const
    {
        return value.HorizontalMax()[0];
    }

    RT_FORCE_INLINE bool IsValid() const
    {
#ifdef RT_ENABLE_SPECTRAL_RENDERING
        return value.IsValid() && Wavelength::ValueType::GreaterEqMask(value, Wavelength::ValueType()) == 0xFF;
#else
        return value.IsValid() && (value >= Wavelength::ValueType::Zero()).All();
#endif
    }

    RT_FORCE_INLINE static const Color Lerp(const Color& a, const Color& b, const float factor)
    {
        return Color{ Wavelength::ValueType::Lerp(a.value, b.value, factor) };
    }

    // calculate ray color values for given wavelength and linear RGB values
    static const Color BlackBody(const Wavelength& wavelength, const float temperature);

    // calculate ray color values for given wavelength and linear RGB values
    static const Color SampleRGB(const Wavelength& wavelength, const math::Vector4& rgbValues);

    // convert to CIE XYZ tristimulus values
    // NOTE: when spectral rendering is disabled, this function does nothing
    const math::Vector4 Resolve(const Wavelength& wavelength) const;
};

RT_FORCE_INLINE const Color operator * (const float a, const Color& b)
{
    return b * a;
}

} // namespace rt
