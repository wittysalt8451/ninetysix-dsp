#include "glitch/Stutter.h"

#include <math.h>

using namespace ninetysix;

namespace {
    constexpr float    kActiveThreshold = 0.01f;
    constexpr uint32_t kMinLoopSamples  = 16;
    // Slices per beat in powers of two: 1 beat (1/4 bar) down to 1/64 beat
    // (1/256 bar)
    constexpr int      kNumDivisions    = 7;
    // ~2.7 ms at 48 kHz: short enough to stay tight, long enough to kill clicks
    constexpr uint32_t kFadeSamples     = 128;
    // Reverse transitions sit right on the beat, next to the (reversed)
    // attacks, so they get a 1 ms fade instead. On release the live input
    // comes back within 1/3 ms so the attack on that beat stays intact,
    // while the reverse fades out underneath it.
    constexpr uint32_t kReverseFadeSamples = 48;
    constexpr uint32_t kDryReturnSamples   = 16;
    constexpr uint32_t kNoPulse         = 0xFFFFFFFFu;
    // The reverse turns around this far before the measured onset: the
    // attack itself never gets mirrored, and on release the dry input is
    // fully back (kDryReturnSamples) when the live attack arrives
    constexpr int32_t  kAlignLead       = static_cast<int32_t>(kDryReturnSamples);

    // Onset detection for the beat offset
    constexpr float    kOnsetRatio      = 3.0f;   // jump over the background
    constexpr float    kOnsetFloor      = 0.003f; // about -50 dBFS
    constexpr int      kOnsetsAgree     = 5;      // of the last 8 beats

    // Mini fade-in/out at the edges of a repeating span. The fade-in can be
    // shorter, so an attack right after the span start stays intact.
    float EdgeEnv(uint32_t phase, uint32_t len, uint32_t fade_in) {
        uint32_t f = kFadeSamples;
        if (len < 4 * f) f = len / 4 + 1;
        const uint32_t fi = fade_in < f ? fade_in : f;
        float env = 1.0f;
        if (phase < fi) env = static_cast<float>(phase) / static_cast<float>(fi);
        const uint32_t tail = len - phase;
        if (tail < f) {
            const float e = static_cast<float>(tail) / static_cast<float>(f);
            if (e < env) env = e;
        }
        return env;
    }

    inline float Mix(float a, float b, float t) { return a + (b - a) * t; }
}

void BeatOffsetTracker::Init(float sample_rate) {
    base_coef_   = 1.0f - expf(-1.0f / (0.015f * sample_rate));
    refractory_  = static_cast<uint32_t>(0.030f * sample_rate);
    measure_len_ = static_cast<uint32_t>(0.015f * sample_rate);
    if (measure_len_ > kMaxMeasure) measure_len_ = kMaxMeasure;
    tolerance_   = static_cast<uint32_t>(0.001f * sample_rate);
    wide_window_ = static_cast<uint32_t>(0.060f * sample_rate);
    clock_ = 0;
    base_ = 0.0f;
    hold_ = 0;
    measuring_ = false;
    for (int i = 0; i < kOnsets; ++i) onset_peak_[i] = 0.0f;
    onset_next_ = 0;
    pending_ = false;
    hist_count_ = hist_next_ = 0;
    offset_ = 0;
    locked_ = false;
}

void BeatOffsetTracker::Process(float left, float right) {
    const float l = fabsf(left), r = fabsf(right);
    const float level = l > r ? l : r;

    if (measuring_) {
        levels_[measure_pos_++] = level;
        if (measure_pos_ >= measure_len_) FinishOnset();
    } else if (hold_ == 0 && level > kOnsetRatio * base_ + kOnsetFloor) {
        measuring_     = true;
        measure_start_ = clock_;
        measure_pos_   = 0;
        levels_[measure_pos_++] = level;
        hold_ = refractory_;
    }
    if (hold_ > 0) --hold_;
    base_ += (level - base_) * base_coef_;

    // Onsets up to a window after the pulse have been measured by now
    if (pending_ && clock_ - pulse_clock_ >= window_ + measure_len_) FinishBeat();
    ++clock_;
}

// The onset is where the level first reaches half the peak that follows
// the trigger: a hard attack, not the soft lead-in that set it off
void BeatOffsetTracker::FinishOnset() {
    measuring_ = false;
    float peak = 0.0f;
    for (uint32_t i = 0; i < measure_pos_; ++i) {
        if (levels_[i] > peak) peak = levels_[i];
    }
    uint32_t at = 0;
    while (at < measure_pos_ && levels_[at] < 0.5f * peak) ++at;
    onset_time_[onset_next_] = measure_start_ + at;
    onset_peak_[onset_next_] = peak;
    onset_next_ = (onset_next_ + 1) % kOnsets;
}

void BeatOffsetTracker::OnPulse(uint32_t beat_samples) {
    if (pending_) FinishBeat();
    pulse_clock_ = clock_;
    // A fifth of a beat either side, but at least 60 ms for fast clocks
    // (16ths) as long as that stays clear of the neighbouring pulses
    window_ = beat_samples / 5;
    const uint32_t wide = wide_window_ < 2 * beat_samples / 5
                          ? wide_window_ : 2 * beat_samples / 5;
    if (wide > window_) window_ = wide;
    pending_ = true;
}

void BeatOffsetTracker::FinishBeat() {
    pending_ = false;

    // Strongest onset near this pulse
    bool    found = false;
    int32_t best  = 0;
    float   best_peak = 0.0f;
    for (int i = 0; i < kOnsets; ++i) {
        if (onset_peak_[i] <= best_peak) continue;
        const int32_t d = static_cast<int32_t>(onset_time_[i] - pulse_clock_);
        const int32_t w = static_cast<int32_t>(window_);
        if (d < -w || d > w) continue;
        found     = true;
        best      = d;
        best_peak = onset_peak_[i];
    }
    if (!found) return;

    history_[hist_next_] = best;
    hist_next_ = (hist_next_ + 1) % kHistory;
    if (hist_count_ < kHistory) ++hist_count_;
    if (hist_count_ < kOnsetsAgree) return;

    // Median, and how many beats agree with it
    int32_t sorted[kHistory];
    for (int i = 0; i < hist_count_; ++i) {
        int32_t v = history_[i];
        int j = i;
        for (; j > 0 && sorted[j - 1] > v; --j) sorted[j] = sorted[j - 1];
        sorted[j] = v;
    }
    const int32_t median = sorted[hist_count_ / 2];
    int agree = 0;
    for (int i = 0; i < hist_count_; ++i) {
        const int32_t diff = sorted[i] - median;
        if (diff <= static_cast<int32_t>(tolerance_)
            && diff >= -static_cast<int32_t>(tolerance_)) ++agree;
    }
    // A lock holds through beats without clear hits (breakdowns): only a
    // new consistent offset replaces it
    if (agree >= kOnsetsAgree) {
        offset_ = median;
        locked_ = true;
    }
}

void Stutter::Init(float sample_rate) {
    sample_rate_  = sample_rate;
    beat_samples_ = static_cast<uint32_t>(sample_rate * 0.5f); // 120 BPM until clocked
    write_pos_ = grid_pos_ = 0;
    active_ = reverse_ = releasing_ = reverse_gate_ = false;
    amount_ = 0.0f;
    since_pulse_ = since_vbeat_ = kNoPulse;
    period_ = vbeat_at_ = 0;
    vbeat_early_ = false;
    tracker_.Init(sample_rate);
    mirror_idx_ = rev_pos_ = rev_beats_ = last_beat_pos_ = 0;
    seam_old_pos_ = 0;
    seam_fade_pos_ = kReverseFadeSamples;
    engage_fade_pos_ = release_fade_pos_ = div_fade_pos_ = 0;
    cur_div_ = 0;
    old_div_ = -1;
    anchor_pos_ = rec_room_ = rec_run_ = 0;
    slice_fade_in_ = kFadeSamples;
    repeat_clocked_ = wait_beat_ = false;
}

void Stutter::SetBuffers(float* left, float* right, size_t size) {
    buf_l_ = left;
    buf_r_ = right;
    size_  = size;
    for (size_t i = 0; i < size; ++i) {
        left[i]  = 0.0f;
        right[i] = 0.0f;
    }
    write_pos_ = 0;
    rec_run_   = 0;
}

void Stutter::SetAmount(float amount) {
    amount_ = amount < 0.0f ? 0.0f : (amount > 1.0f ? 1.0f : amount);
}

void Stutter::SetBeatSamples(uint32_t samples) {
    // Two full bars (8 beats) must fit in the ring: the reverse reads up to
    // five beats back from the current position
    const uint32_t max_beat = size_ ? static_cast<uint32_t>(size_ / 8) : samples;
    if (samples > max_beat) samples = max_beat;
    if (samples < kMinLoopSamples) samples = kMinLoopSamples;
    beat_samples_ = samples;
    if (grid_pos_ >= BarLen()) grid_pos_ %= BarLen();
}

void Stutter::SyncBeat() {
    // Real pulse period, to predict an audio beat that leads the pulse
    if (since_pulse_ >= beat_samples_ / 2 && since_pulse_ <= 2 * beat_samples_) {
        period_ = since_pulse_;
    }

    // Snap to the nearest beat boundary; corrects free-run drift while
    // leaving the bar phase (set by SyncDownbeat) intact
    const uint32_t beat = beat_samples_;
    const uint32_t k = (grid_pos_ + beat / 2) / beat;
    grid_pos_ = (k % 4) * beat;
    since_pulse_ = 0;
    tracker_.OnPulse(beat);

    if (!vbeat_early_) {
        if (vbeat_at_ == 0) since_vbeat_ = 0;
    } else if (since_vbeat_ >= beat / 2) {
        // The predicted audio beat did not come before this pulse (the
        // beat got shorter): take it now rather than skip it
        since_vbeat_ = 0;
    }
}

void Stutter::SyncDownbeat() {
    grid_pos_ = 0;
}

void Stutter::AdvanceGrid() {
    if (++grid_pos_ >= BarLen()) grid_pos_ = 0;
    if (since_pulse_ != kNoPulse) ++since_pulse_;
    if (since_vbeat_ != kNoPulse) ++since_vbeat_;
    // A running repeat or reverse keeps the offset it engaged with
    if (!active_) UpdateBeatOffset();
    if (since_pulse_ != kNoPulse && since_pulse_ != 0 && since_pulse_ == vbeat_at_) {
        since_vbeat_ = 0;
    }
}

void Stutter::UpdateBeatOffset() {
    const int32_t off = tracker_.Locked() ? tracker_.Offset() - kAlignLead : 0;
    if (off >= 0) {
        vbeat_at_    = static_cast<uint32_t>(off);
        vbeat_early_ = false;
    } else {
        const uint32_t lead = static_cast<uint32_t>(-off);
        vbeat_at_    = period_ > lead ? period_ - lead : 0;
        vbeat_early_ = true;
    }
}

// Leaving a freeze. The repeat recorded underneath, so the ring is gap-free;
// after a reverse it skips the reversed stretch.
void Stutter::Resume() {
    active_    = false;
    releasing_ = false;
}

// Slices per beat, as a power of two
int Stutter::Division() const {
    const int k = static_cast<int>(amount_ * static_cast<float>(kNumDivisions));
    return k >= kNumDivisions ? kNumDivisions - 1 : k;
}

// Samples since the beat the repeat runs on: with a clock the audio's own
// beat, free-running the grid's
uint32_t Stutter::RepeatBeatPhase() const {
    return repeat_clocked_ ? since_vbeat_ : grid_pos_ % beat_samples_;
}

// Beat-locked repeat: every beat is cut into 2^div slices that each replay
// the start of the frozen beat, with mini fades at the slice edges. The
// slice edges are spread so they tile the beat exactly, and the phase
// restarts on every beat, so a beat length that is not a multiple of the
// slice never drifts the slices off the beat.
float Stutter::SliceRead(const float* buf, int div) const {
    const uint64_t beat = beat_samples_;
    uint64_t n = 1ull << div;
    while (n > 1 && beat / n < kMinLoopSamples) n >>= 1;
    const uint64_t pos   = RepeatBeatPhase();
    const uint64_t slice = pos * n / beat;
    const uint64_t start = (slice * beat + n - 1) / n;
    const uint64_t end   = ((slice + 1) * beat + n - 1) / n;
    const uint32_t phase = static_cast<uint32_t>(pos - start);
    return buf[(anchor_pos_ + phase) % size_]
           * EdgeEnv(phase, static_cast<uint32_t>(end - start), slice_fade_in_);
}

void Stutter::Record(float left, float right) {
    if (!buf_l_ || !buf_r_ || size_ == 0) return;
    tracker_.Process(left, right);
    RecordSample(left, right);
}

void Stutter::RecordSample(float left, float right) {
    if (active_) Resume();
    WriteRing(left, right);
    AdvanceGrid();
}

void Stutter::WriteRing(float left, float right) {
    buf_l_[write_pos_] = left;
    buf_r_[write_pos_] = right;
    write_pos_ = (write_pos_ + 1) % size_;
    if (rec_run_ < size_) ++rec_run_;
}

void Stutter::Process(float& left, float& right) {
    if (!buf_l_ || !buf_r_ || size_ == 0) return;

    if (active_ && reverse_) Resume(); // switched over from the reverse
    tracker_.Process(left, right);

    const bool want = amount_ > kActiveThreshold;

    if (!active_) {
        if (!want) {
            RecordSample(left, right);
            return;
        }
        // Engage immediately: playback is phase-locked to the beat anyway.
        // The frozen beat is the one we are in, so a press on the kick
        // repeats that kick (a whole-beat slice first plays it out live).
        active_           = true;
        reverse_          = false;
        releasing_        = false;
        repeat_clocked_   = since_pulse_ <= 2 * beat_samples_
                            && since_vbeat_ < 2 * beat_samples_;
        // A measured audio beat sits kAlignLead before the attack: fade in
        // no longer than that so the attack stays intact
        slice_fade_in_    = repeat_clocked_ && tracker_.Locked()
                            ? static_cast<uint32_t>(kAlignLead) : kFadeSamples;
        const uint32_t beat_phase = RepeatBeatPhase();
        anchor_pos_       = (write_pos_ + static_cast<uint32_t>(size_) - beat_phase)
                            % static_cast<uint32_t>(size_);
        rec_room_         = static_cast<uint32_t>(size_) - beat_phase;
        // A gap in the ring since this beat began (after a reverse or a
        // very long repeat): stay live and freeze the next beat instead
        wait_beat_        = beat_phase > rec_run_;
        engage_fade_pos_  = 0;
        release_fade_pos_ = 0;
        cur_div_          = Division();
        old_div_          = -1;
        div_fade_pos_     = kFadeSamples;
    }

    if (wait_beat_ && RepeatBeatPhase() % beat_samples_ == 0) {
        wait_beat_  = false;
        anchor_pos_ = write_pos_;
        rec_room_   = static_cast<uint32_t>(size_);
    }

    // Keep recording underneath (before reading: the first pass of a slice
    // is this very input) until the write head would reach the frozen beat
    if (rec_room_ > 0) {
        WriteRing(left, right);
        --rec_room_;
    } else {
        rec_run_ = 0;
    }

    // Crossfade division changes so sweeping the knob never clicks
    const int div = Division();
    if (want && div != cur_div_) {
        old_div_      = cur_div_;
        cur_div_      = div;
        div_fade_pos_ = 0;
    }

    float wetL = left, wetR = right;
    if (!wait_beat_) {
        wetL = SliceRead(buf_l_, cur_div_);
        wetR = SliceRead(buf_r_, cur_div_);
        if (div_fade_pos_ < kFadeSamples && old_div_ >= 0) {
            const float t = static_cast<float>(div_fade_pos_++) / kFadeSamples;
            wetL = Mix(SliceRead(buf_l_, old_div_), wetL, t);
            wetR = Mix(SliceRead(buf_r_, old_div_), wetR, t);
        }
    }

    if (!want && !releasing_) {
        releasing_        = true;
        release_fade_pos_ = 0;
    }

    if (releasing_) {
        // Wet fades back into the live input
        const float t = static_cast<float>(release_fade_pos_++) / kFadeSamples;
        left  = Mix(wetL, left, t);
        right = Mix(wetR, right, t);
        if (release_fade_pos_ >= kFadeSamples) Resume();
    } else if (engage_fade_pos_ < kFadeSamples) {
        const float t = static_cast<float>(engage_fade_pos_++) / kFadeSamples;
        left  = Mix(left, wetL, t);
        right = Mix(right, wetR, t);
    } else {
        left  = wetL;
        right = wetR;
    }

    AdvanceGrid();
}

// Mirror read: rev_pos_ samples after the mirror beat plays the sample
// recorded rev_pos_ samples before it
float Stutter::ReverseRead(const float* buf, uint32_t pos) const {
    int64_t idx = (static_cast<int64_t>(mirror_idx_) - static_cast<int64_t>(pos))
                  % static_cast<int64_t>(size_);
    if (idx < 0) idx += static_cast<int64_t>(size_);
    return buf[idx];
}

void Stutter::ProcessReverse(float& left, float& right) {
    if (!buf_l_ || !buf_r_ || size_ == 0) return;
    if (active_ && !reverse_) Resume(); // switched over from the repeat
    tracker_.Process(left, right);

    // Beat reference: with a clock patched, the pulse shifted to where the
    // audio's own beat lands. The grid's beat length may sit up to the
    // clock filter's deadband off, and the snap on every pulse would shift
    // a mirror image twice as far, so the grid is only the fallback when
    // free-running.
    const bool     clocked    = since_pulse_ <= 2 * beat_samples_;
    const uint32_t beat_phase = clocked ? since_vbeat_ : grid_pos_ % beat_samples_;
    const bool     on_beat    = beat_phase == 0;
    // A request up to a quarter beat late still belongs to the beat just
    // passed; anything later waits for the next beat (an early press or a
    // gate that falls mid-step never cuts in between beats)
    const bool     in_window  = beat_phase < beat_samples_ / 4;

    if (!active_) {
        if (!reverse_gate_ || !in_window) {
            RecordSample(left, right);
            return;
        }
        active_           = true;
        reverse_          = true;
        releasing_        = false;
        release_fade_pos_ = 0;
        if (on_beat) {
            // This sample is the mirror point: store it and play it back
            // first, so the turnaround is a seamless reflection (no fade)
            mirror_idx_        = write_pos_;
            WriteRing(left, right);
            engage_fade_pos_   = kReverseFadeSamples;
        } else {
            // Late: mirror around the beat that just passed, which is
            // beat_phase samples back in the recording
            mirror_idx_ = (write_pos_ + static_cast<uint32_t>(size_) - beat_phase)
                          % static_cast<uint32_t>(size_);
            engage_fade_pos_ = 0;
        }
        rev_pos_       = beat_phase;
        rev_beats_     = 0;
        last_beat_pos_ = 0;
        seam_fade_pos_ = kReverseFadeSamples;
    } else if ((on_beat && rev_pos_ - last_beat_pos_ >= beat_samples_ / 2)
               || rev_pos_ >= BarLen() + beat_samples_) {
        // Loop the bar before the mirror beat: restart on its 4th beat
        // (the length check only catches a clock that stopped mid-bar)
        last_beat_pos_ = rev_pos_;
        if (++rev_beats_ >= 4 || rev_pos_ >= BarLen() + beat_samples_) {
            seam_old_pos_  = rev_pos_;
            seam_fade_pos_ = 0;
            rev_pos_       = 0;
            rev_beats_     = 0;
            last_beat_pos_ = 0;
        }
    }
    rec_run_ = 0; // the ring stands still while reversing

    if (!reverse_gate_ && !releasing_ && in_window) {
        releasing_        = true;
        release_fade_pos_ = 0;
    }

    float wetL = ReverseRead(buf_l_, rev_pos_);
    float wetR = ReverseRead(buf_r_, rev_pos_);
    if (seam_fade_pos_ < kReverseFadeSamples) {
        // Loop seam: the previous pass runs on past its last attack and
        // fades out after the beat, so that attack stays intact
        const float    t   = static_cast<float>(seam_fade_pos_) / kReverseFadeSamples;
        const uint32_t old = seam_old_pos_ + seam_fade_pos_;
        wetL = Mix(ReverseRead(buf_l_, old), wetL, t);
        wetR = Mix(ReverseRead(buf_r_, old), wetR, t);
        ++seam_fade_pos_;
    }

    if (releasing_) {
        const uint32_t pos = release_fade_pos_++;
        const float wet = 1.0f - static_cast<float>(pos) / kReverseFadeSamples;
        const float dry = pos < kDryReturnSamples
                          ? static_cast<float>(pos) / kDryReturnSamples : 1.0f;
        left  = left * dry + wetL * wet;
        right = right * dry + wetR * wet;
        if (release_fade_pos_ >= kReverseFadeSamples) Resume();
    } else if (engage_fade_pos_ < kReverseFadeSamples) {
        const float t = static_cast<float>(engage_fade_pos_++) / kReverseFadeSamples;
        left  = Mix(left, wetL, t);
        right = Mix(right, wetR, t);
    } else {
        left  = wetL;
        right = wetR;
    }

    ++rev_pos_;
    AdvanceGrid();
}
