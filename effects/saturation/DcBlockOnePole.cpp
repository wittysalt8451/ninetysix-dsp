#include "DcBlockOnePole.h"

namespace ninetysix {

void DcBlockOnePole::Init(float coefficient) {
    coefficient_ = coefficient;
    x1_ = 0.f;
    y1_ = 0.f;
}

void DcBlockOnePole::Reset() {
    x1_ = 0.f;
    y1_ = 0.f;
}

float DcBlockOnePole::Process(float x) {
    const float y = x - x1_ + coefficient_ * y1_;
    x1_ = x;
    y1_ = y;
    return y;
}

} // namespace ninetysix
