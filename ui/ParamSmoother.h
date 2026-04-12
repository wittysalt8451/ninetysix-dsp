#pragma once

#include <cmath>
#include <algorithm>

namespace sudwalfulkaan
{
class ParamSmoother
{
  public:
    void Init(float initial, float slewRate = 0.05f, float threshold = 0.001f)
    {
        smoothed_ = initial;
        previous_ = initial;
        slewRate_ = slewRate;
        threshold_ = threshold;
    }

    float Process(float target)
    {
        smoothed_ += (target - smoothed_) * slewRate_;
        return smoothed_;
    }

    // Only returns true if change is meaningful
    bool HasSignificantChange()
    {
        if(std::fabs(smoothed_ - previous_) > threshold_)
        {
            previous_ = smoothed_;
            return true;
        }
        return false;
    }

    float Value() const { return smoothed_; }

  private:
    float smoothed_;
    float previous_;
    float slewRate_;
    float threshold_;
};
} // namespace sudwalfulkaan
