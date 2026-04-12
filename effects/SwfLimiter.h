#ifndef SWF_LIMITER_H
#define SWF_LIMITER_H

namespace sudwalfulkaan {

class SwfLimiter {
public:
    void Init(float threshold = 1.0f, float attack = 0.01f, float release = 0.1f);
    float Process(float in);

    /** Set threshold;
        \param t Works best at around 0.5-1 (default = 1.0f)
    */
    void SetThreshold(float t);

    /** Set attack;
        \param a Works 0.01 - x (default = 0.01f)
    */
    void SetAttack(float a);

    /** Set r;
        \param r Works 0.1 - x (default = 0.1f)
    */
    void SetRelease(float r);

    /** Set s;
        \param r Works at ? (default = 5f)
    */
    void SetSoftness(float s);

    /** Set e;
        \param e Works at (0.0f - 1.0f). From Long Attack / Short Release to Short Attack / Long Release
    */
    void SetEnvelope(float e);

private:
    float threshold_;
    float attack_;
    float release_;
    float gain_; // Automatic gain compensation
    float envelope_; // Tracks signal level for smooth limiting
    float softness_ = 5.0f; // Adjust this value for desired softness
};

} // namespace sudwalfulkaan

#endif // SWF_LIMITER_H
