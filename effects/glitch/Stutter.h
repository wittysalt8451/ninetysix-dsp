#pragma once
#ifndef NINETYSIX_STUTTER_H
#define NINETYSIX_STUTTER_H

#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus

namespace ninetysix {

    /**
     * @brief Measures where the audio's beat sits relative to the clock
     * pulses. A clock that bypasses the audio path (e.g. MIDI clock next to
     * a DAW's buffered audio) can lead the audio by tens of milliseconds;
     * short beat-repeat slices would then miss the attack, and a reverse
     * mirrors it into twice the error the other way.
     *
     * Per pulse it takes the strongest onset within a fifth of a beat
     * either side (60 ms for fast clocks; an onset sits where the level
     * first reaches half its peak, so a soft lead-in does not count). Once five of the last eight
     * agree within 1 ms, their median is the offset; until then it is 0.
     */
    class BeatOffsetTracker {
        public:
            void Init(float sample_rate);

            /** @brief Feed one input sample, before any processing. */
            void Process(float left, float right);

            /** @brief A clock pulse lands on the next sample. */
            void OnPulse(uint32_t beat_samples);

            /** @brief Samples from pulse to audio beat (negative: audio
             *  first). Only meaningful while Locked(). */
            int32_t Offset() const { return offset_; }
            bool Locked() const { return locked_; }

        private:
            static constexpr uint32_t kMaxMeasure = 768; // 16 ms at 48 kHz
            static constexpr int      kOnsets     = 32;
            static constexpr int      kHistory    = 8;

            void FinishOnset();
            void FinishBeat();

            uint32_t clock_ = 0;        // running sample counter
            float    base_ = 0.0f;      // recent background level
            float    base_coef_ = 0.0f;
            uint32_t hold_ = 0;         // refractory countdown
            uint32_t refractory_ = 0;

            // Level right after a trigger, to find the half-peak point
            bool     measuring_ = false;
            uint32_t measure_start_ = 0;
            uint32_t measure_pos_ = 0;
            uint32_t measure_len_ = 0;
            float    levels_[kMaxMeasure] = {};

            uint32_t onset_time_[kOnsets] = {};
            float    onset_peak_[kOnsets] = {};
            int      onset_next_ = 0;

            bool     pending_ = false;  // pulse waiting for its onsets
            uint32_t pulse_clock_ = 0;
            uint32_t window_ = 0;
            uint32_t wide_window_ = 0;
            uint32_t tolerance_ = 0;

            int32_t  history_[kHistory] = {};
            int      hist_count_ = 0;
            int      hist_next_ = 0;
            int32_t  offset_ = 0;
            bool     locked_ = false;
    };

    /**
     * @brief Grid-locked beat repeat and bar reverse (DJ "roll" style).
     *
     * The class keeps an internal bar grid (4 beats per bar, 4/4) that is
     * advanced every sample and re-synced externally via SyncBeat() /
     * SyncDownbeat(). Input records continuously into a ring buffer (the
     * repeat keeps recording underneath, so the ring has no gap after it).
     *
     * Process() freezes the beat it engages in and cuts every following
     * beat into slices that all replay that beat's start, so each slice
     * opens on the beat (the kick, in four-on-the-floor). The amount
     * selects the slice length in powers of two from 1 beat (1/4 bar) down
     * to 1/64 beat (1/256 bar) across seven equal knob zones. With a clock
     * patched, "the beat" is where the audio's own beat lands, as for the
     * reverse, so even the shortest slices start right at the attack.
     * Because playback position is derived from the running beat phase,
     * division changes are seamless and the roll never leaves the beat.
     *
     * ProcessReverse() mirrors the audio around a beat: it engages and
     * releases on the beat (or retroactively, up to a quarter beat late)
     * and loops the bar before that beat backwards. With a clock patched,
     * "the beat" is the pulse shifted to where the audio's own beat lands
     * (BeatOffsetTracker), so reversed hits end exactly where the live hits
     * start even when the clock leads the audio. Playback runs on a sample
     * counter and restarts on the 4th such beat, so a grid beat length that
     * is slightly off does not matter; without a clock the grid takes over.
     *
     * The ring must hold at least two bars: the reverse reads up to five
     * beats back.
     */
    class Stutter {
        public:
            void Init(float sample_rate);

            /** @brief External loop memory (e.g. SDRAM), one buffer per channel.
             *  Zeroes both. Must hold at least two bars at the slowest clock. */
            void SetBuffers(float* left, float* right, size_t size);

            /** @brief 0 = bypass/record, >0 = active. */
            void SetAmount(float amount);

            /** @brief On/off request for the reverse. Update it every audio
             *  block from an unsmoothed control so the beat it lands on is
             *  never missed. */
            void SetReverseGate(bool on) { reverse_gate_ = on; }

            /** @brief True while a repeat or reverse is playing (including
             *  a pending reverse release). */
            bool IsActive() const { return active_; }

            /** @brief Clock period in samples for one beat (quarter note). */
            void SetBeatSamples(uint32_t samples);
            uint32_t GetBeatSamples() const { return beat_samples_; }

            /** @brief Beat pulse (B10): snap the grid to the nearest beat. */
            void SyncBeat();

            /** @brief Downbeat pulse (B9): reset the grid to the bar start. */
            void SyncDownbeat();

            /** @brief Feed the loop memory without playing (call while the
             *  effect slot is occupied by something else). Drops any active
             *  freeze and keeps the grid running. */
            void Record(float left, float right);

            /** @brief Beat repeat, in place. */
            void Process(float& left, float& right);

            /** @brief One-bar reverse, in place. */
            void ProcessReverse(float& left, float& right);

        private:
            void AdvanceGrid();
            void Resume();
            void RecordSample(float left, float right);
            void WriteRing(float left, float right);
            void UpdateBeatOffset();
            float ReverseRead(const float* buf, uint32_t pos) const;
            uint32_t BarLen() const { return beat_samples_ * 4; }
            int Division() const;
            uint32_t RepeatBeatPhase() const;
            float SliceRead(const float* buf, int div) const;

            float*   buf_l_ = nullptr;
            float*   buf_r_ = nullptr;
            size_t   size_ = 0;
            uint32_t write_pos_ = 0;
            uint32_t grid_pos_ = 0;      // samples into the current bar
            uint32_t since_pulse_ = 0xFFFFFFFFu; // samples since the last SyncBeat
            uint32_t period_ = 0;        // measured pulse period
            // Audio beat: vbeat_at_ samples after each pulse, or before the
            // next one (predicted from period_) when the audio leads
            uint32_t since_vbeat_ = 0xFFFFFFFFu; // samples since the last audio beat
            uint32_t vbeat_at_ = 0;
            bool     vbeat_early_ = false;
            BeatOffsetTracker tracker_;
            bool     active_ = false;
            bool     reverse_ = false;   // active_ belongs to ProcessReverse
            bool     releasing_ = false;
            bool     reverse_gate_ = false;
            float    amount_ = 0.0f;
            uint32_t beat_samples_ = 24000;
            float    sample_rate_ = 48000.0f;

            // Click-free transitions
            uint32_t engage_fade_pos_ = 0;  // dry -> wet on engage
            uint32_t release_fade_pos_ = 0; // wet -> dry on release
            uint32_t div_fade_pos_ = 0;     // old -> new division
            int      cur_div_ = 0;
            int      old_div_ = -1;

            // Repeat playback
            uint32_t anchor_pos_ = 0;       // ring index of the frozen beat
            uint32_t rec_room_ = 0;         // samples left to record underneath
            uint32_t rec_run_ = 0;          // samples recorded since the last gap
            uint32_t slice_fade_in_ = 0;
            bool     repeat_clocked_ = false; // beat phase from the audio beat
            bool     wait_beat_ = false;    // live until the next beat freezes

            // Reverse playback
            uint32_t mirror_idx_ = 0;     // ring index of the mirror beat
            uint32_t rev_pos_ = 0;        // samples since the mirror beat
            uint32_t rev_beats_ = 0;      // beats counted in this pass
            uint32_t last_beat_pos_ = 0;  // rev_pos_ at the last counted beat
            uint32_t seam_old_pos_ = 0;   // where the previous pass left off
            uint32_t seam_fade_pos_ = 0;  // previous pass -> restart
    };

} // namespace ninetysix

#endif
#endif
