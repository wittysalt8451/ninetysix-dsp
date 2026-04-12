# sudwalfulkaan-dsp

Shared DSP effects and UI library for Súdwâlfulkaan eurorack modules.

## Structure

```
effects/    Pure DSP effects (filter, delay, saturation, envelope, etc.)
ui/         UI components (pot management, clock detection, gate utilities)
tools.cpp   Utility functions (mapping, slew limiter, envelope follower)
```

## Effects

| Module | Description |
|--------|-------------|
| SwfBiquad | Biquad filter (LP, HP, BP, notch, peak, shelves) |
| Envelope | ADSR envelope generator |
| Saturation | Multi-mode saturation/distortion |
| TapeDrive | Analog tape saturation emulation with LP filter |
| Tanh | Tanh soft saturation |
| SwfLimiter | Stereo limiter with envelope follower |
| Ducker | Stereo sidechain ducker |
| MidSide | Mid/side stereo width processing |
| LinkwitzRileyCrossover | Linkwitz-Riley crossover filter |
| StereoChorus | Stereo chorus (DaisySP) |
| StereoPhaser | Stereo phaser with LFO (DaisySP) |
| StereoReverbSc | Stereo reverb (DaisySP LGPL) |

## UI

| Module | Description |
|--------|-------------|
| UIManager | Pot smoothing, button/toggle handling with callbacks |
| ParamSmoother | One-pole parameter slew with change detection |
| ClockDetector | BPM/clock detection from gate input |
| GatePulse | Timed gate pulse generator for CV outputs |

## Usage

Link or symlink this directory as `library/` in your module project:

```bash
ln -s ../sudwalfulkaan-dsp library
```

Then include in your main source:

```cpp
#include "library/effects/SwfBiquad.h"
#include "library/ui/UIManager.h"
```

## Requirements

- C++17 or later
- [libDaisy](https://github.com/electro-smith/libDaisy)
- [DaisySP](https://github.com/electro-smith/DaisySP) (for chorus, phaser, reverb, biquad)

## Author

Súdwâlfulkaan
