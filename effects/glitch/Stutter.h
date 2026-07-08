#pragma once
#ifndef NINETYSIX_STUTTER_H
#define NINETYSIX_STUTTER_H

#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus

namespace ninetysix {

    /**
     * @brief Grid-locked beat repeat and bar reverse (DJ "roll" style).
     *
     * The class keeps an internal bar grid (4 beats per bar, 4/4) that is
     * advanced every sample and re-synced externally via SyncBeat() /
     * SyncDownbeat(). Input records continuously into a ring buffer while
     * inactive; ring positions stay mapped onto grid slots.
     *
     * Process() repeats the last completed slice, phase-locked to the grid:
     * the amount selects the slice length in powers of two from 1 bar down
     * to 1/256 bar across nine equal knob zones. Because playback position
     * is derived from the running grid phase, division changes are seamless
     * and the roll never leaves the beat.
     *
     * ProcessReverse() plays the recorded bar mirrored within the bar grid,
     * so bar boundaries always land on the downbeat.
     *
     * The ring must hold at least two bars: slices anchor to the previous
     * grid cell, which can reach one full bar back.
     */
    class Stutter {
        public:
            void Init(float sample_rate);

            /** @brief External loop memory (e.g. SDRAM), one buffer per channel.
             *  Zeroes both. Must hold at least two bars at the slowest clock. */
            void SetBuffers(float* left, float* right, size_t size);

            /** @brief 0 = bypass/record, >0 = active. */
            void SetAmount(float amount);

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
            uint32_t BarLen() const { return beat_samples_ * 4; }
            uint32_t LoopLengthSamples() const;
            float ReadRing(const float* buf, int64_t offset_from_bar_start) const;
            float SliceRead(const float* buf, uint32_t loop_len) const;

            float*   buf_l_ = nullptr;
            float*   buf_r_ = nullptr;
            size_t   size_ = 0;
            uint32_t write_pos_ = 0;
            uint32_t bar_start_pos_ = 0; // ring index of the current bar's grid 0
            uint32_t grid_pos_ = 0;      // samples into the current bar
            uint32_t engage_grid_ = 0;   // grid position at freeze
            bool     active_ = false;
            bool     releasing_ = false;
            float    amount_ = 0.0f;
            uint32_t beat_samples_ = 24000;
            float    sample_rate_ = 48000.0f;

            // Click-free transitions
            uint32_t engage_fade_pos_ = 0;  // dry -> wet on engage
            uint32_t release_fade_pos_ = 0; // wet -> dry on release
            uint32_t div_fade_pos_ = 0;     // old -> new division
            uint32_t cur_loop_len_ = 0;
            uint32_t old_loop_len_ = 0;
    };

} // namespace ninetysix

#endif
#endif
