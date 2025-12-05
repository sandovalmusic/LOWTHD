# Low THD Tape Simulator

[![Build](https://github.com/sandovalmusic/LOWTHD/actions/workflows/build.yml/badge.svg)](https://github.com/sandovalmusic/LOWTHD/actions/workflows/build.yml)

Physics-based tape saturation plugin emulating the **Ampex ATR-102** (transformerless) and **Studer A820**.

## Download

**[Download Latest Builds](https://github.com/sandovalmusic/LOWTHD/actions/workflows/build.yml)** — Click the latest successful run, scroll to "Artifacts":

| Platform | Format | File |
|----------|--------|------|
| **Windows** | VST3 | `LOWTHD-Windows-VST3.zip` |
| **macOS** | VST3/AU | `LOWTHD-macOS-VST3.zip` / `LOWTHD-macOS-AU.zip` |
| **Linux** | VST3 | `LOWTHD-Linux-VST3.zip` |

## Why Physics-Based Modeling

The electronics in professional tape machines (ATR-102, A820) measure extremely linearly—they were precision instruments. The "tape sound" comes from magnetic recording physics: ferromagnetic hysteresis, AC bias effects, head gap phase smear, and nonlinear compression near saturation.

LOWTHD models the tape and heads, not the amplifiers that were designed to stay out of the way.

## Controls

| Control | Range | Default | Function |
|---------|-------|---------|----------|
| **Mode** | Master / Tracks | Master | Ampex ATR-102 or Studer A820 |
| **Drive** | -12dB to +18dB | -6dB | Input level into saturation |
| **Volume** | -20dB to +9.5dB | 0dB | Output level (auto-compensated) |

## Features

### Machine Modes

| Mode | Machine | Character | THD @ 0dB | E/O Ratio |
|------|---------|-----------|-----------|-----------|
| **Master** | Ampex ATR-102 | Cleaner, faster, odd-dominant | ~0.32% | 0.45 |
| **Tracks** | Studer A820 | Softer, warmer, even-dominant | ~0.95% | 1.06 |

### AC Bias Shielding (Parallel Clean HF Path)

Real tape uses AC bias (~150kHz oscillator) to linearize the magnetic recording process. The bias signal "shields" high frequencies from nonlinear distortion—higher bias frequency = better HF linearity.

**Why parallel paths?** On real tape, HF content genuinely experiences less distortion because the bias keeps the magnetic particles in their linear operating region. A simple EQ cut/boost would still saturate the HF content and then try to restore it—that's not what happens on tape. Our parallel architecture models the actual physics:

```
Input ──┬── HFCut ─────────→ Saturation ──┬── Sum → Output
        │                                  │
        └── (Input - HFCut) ───────────────┘
             Clean HF (no saturation)
```

- **HFCut path**: LF/mid content goes through full saturation chain
- **Clean HF path**: High frequencies bypass saturation entirely, then sum back in

This creates frequency-dependent THD that matches real machines: full saturation on lows/mids, progressively less distortion at higher frequencies where bias is most effective.

| Machine | Bias Frequency | HF Saturation Reduction |
|---------|---------------|------------------------|
| **Ampex ATR-102** | 432 kHz | -8dB @ 20kHz (flat to 8kHz) |
| **Studer A820** | 153.6 kHz | -12dB @ 20kHz (flat to 6kHz) |

The ATR-102's exceptionally high 432 kHz bias was a major engineering achievement—it's why the machine was known for pristine HF response even when driven hard.

### Saturation Architecture

Three complementary stages with level-dependent blending:

1. **Asymmetric Tanh** — Primary saturation curve with even/odd harmonic balance control
2. **Jiles-Atherton Hysteresis** — Physics-based magnetic domain model, blends in at higher levels for authentic tape compression
3. **Level-Dependent Atan** — Soft-clipping knee at high levels for MOL targeting

The saturation blend adapts to input level via envelope follower with cubic smoothstep transitions.

### Machine EQ (Head Bump)

Always-on EQ modeled from Jack Endino and EMC Published Specs for real machines at 30 IPS:

| Mode | Low End | Head Bump | Character |
|------|---------|-----------|-----------|
| **Ampex** | HP @ 20.8Hz | +1.1dB @ 40Hz | -2.7dB @ 20Hz, -0.5dB @ 300Hz, -0.4dB @ 3kHz |
| **Studer** | 18dB/oct HP @ 22Hz | +0.6dB @ 50Hz, +1.2dB @ 110Hz | Dual head bump, -5dB @ 20Hz, -2.5dB @ 28Hz |

### HF Phase Smear

Head gap geometry creates frequency-dependent phase shifts via cascaded allpass filters. Corner frequencies based on actual head specs:

| Machine | Head Gap | Corner Freq |
|---------|----------|-------------|
| **Ampex** | 0.25μm ceramic (Flux Magnetics) | 10kHz |
| **Studer** | 3μm (1.317 playback head) | 2.8kHz |

**Note:** LOWTHD models a *transformerless* ATR-102—a common high-end studio modification that bypasses I/O transformers for cleaner signal path.

### Stereo Processing

- **Azimuth delay**: Right channel delayed (Ampex: 8μs, Studer: 12μs)
- **Crosstalk** (Studer only): -55dB mono bleed simulates adjacent track coupling

### Analog Variations

- **Wow**: Three-LFO head bump modulation (±0.08-0.12dB) with randomized phase per instance
- **Channel tolerance**: Randomized shelving EQ (±0.10-0.18dB) unique per plugin instance
- **Print-through** (Studer only): 65ms pre-echo at -58dB, signal-dependent (GP9 tape spec)

## Design Philosophy

**This plugin is not meant to be pushed hard.**

LOWTHD uses extremely level-dependent saturation tuned for realistic levels (-6dB to +3dB). The cumulative effect of subtle colorations creates the "tape sound":

- Asymmetric saturation: <1% controlled harmonics at 0dB
- Jiles-Atherton hysteresis: Physics-based compression at higher levels
- AC bias shielding: HF bypasses saturation (parallel clean path)
- Head bump: +1-2dB low frequency lift
- Phase smear: 10-21μs transient softening
- Azimuth: 8-12μs stereo decorrelation

**Recommended:** Keep Drive between -6dB and +3dB. Stack on multiple tracks at low drive for cumulative effect.

## THD Specifications

Measured at 100Hz (where AC bias shielding has minimal effect):

### Ampex ATR-102 — Target: MOL @ +12dB (3% THD), E/O = 0.5

| Level | THD | E/O |
|-------|-----|-----|
| -12 dB | 0.06% | — |
| -6 dB | 0.13% | 1.82 |
| 0 dB | 0.32% | 0.91 |
| +6 dB | 1.02% | 0.45 |
| +12 dB | 3.60% | 0.20 |

### Studer A820 — Target: MOL @ +9dB (3% THD), E/O = 1.12

| Level | THD | E/O |
|-------|-----|-----|
| -12 dB | 0.22% | — |
| -6 dB | 0.45% | 4.27 |
| 0 dB | 0.95% | 2.12 |
| +6 dB | 2.19% | 1.06 |
| +9 dB | 3.42% | 0.74 |

## Technical Details

### Oversampling & Latency

**2x minimum-phase IIR** — Gentle saturation allows 2x (vs 4x/8x typical for physics-based tape). Adds ~7 samples latency (<0.2ms at 44.1kHz). Suitable for tracking and live monitoring.

### Performance

Single 2x oversample, efficient biquads, no neural networks or convolution. Multiple instances run simultaneously.

### Saturation Parameters

**Ampex ATR-102:**
```
Tanh: drive=0.175, asymmetry=1.15
Atan: drive=4.0, mixMax=0.60, threshold=2.5
J-A:  blendMax=0.70, threshold=1.0, width=2.5
      a=50, k=0.005, c=0.96, α=2e-7
```

**Studer A820:**
```
Tanh: drive=0.24, asymmetry=1.35
Atan: drive=4.0, mixMax=0.65, threshold=2.0, asym=1.20
J-A:  blendMax=0.75, threshold=0.8, width=2.5
      a=45, k=0.008, c=0.92, α=5e-6
```

## Signal Flow

```
INPUT → Drive → 2x Upsample → Envelope Follower
                                    │
                    ┌───────────────┴───────────────┐
                    ↓                               ↓
                 HFCut                        Clean HF Path
                    │                         (Input - HFCut)
        ┌───────────┴───────────┐                   │
        ↓                       ↓                   │
    Tanh → Atan              J-A Core               │
    (primary sat)         (hysteresis)              │
        │                       │                   │
        └───────┬───────────────┘                   │
                ↓                                   │
          Blend (level-dependent)                   │
                │                                   │
                └───────────────┬───────────────────┘
                                ↓
                    Sum (saturated + cleanHF)
                                ↓
                    Machine EQ (head bump)
                                ↓
                    Dispersive Allpass (phase smear)
                                ↓
                    DC Block → Azimuth Delay (R)
                                ↓
                    2x Downsample
                                ↓
                    Crosstalk (Studer) → Wow
                                ↓
                    Tolerance EQ → Print-through (Studer)
                                ↓
                           Volume → OUTPUT
```

**Three parallel paths:**
1. **Tanh → Atan** (primary saturation with level-dependent soft knee)
2. **Jiles-Atherton** (physics-based hysteresis, blends in at higher levels)
3. **Clean HF** (bypasses saturation entirely, preserves AC-bias-shielded frequencies)

**Latency:** ~7 samples @ 44.1kHz (~0.16ms)

## Building

```bash
cd Plugin && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release && make -j8
```

Requirements: CMake 3.22+, C++17, macOS 10.13+ (JUCE 8.0.4 fetched automatically)

## Project Structure

```
LOWTHD/
├── Source/DSP/
│   ├── HybridTapeProcessor.cpp/h   # Main saturation engine
│   ├── JilesAthertonCore.h         # Physics-based hysteresis
│   ├── BiasShielding.cpp/h         # AC bias shielding curves
│   └── MachineEQ.cpp/h             # Head bump EQ
└── Plugin/Source/
    ├── PluginProcessor.cpp/h       # JUCE wrapper
    └── PluginEditor.cpp/h          # UI
```

## Credits

Developed by Ben Sandoval

DSP: Jiles-Atherton magnetic hysteresis, asymmetric tanh/atan saturation, parallel AC bias shielding, dispersive allpass filtering.
