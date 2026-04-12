#ifndef CROSSOVER_H
#define CROSSOVER_H

#include <cmath>

namespace sudwalfulkaan {
    class LinkwitzRileyCrossover {
        public:
            void Init(float cutoff, float sampleRate);
            void Process(float in, float& lowOut, float& highOut);
            void SetCutoff(float cutoff);

        private:
            void UpdateCoefficients();

            float cutoff_;
            float sampleRate_;
            float a0_, a1_, a2_, b1_, b2_; // Coefficients for Butterworth filters
            float lowPrev1_, lowPrev2_, highPrev1_, highPrev2_;
    };
} // namespace sudwalfulkaan

#endif // CROSSOVER_H

