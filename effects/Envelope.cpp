#include "Envelope.h"

using namespace sudwalfulkaan;

void Envelope::Init(float attackTime, float decayTime, float sustainLevel, float releaseTime, float sampleRate)
{
    attackTime_ = attackTime;
    decayTime_ = decayTime;
    sustainLevel_ = sustainLevel;
    releaseTime_ = releaseTime;
    sampleRate_ = sampleRate;
    envelope_ = 0.0f;
    state_ = ADSR_IDLE;
}

void Envelope::Trigger()
{
    state_ = ADSR_ATTACK;
    envelope_ = 0.0f; // Reset envelope to start attack
}

float Envelope::Process()
{
    switch (state_)
    {
        case ADSR_ATTACK:
            envelope_ += (1.0f / (attackTime_ * sampleRate_));
            if (envelope_ >= 1.0f)
            {
                envelope_ = 1.0f;
                state_ = ADSR_DECAY;
            }
            break;

        case ADSR_DECAY:
            envelope_ -= (1.0f / (decayTime_ * sampleRate_));
            if (envelope_ <= sustainLevel_)
            {
                envelope_ = sustainLevel_;
                state_ = ADSR_SUSTAIN;
            }
            break;

        case ADSR_SUSTAIN:
            // Maintain sustain level
            break;

        case ADSR_RELEASE:
            envelope_ -= (1.0f / (releaseTime_ * sampleRate_));
            if (envelope_ <= 0.0f)
            {
                envelope_ = 0.0f;
                state_ = ADSR_IDLE;
            }
            break;

        case ADSR_IDLE:
            // Envelope is idle, no output.
            break;
    }

    return envelope_;
}

void Envelope::SetDecay(float decayTime) {
    decayTime_ = decayTime;
}