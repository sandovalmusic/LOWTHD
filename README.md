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
| **Master** | Ampex ATR-102 | Cleaner, faster, odd-dominant | ~0.38% | 0.54 |
| **Tracks** | Studer A820 | Softer, warmer, even-dominant | ~0.49% | 1.20 |

### AC Bias Shielding

Real tape uses AC bias to linearize magnetic response. Higher bias frequency = better HF preservation (less self-erasure). We simulate this with de-emphasis before saturation, re-emphasis after:

| Machine | Bias Frequency | Flat Through | @ 20kHz |
|---------|---------------|-------------|---------|
| **Ampex ATR-102** | 432 kHz | 8kHz | -8dB |
| **Studer A820** | 153.6 kHz | 6kHz | -12dB |

The ATR-102's exceptionally high 432 kHz bias (vs typical 150 kHz) was a major engineering achievement—it's why the machine was known for pristine HF response. The Studer uses standard professional bias frequency.

Result: HF content experiences less saturation (protected by simulated bias), while lows/mids get full tape character.

### Saturation Architecture

Three complementary stages:
1. **Jiles-Atherton Hysteresis** — Physics-based magnetic domain model (Newton-Raphson solver, 8-iteration limit). Blends in at higher levels.
2. **Asymmetric Tanh** — Primary harmonic generation with even/odd balance control
3. **Level-Dependent Atan** — Soft-clipping at high levels for MOL targeting

### Machine EQ (Head Bump)

Always-on EQ modeled from real machines at 30 IPS:

| Mode | Low End | Head Bump |
|------|---------|-----------|
| **Ampex** | 12dB/oct HP @ 20Hz | +1.15dB @ 40Hz |
| **Studer** | 18dB/oct HP @ 27Hz | +0.7dB @ 50Hz, +1.2dB @ 110Hz |

### HF Phase Smear

Head gap geometry creates frequency-dependent phase shifts. Corner frequencies based on actual head specs:

| Machine | Head Gap | Corner Freq |
|---------|----------|-------------|
| **Ampex** | 0.25μm ceramic (Flux Magnetics) | 10kHz |
| **Studer** | 3μm (1.317 playback head) | 2.8kHz |

**Note:** LOWTHD models a *transformerless* ATR-102—a common high-end studio modification that bypasses I/O transformers for cleaner signal path.

### Stereo Processing

- **Azimuth delay**: Right channel delayed (Ampex: 8μs, Studer: 12μs)
- **Crosstalk** (Studer only): -50dB mono bleed simulates adjacent track coupling

### Analog Variations

- **Wow**: Three-LFO head bump modulation (±0.08-0.12dB) with randomized phase per instance
- **Channel tolerance**: Randomized shelving EQ (±0.10-0.18dB) unique per plugin instance
- **Print-through** (Studer only): 65ms pre-echo at -58dB, signal-dependent (GP9 tape spec)

## Design Philosophy

**This plugin is not meant to be pushed hard.**

LOWTHD uses extremely level-dependent saturation tuned for realistic levels (-6dB to +3dB). The cumulative effect of subtle colorations creates the "tape sound":

- Hysteresis: ~0.01-0.05% THD with frequency-dependent phase
- Asymmetric saturation: <1% controlled harmonics
- AC bias shielding: up to 12dB frequency-dependent saturation difference
- Head bump: +1-2dB low frequency lift
- Phase smear: 10-21μs transient softening
- Azimuth: 8-12μs stereo decorrelation

**Recommended:** Keep Drive between -6dB and +3dB. Stack on multiple tracks at low drive for cumulative effect.

## THD Specifications

### Ampex ATR-102 — Target: MOL @ +12dB (3% THD), E/O = 0.503

| Level | THD | E/O |
|-------|-----|-----|
| -12 dB | 0.07% | 4.71 |
| -6 dB | 0.16% | 2.35 |
| 0 dB | 0.38% | 1.17 |
| +6 dB | 1.13% | 0.54 |
| +12 dB | 2.74% | 0.22 |

### Studer A820 — Target: MOL @ +9dB (3% THD), E/O = 1.122

| Level | THD | E/O |
|-------|-----|-----|
| -12 dB | 0.12% | 10.14 |
| -6 dB | 0.24% | 5.08 |
| 0 dB | 0.49% | 2.54 |
| +6 dB | 1.75% | 1.20 |
| +9 dB | 2.98% | 0.75 |

## Technical Details

### Oversampling & Latency

**2x minimum-phase IIR** — Gentle saturation allows 2x (vs 4x/8x typical for physics-based tape). Adds ~7 samples latency (<0.2ms at 44.1kHz). Suitable for tracking and live monitoring.

### Performance

Single 2x oversample, one J-A pass, efficient biquads. No neural networks or convolution. Multiple instances run simultaneously.

### Saturation Parameters

**Ampex:**
```
Tanh: drive=0.20, asymmetry=1.12 | Atan: drive=6.5, mixMax=0.75
J-A:  threshold=0.50, width=2.0  | a=50, k=0.005, c=0.95, α=1e-6
```

**Studer:**
```
Tanh: drive=0.12, asymmetry=1.38 | Atan: drive=5.5, mixMax=0.72, asym=1.42
J-A:  threshold=0.45, width=2.5  | a=35, k=0.01, c=0.92, α=1e-5
```

## Signal Flow

```
INPUT → Drive → 2x Upsample → Envelope Follower
      → De-emphasis (AC bias shielding)
      → [J-A Hysteresis] ─┬─ Parallel Blend → Re-emphasis
      → [Tanh → Atan]  ───┘
      → Machine EQ (head bump) → Dispersive Allpass (phase smear)
      → DC Block → Azimuth Delay (R channel)
      → 2x Downsample → Crosstalk (Studer) → Wow Modulation
      → Tolerance EQ → Print-through (Studer) → Volume → OUTPUT
```

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

DSP: Jiles-Atherton magnetic hysteresis, asymmetric tanh/atan saturation, AC bias shielding, dispersive allpass filtering.
