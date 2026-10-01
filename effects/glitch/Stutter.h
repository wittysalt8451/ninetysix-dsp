#pragma once
#ifndef NINETYSIX_STUTTER_H
#define NINETYSIX_STUTTER_H

#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus

namespace ninetysix {

    /**
     * @brief Bar-locked beat repeat (DJ "roll") and reverse.
     *
     * A bar grid (4 beats, 4/4) runs every sample and is locked to an
     * external clock with SyncBeat() / SyncDownbeat(). Input records into
     * a ring continuously, also while an effect plays, so the ring is
     * always one gapless timeline.
     *
     * With quantize on (a clock is patched) both effects switch on and off
     * only on a bar line: open or close the knob anywhere in the bar and
     * the change lands on the next downbeat. The release fades out just
     * before the line, so the downbeat itself plays dry and intact.
     * Without a clock they switch immediately.
     *
     * Process() is the roll. Like a DJM ROLL it captures from the engage
     * point: the first pass plays live while recording, after that the
     * slice repeats. The amount picks the slice length in powers of two
     * from 1 bar down to 1/256 bar across nine equal knob zones; slices sit
     * on the bar grid, so division changes stay on the beat.
     *
     * ProcessReverse() mirrors playback around the engage point, like a
     * CDJ reverse: the bar before the line plays backwards, so its hits
     * land on the beats leading into the next downbeat. Held longer, that
     * reversed bar loops.
     *
     * Both play from a capture buffer, so they can be held indefinitely.
     */
    class Stutter {
        public:
            /** @brief Loop seam and release crossfade (~2.7 ms at 48 kHz). */
            static constexpr uint32_t kXfadeSamples = 128;

            void Init(float sample_rate);

            /** @brief Loop memory (e.g. SDRAM), one buffer per channel; all
             *  are zeroed. The ring must hold two bars at the slowest clock,
             *  the capture buffer one bar plus kXfadeSamples. */
            void SetBuffers(float* ring_left, float* ring_right, size_t ring_size,
                            float* cap_left, float* cap_right, size_t cap_size);

            /** @brief 0 = off, >0 = on; for the roll also the slice length. */
            void SetAmount(float amount);

            /** @brief true while a clock is patched: engage and release wait
             *  for the next bar line. false: they switch immediately. */
            void SetQuantize(bool quantize) { quantize_ = quantize; }

            /** @brief Clock period in samples for one beat (quarter note). */
            void SetBeatSamples(uint32_t samples);
            uint32_t GetBeatSamples() const { return beat_samples_; }

            /** @brief Beat pulse: move the grid onto the next beat. */
            void SyncBeat();

            /** @brief Downbeat pulse: the grid is on the 1 of a bar. */
            void SyncDownbeat();

            /** @brief Feed the ring without playing (call while the effect
             *  slot is occupied by something else). Drops a running effect
             *  and keeps the grid running. */
            void Record(float left, float right);

            /** @brief Beat repeat (roll), in place. */
            void Process(float& left, float& right);

            /** @brief Reverse, in place. */
            void ProcessReverse(float& left, float& right);

            bool IsActive() const { return active_; }
            uint32_t GetGridPos() const { return grid_pos_; }

        private:
            enum class Mode : uint8_t { kRoll, kReverse };

            void Tick(Mode mode, float& left, float& right);
            void Engage(Mode mode, size_t now);
            void WriteRing(float left, float right);
            void AdvanceGrid();
            uint32_t BarLen() const { return beat_samples_ * 4; }
            int Division() const;
            size_t RingIndex(int64_t offset_from_engage) const;
            void SliceAt(int division, uint32_t& phase, uint32_t& len) const;
            float RollRead(const float* cap, uint32_t phase, uint32_t len) const;
            float ReverseRead(const float* cap, const float* ring) const;

            float*   ring_l_ = nullptr;
            float*   ring_r_ = nullptr;
            size_t   ring_size_ = 0;
            float*   cap_l_ = nullptr;
            float*   cap_r_ = nullptr;
            size_t   cap_size_ = 0;
            bool     ready_ = false;
            size_t   write_pos_ = 0;
            float    amount_ = 0.0f;
            float    sample_rate_ = 48000.0f;

            // Bar grid
            uint32_t beat_samples_ = 24000;
            uint32_t grid_pos_ = 0;     // samples into the current bar
            uint32_t sync_beat_ = 0;    // beat of the bar set by the last pulse
            uint32_t since_sync_ = 0;   // samples since the last pulse
            bool     bar_line_ = false; // a pulse put the grid on the 1
            bool     quantize_ = false;

            // Running effect
            bool     active_ = false;
            bool     releasing_ = false;
            Mode     mode_ = Mode::kRoll;
            size_t   engage_pos_ = 0;   // ring index of the first effect sample
            uint32_t elapsed_ = 0;      // samples since the engage point
            uint32_t release_pos_ = 0;  // wet -> dry crossfade progress
            uint32_t cap_len_ = 0;      // roll: capture length (one bar)
            uint32_t rev_len_ = 0;      // reverse: length of the mirrored bar
            uint32_t copy_pos_ = 0;     // reverse: lowest index copied out of the ring
            int      div_ = 0;          // roll: current division
            int      old_div_ = 0;      // roll: division being faded out
            uint32_t div_fade_pos_ = 0;
    };

} // namespace ninetysix

#endif
#endif
