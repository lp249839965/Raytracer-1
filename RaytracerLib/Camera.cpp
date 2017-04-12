#include "PCH.h"
#include "Camera.h"


namespace rt {

using namespace math;

Camera::Camera()
    : mFieldOfView(RT_PI * 70.0f / 180.0f)
    , mAspectRatio(1.0f)
    , mMode(CameraMode::Perspective)
{ }

void Camera::SetPerspective(const math::Vector& pos, const math::Vector& dir, const math::Vector& up, Float aspectRatio, Float FoV)
{
    mMode = CameraMode::Perspective;
    mPosition = pos;
    mForward = dir;
    mUp = up;
    mAspectRatio = aspectRatio;
    mFieldOfView = FoV;
}

void Camera::Update()
{
    mForwardInternal = mForward.Normalized3();
    mRightInternal = Vector::Cross3(mUp, mForward).Normalized3();
    mUpInternal = Vector::Cross3(mForward, mRightInternal).Normalized3();

    // field of view
    const Float tanHalfFoV = tanf(mFieldOfView * 0.5f);
    mUpInternal *= tanHalfFoV;
    mRightInternal *= tanHalfFoV;

    // aspect ratio
    mRightInternal *= mAspectRatio;
}

math::Ray Camera::GenerateRay(Float x, Float y) const
{
    Vector origin = mPosition;
    Vector direction;

    switch (mMode)
    {
        case rt::CameraMode::Perspective:
        {
            x = 2.0f * x - 1.0f;
            y = 2.0f * y - 1.0f;
            direction = mForward + x * mRightInternal + y * mUpInternal;
            break;
        }
    }

    return Ray(direction, origin);
}


} // namespace rt
