#include "utils/Slew.h"

namespace ninetysix {

float SlewTowards(float target, float current, float slewRate) {
    return current + slewRate * (target - current);
}

} // namespace ninetysix
