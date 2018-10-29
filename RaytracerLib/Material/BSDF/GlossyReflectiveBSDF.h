#pragma once

#include "BSDF.h"

namespace rt {

// Cook-Torrance glossy BRDF
class GlossyReflectiveBSDF : public BSDF
{
public:
    virtual bool Sample(SamplingContext& ctx) const override;
    virtual const Color Evaluate(const EvaluationContext& ctx, Float* outDirectPdfW = nullptr) const override;
};

} // namespace rt
