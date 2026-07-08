#pragma once

namespace ninetysix {

/**
 * @brief Map normalized 0..1 to a linear range [min, max].
 * @param normalized Input clamped to 0..1 before mapping.
 */
float MapLinear(float normalized, float min, float max);

/**
 * @brief Map normalized 0..1 to a logarithmic range [min, max] (min, max > 0).
 */
float MapLogarithmic(float normalized, float min, float max);

} // namespace ninetysix
