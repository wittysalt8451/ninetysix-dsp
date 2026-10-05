# ninetysix-dsp

Shared DSP effects and UI library for ninetysix eurorack modules.

## Structure

```
effects/       Audio processors (filters, dynamics, saturation, spatial, modulation, reverb)
envelopes/     ADSR and other modulation envelopes (not bus effects)
generators/    Placeholder for future oscillators, LFOs, noise sources (see `.gitkeep`)
control/       Clock/gate/timing logic (clock from gate, gate pulses)
ui/            Human interface: pots, buttons, smoothing tied to controls
utils/         Hardware-agnostic helpers (mapping, slew, tempo math, level follower)
```

All types live in namespace `ninetysix`.

## Effects

| Module | Folder | Description |
|--------|--------|-------------|
| Biquad | filters | Biquad filter (LP, HP, BP, notch, peak, shelves) |
| LinkwitzRileyCrossover | filters | Linkwitz-Riley crossover filter |
| Resonator | filters | Tuned stereo comb resonator: damped feedback delay with pitch glide, constant ring time and level compensation |
| TapeDrive | saturation | Analog tape saturation emulation with LP filter |
| Tanh | saturation | Tanh soft saturation (`TanhSaturation` class) |
| Limiter | dynamics | Stereo limiter with envelope follower |
| Ducker | dynamics | Stereo sidechain ducker |
| MidSide | spatial | Mid/side stereo width processing |
| StereoChorus | modulation | Stereo chorus |
| StereoPhaser | modulation | Stereo phaser with LFO |
| FDN4Reverb | reverb | 4-line FDN reverb (Householder matrix, decorrelated stereo, no external deps) |
| Echo | delay | Beat-synced stereo echo after the DJM ECHO: filtered feedback, echo-out tail, click-free delay changes |
| SpectralStretch | spectral | Paulstretch-style spectral time stretch (crossfaded random-phase grains) with spectral pitch shift, onset-following playhead and freeze |
| Stutter | glitch | Clock-locked beat repeat (roll) and reverse; with a clock both switch on and off on the downbeat |

## Envelopes

| Module | Description |
|--------|-------------|
| Envelope | ADSR envelope generator |

## Control

| Module | Description |
|--------|-------------|
| ClockDetector | BPM/clock detection from gate input |
| GatePulse | Timed gate pulse generator for CV outputs |

## UI

| Module | Description |
|--------|-------------|
| UIManager | Pot smoothing, button/toggle handling with callbacks |
| ParamSmoother | One-pole parameter slew with change detection |

## Utils

| Module | Description |
|--------|-------------|
| Mapping | `Clamp`, `MapLinear`, `MapLogarithmic` (normalized pot to range) |
| Lfo | Phase-accumulator LFO (sine, triangle) for modulation effects |
| DelayLine | Statically sized fractional delay line with linear interpolation |
| EnvelopeFollower | Smoothed stereo level follower (0..1), per-instance state |
| Slew | `SlewTowards` — one-pole step toward a target |
| Tempo | `CalculateReleaseTime` — BPM-synced release time in seconds |
| Fft | Allocation-free radix-2 complex FFT with caller-owned twiddle tables |
| SpscRingBuffer | Lock-free single-producer/single-consumer float ring (ISR ↔ main loop) |

Migrating from the old single `tools.cpp` is documented in [REFACTOR_TOOLS.md](REFACTOR_TOOLS.md). Folder layout changes (envelopes, control, generators) are in [REFACTOR_LAYOUT.md](REFACTOR_LAYOUT.md).

## Usage

Link or symlink this directory as `library/` in your module project:

```bash
ln -s ../ninetysix-dsp library
```

Add include paths so `effects/`, `envelopes/`, `control/`, `ui/`, and `utils/` resolve:

```make
CFLAGS += -Ilibrary/effects -Ilibrary
```

Then include from application code, for example:

```cpp
#include "filters/Biquad.h"
#include "saturation/Tanh.h"
#include "envelopes/Envelope.h"
#include "control/ClockDetector.h"
#include "ui/UIManager.h"
#include "utils/Mapping.h"
```

If you prefer includes relative to the firmware project root only:

```cpp
#include "library/effects/filters/Biquad.h"
#include "library/envelopes/Envelope.h"
```

## Unit tests (host)

Pure helpers and `Envelope` are covered with **[doctest](https://github.com/doctest/doctest)** (`third_party/doctest/doctest.h`, v2.4.11). Daisy-dependent code is not built in this suite.

**Makefile (no CMake required):**

```bash
make -f Makefile.tests test
```

**CMake:**

```bash
cmake -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build
```

Tests live in `tests/test_utils.cpp`. Add cases there or split into more translation units as the suite grows.

## Requirements

- C++14 or later
- No library dependencies: the DSP code is self-contained (firmware projects still use [libDaisy](https://github.com/electro-smith/libDaisy) for the hardware itself)

## Author

ninetysix
