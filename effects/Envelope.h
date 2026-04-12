#pragma once
#ifndef SWF_ENVELOPE_H
#define SWF_ENVELOPE_H
#ifdef __cplusplus

namespace sudwalfulkaan {
    class Envelope
    {
        public:
            void Init(float attackTime, float decayTime, float sustainLevel, float releaseTime, float sampleRate);
            void Trigger();
            float Process();
            void SetDecay(float decayTime);

        private:
            enum EnvelopeState
            {
                ADSR_IDLE,
                ADSR_ATTACK,
                ADSR_DECAY,
                ADSR_SUSTAIN,
                ADSR_RELEASE
            };

            float attackTime_;
            float decayTime_;
            float sustainLevel_;
            float releaseTime_;
            float sampleRate_;
            float envelope_;
            EnvelopeState state_;
    };
}
#endif
#endif