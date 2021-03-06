#include "PCH.h"
#include "AreaLight.h"
#include "../../Rendering/Context.h"
#include "../../Rendering/ShadingData.h"
#include "../../Math/Geometry.h"
#include "../../Utils/Bitmap.h"

namespace rt {

using namespace math;

AreaLight::AreaLight(const Vector4& p0, const Vector4& edge0, const Vector4& edge1, const Vector4& color)
    : ILight(color)
    , p0(p0)
    , edge0(edge0)
    , edge1(edge1)
{
    RT_ASSERT(p0.IsValid());
    RT_ASSERT(edge0.IsValid());
    RT_ASSERT(edge1.IsValid());

    edgeLengthInv0 = 1.0f / edge0.Length3();
    edgeLengthInv1 = 1.0f / edge1.Length3();

    const Vector4 cross = Vector4::Cross3(edge1, edge0);
    normal = cross.Normalized3();

    Float surfaceArea = cross.Length3();
    if (isTriangle)
    {
        surfaceArea *= 0.5f;
    }

    invArea = 1.0f / surfaceArea;
}

const Box AreaLight::GetBoundingBox() const
{
    Box box(p0, p0 + edge0, p0 + edge1);
    if (!isTriangle)
    {
        box.AddPoint(p0 + edge0 + edge1);
    }

    return box;
}

bool AreaLight::TestRayHit(const math::Ray& ray, Float& outDistance) const
{
    Float u, v; // unused

    if (Intersect_TriangleRay(ray, p0, edge0, edge1, u, v, outDistance))
    {
        return true;
    }

    if (!isTriangle)
    {
        const Vector4 oppositePoint = p0 + edge0 + edge1;

        if (Intersect_TriangleRay(ray, oppositePoint, -edge0, -edge1, u, v, outDistance))
        {
            return true;
        }
    }

    return false;
}

const Color AreaLight::Illuminate(IlluminateParam& param) const
{
    const Float2 uv = isTriangle ? param.context.randomGenerator.GetTriangle() : param.context.randomGenerator.GetFloat2();

    Vector4 rgbColor = mColor;

    // sample texture map
    if (mTexture)
    {
        rgbColor *= mTexture->Sample(Vector4(uv), SamplerDesc());
    }

    // p0 + edge0 * uv.x + edge1 * uv.y;
    const Vector4 lightPoint = Vector4::MulAndAdd(edge0, uv.x, Vector4::MulAndAdd(edge1, uv.y, p0));

    param.outDirectionToLight = lightPoint - param.shadingData.position;
    const float sqrDistance = param.outDirectionToLight.SqrLength3();

    param.outDistance = sqrtf(sqrDistance);
    param.outDirectionToLight /= param.outDistance;

    const float cosNormalDir = Vector4::Dot3(normal, -param.outDirectionToLight);
    if (cosNormalDir < RT_EPSILON)
    {
        return Color::Zero();
    }

    param.outDirectPdfW = invArea * sqrDistance / cosNormalDir;

    return Color::SampleRGB(param.context.wavelength, rgbColor);
}

const Color AreaLight::GetRadiance(RenderingContext& context, const math::Vector4& rayDirection, const math::Vector4& hitPoint, Float* outDirectPdfA) const
{
    const float cosNormalDir = Vector4::Dot3(normal, -rayDirection);
    if (cosNormalDir < RT_EPSILON)
    {
        return Color::Zero();
    }

    if (outDirectPdfA)
    {
        *outDirectPdfA = invArea;
    }

    Vector4 rgbColor = mColor;

    if (mTexture)
    {
        const Vector4 lightSpaceHitPoint = hitPoint - p0;
        const Float u = Vector4::Dot3(lightSpaceHitPoint, edge0 * edgeLengthInv0) * edgeLengthInv0;
        const Float v = Vector4::Dot3(lightSpaceHitPoint, edge1 * edgeLengthInv1) * edgeLengthInv1;
        const Vector4 textureCoords(u, v, 0.0f, 0.0f);

        rgbColor *= mTexture->Sample(textureCoords, SamplerDesc());
    }

    return Color::SampleRGB(context.wavelength, rgbColor);
}

const Vector4 AreaLight::GetNormal(const Vector4&) const
{
    return normal;
}

bool AreaLight::IsFinite() const
{
    return true;
}

bool AreaLight::IsDelta() const
{
    return false;
}

} // namespace rt
