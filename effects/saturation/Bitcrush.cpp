#include "saturation/Bitcrush.h"
#include "utils/Mapping.h"

using namespace ninetysix;

void Bitcrush::SetBits(float bits) {
    // One bit is the sign: n bits leave 2^(n-1) steps per polarity
    scale_ = powf(2.0f, Clamp(bits, 1.0f, 24.0f) - 1.0f);
    inv_scale_ = 1.0f / scale_;
}
