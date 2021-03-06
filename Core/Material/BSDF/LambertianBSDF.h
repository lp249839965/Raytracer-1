#pragma once

#include "BSDF.h"

namespace rt {

// simplest Lambertian diffuse
class LambertianBSDF : public BSDF
{
public:
    virtual bool Sample(SamplingContext& ctx) const override;
    virtual const Color Evaluate(const EvaluationContext& ctx, Float* outDirectPdfW = nullptr) const override;
};

} // namespace rt
