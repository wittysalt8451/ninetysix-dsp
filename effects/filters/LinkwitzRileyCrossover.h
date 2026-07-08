#ifndef CROSSOVER_H
#define CROSSOVER_H

#include <cmath>

namespace ninetysix {
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
} // namespace ninetysix

#endif // CROSSOVER_H
