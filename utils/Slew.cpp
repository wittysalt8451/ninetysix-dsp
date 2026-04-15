#include "utils/Slew.h"

namespace sudwalfulkaan {

float SlewTowards(float target, float current, float slewRate) {
    return current + slewRate * (target - current);
}

} // namespace sudwalfulkaan
