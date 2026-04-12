#ifndef MIDSIDE_H
#define MIDSIDE_H

#include <cmath>

namespace sudwalfulkaan {
    class MidSide {
        public:
            void Init(float width, float sampleRate);
            void Process(float& left, float& right);
            void SetWidth(float width);

        private:
            float width_;
            float sampleRate_;
    };
}

#endif // MIDSIDE_H

