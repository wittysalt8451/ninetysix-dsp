# sudwalfulkaan-dsp

Shared DSP effects and UI library for Súdwâlfulkaan eurorack modules.

## Structure

```
effects/       Audio processors (filters, dynamics, saturation, spatial, modulation, reverb)
envelopes/     ADSR and other modulation envelopes (not bus effects)
generators/    Placeholder for future oscillators, LFOs, noise sources (see `.gitkeep`)
control/       Clock/gate/timing logic (clock from gate, gate pulses)
ui/            Human interface: pots, buttons, smoothing tied to controls
utils/         Hardware-agnostic helpers (mapping, slew, tempo math, level follower)
```

All types live in namespace `sudwalfulkaan`.

## Effects

| Module | Folder | Description |
|--------|--------|-------------|
| Biquad | filters | Biquad filter (LP, HP, BP, notch, peak, shelves) |
| LinkwitzRileyCrossover | filters | Linkwitz-Riley crossover filter |
| TapeDrive | saturation | Analog tape saturation emulation with LP filter |
| Tanh | saturation | Tanh soft saturation (`TanhSaturation` class) |
| Limiter | dynamics | Stereo limiter with envelope follower |
| Ducker | dynamics | Stereo sidechain ducker |
| MidSide | spatial | Mid/side stereo width processing |
| StereoChorus | modulation | Stereo chorus (DaisySP) |
| StereoPhaser | modulation | Stereo phaser with LFO (DaisySP) |
| StereoReverbSc | reverb | Stereo reverb (DaisySP LGPL) |

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
| Mapping | `MapLinear`, `MapLogarithmic` (normalized pot to range) |
| EnvelopeFollower | Smoothed stereo level follower (0..1), per-instance state |
| Slew | `SlewTowards` — one-pole step toward a target |
| Tempo | `CalculateReleaseTime` — BPM-synced release time in seconds |

Migrating from the old single `tools.cpp` is documented in [REFACTOR_TOOLS.md](REFACTOR_TOOLS.md). Folder layout changes (envelopes, control, generators) are in [REFACTOR_LAYOUT.md](REFACTOR_LAYOUT.md).

## Usage

Link or symlink this directory as `library/` in your module project:

```bash
ln -s ../sudwalfulkaan-dsp library
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

## Requirements

- C++17 or later
- [libDaisy](https://github.com/electro-smith/libDaisy)
- [DaisySP](https://github.com/electro-smith/DaisySP) (for chorus, phaser, reverb, biquad)

## Author

Súdwâlfulkaan
