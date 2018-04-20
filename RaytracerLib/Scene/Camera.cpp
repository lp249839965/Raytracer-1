#include "PCH.h"
#include "Camera.h"


namespace rt {

using namespace math;

Camera::Camera()
    : mAspectRatio(1.0f)
    , mFieldOfView(RT_PI * 80.0f / 180.0f)
    , barrelDistortionFactor(0.005f)
{ }

void Camera::SetPerspective(const math::Vector4& pos, const math::Vector4& dir, const math::Vector4& up, Float aspectRatio, Float FoV)
{
    mPosition = pos;
    mForward = dir;
    mUp = up;
    mAspectRatio = aspectRatio;
    mFieldOfView = FoV;
}

void Camera::Update()
{
    mForwardInternal = mForward.Normalized3();
    mRightInternal = Vector4::Cross3(mUp, mForwardInternal).Normalized3();
    mUpInternal = Vector4::Cross3(mForwardInternal, mRightInternal).Normalized3();

    // field of view & aspect ratio
    const Float tanHalfFoV = tanf(mFieldOfView * 0.5f);
    mUpScaled = mUpInternal * tanHalfFoV;
    mRightScaled = mRightInternal * (tanHalfFoV * mAspectRatio);
}

math::Ray Camera::GenerateRay(const math::Vector4& coords, math::Random& randomGenerator) const
{
    Vector4 origin = mPosition;
    Vector4 offsetedCoords = 2.0f * coords - VECTOR_ONE;

    // barrel distortion
    if (barrelDistortionFactor != 0.0f)
    {
        const Vector4 radius = Vector4::Dot2V(offsetedCoords, offsetedCoords);
        offsetedCoords += offsetedCoords * radius * barrelDistortionFactor;
    }

    Vector4 direction = mForwardInternal + offsetedCoords[0] * mRightScaled + offsetedCoords[1] * mUpScaled;

    // depth of field
    if (mDOF.aperture > 0.0f)
    {
        const Vector4 focusPoint = origin + mDOF.focalPlaneDistance * direction;

        // TODO different bokeh shapes, texture, etc.
        const Vector4 randomPointOnCircle = randomGenerator.GetCircle() * mDOF.aperture;
        origin += randomPointOnCircle[0] * mRightInternal;
        origin += randomPointOnCircle[1] * mUpInternal;

        direction = focusPoint - origin;
    }

    return Ray(origin, direction);
}


} // namespace rt