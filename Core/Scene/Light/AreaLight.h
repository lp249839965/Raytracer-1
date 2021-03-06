#pragma once

#include "Light.h"

namespace rt {

class Bitmap;
using BitmapPtr = std::shared_ptr<Bitmap>;

class RAYLIB_API AreaLight : public ILight
{
public:
    AreaLight(const math::Vector4& p0, const math::Vector4& edge0, const math::Vector4& edge1, const math::Vector4& color);

    virtual const math::Box GetBoundingBox() const override;
    virtual bool TestRayHit(const math::Ray& ray, Float& outDistance) const override;
    virtual const Color Illuminate(IlluminateParam& param) const override;
    const math::Vector4 GetNormal(const math::Vector4& hitPoint) const override;
    virtual const Color GetRadiance(RenderingContext& context, const math::Vector4& rayDirection, const math::Vector4& hitPoint, Float* outDirectPdfA) const override;
    virtual bool IsFinite() const override final;
    virtual bool IsDelta() const override final;

    BitmapPtr mTexture = nullptr;

private:
    math::Vector4 p0;
    math::Vector4 edge0;
    math::Vector4 edge1;
    math::Vector4 normal;

    Float invArea; // inverted surface area
    Float edgeLengthInv0;
    Float edgeLengthInv1;

    bool isTriangle = false;
};

} // namespace rt
