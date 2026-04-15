#pragma once

namespace sudwalfulkaan {

/**
 * @brief One-pole step toward target (same as former SlewLimiter in tools.cpp).
 * @param slewRate Typically small; larger moves faster toward target.
 */
float SlewTowards(float target, float current, float slewRate);

} // namespace sudwalfulkaan
