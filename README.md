# Low THD Tape Simulator

[![Build](https://github.com/sandovalmusic/LOWTHD/actions/workflows/build.yml/badge.svg)](https://github.com/sandovalmusic/LOWTHD/actions/workflows/build.yml)

Physics-based tape saturation plugin emulating the **Ampex ATR-102** and **Studer A820**.

## Download

**[Download Latest Builds](https://github.com/sandovalmusic/LOWTHD/actions/workflows/build.yml)** — Click the latest successful run, scroll to "Artifacts", and download for your platform:

| Platform | Format | File |
|----------|--------|------|
| **Windows** | VST3 | `LOWTHD-Windows-VST3.zip` |
| **macOS** | VST3 | `LOWTHD-macOS-VST3.zip` |
| **macOS** | AU | `LOWTHD-macOS-AU.zip` |
| **Linux** | VST3 | `LOWTHD-Linux-VST3.zip` |

## Why Physics-Based Modeling

Other plugin developers have sought to model these machines by meticulously recreating their electronic components—input transformers, repro amplifiers, bias oscillators, output stages. This approach assumes the "tape sound" emerges from the cumulative coloration of the signal path.

However, the electronics in professional tape machines measure extremely linearly. The Ampex ATR-102 and Studer A820 were precision instruments designed for mastering and multitrack recording. Their amplifiers, transformers, and signal paths were engineered to be as transparent as possible. Many studios even modified their machines to bypass the input/output transformers entirely, preferring the cleaner signal path.

The sound of these machines is not in their electronics. It is in the physics of magnetic recording: the hysteresis behavior of ferromagnetic particles, the frequency-dependent effects of AC bias, the phase smear of the playback head, and the nonlinear compression as signal levels approach tape saturation.

LOWTHD models what actually matters—the tape and heads—rather than the amplifiers that were designed to stay out of the way.

## Features

### Two Machine Modes

| Mode | Machine | Use On | Character | THD @ 0dB | E/O Ratio |
|------|---------|--------|-----------|-----------|-----------|
| **Master** | Ampex ATR-102 | Master bus | Cleaner, faster | ~0.08% | 0.53 (odd) |
| **Tracks** | Studer A820 | Individual tracks | Softer, warmer | ~0.24% | 1.09 (even) |

### AC Bias Simulation

Professional tape machines use AC bias—a high-frequency signal (approximately 150 kHz) combined with the audio before recording. This bias current keeps the magnetic particles in constant motion, linearizing their response at normal operating levels. The characteristic nonlinearity only appears when signal levels approach tape saturation.

The Jiles-Atherton hysteresis model is a physics-based description of ferromagnetic behavior, but the standard implementation models *unbiased* magnetic recording—appropriate for consumer-grade equipment, not professional machines running calibrated bias.

LOWTHD simulates the effect of proper bias through three mechanisms:

**Frequency-Selective Saturation**

AC bias partially erases high-frequency content, which is why professional tape maintains treble clarity while saturating the bass and midrange. We replicate this by applying the CCIR 30 IPS de-emphasis curve before saturation (cutting highs ~7-9dB), then restoring them afterward with the inverse curve.

**Level-Dependent Nonlinearity**

On a properly-biased machine, hysteresis is inaudible at normal levels—it only emerges as signal approaches the maximum output level (MOL). The plugin implements this through a threshold-based crossfade: at nominal levels the signal path is nearly linear; as input increases, hysteresis and saturation progressively engage.

**Dynamic Parameter Modulation**

Within the Jiles-Atherton equations, parameters adjust in real-time based on signal level. Lower levels produce more linear behavior (simulating effective bias); higher levels shift toward full ferromagnetic nonlinearity (simulating bias breakdown).

The result: LOWTHD models tape as a recording medium rather than an effect. The nonlinearity is emergent—a consequence of signal exceeding what the virtual bias can linearize—not a static curve applied uniformly.

### Saturation Architecture

The plugin combines three complementary saturation stages:

1. **Jiles-Atherton Hysteresis** — Physics-based magnetic domain modeling with history-dependent behavior. Blends in only at higher levels.

2. **Asymmetric Tanh Saturation** — Primary harmonic generation with DC-bias for even/odd balance. Ampex is odd-dominant (0.53 ratio); Studer is even-dominant (1.09 ratio).

3. **Level-Dependent Atan** — Soft-clipping that engages at high levels to hit MOL targets.

### Machine EQ (Head Bump)

Each machine includes a modest head bump modeled after the total output EQ of both respective machines running at 30 IPS. These frequency response curves are always active based on the selected mode.

| Mode | Character | Low End | Head Bump |
|------|-----------|---------|-----------|
| **Ampex** | Cleaner, tighter | HP @ 20Hz | +1.15dB @ 40Hz |
| **Studer** | Fuller, warmer | 18dB/oct HP @ 27Hz | +0.7dB @ 50Hz, +1.2dB @ 110Hz |

### HF Phase Smear

Real tape heads create frequency-dependent phase shifts—higher frequencies experience slightly more delay due to head gap geometry. This subtly softens transients without dulling the highs.

We emulate this with a 4-stage dispersive allpass cascade:

| Mode | Phase @ 8kHz | Transient Smear |
|------|--------------|-----------------|
| Ampex | -124° | 10μs |
| Studer | -116° | 21μs |

### Stereo Processing

- **Left channel**: Direct processing
- **Right channel**: Processing + azimuth delay (Ampex: 8μs, Studer: 12μs)

Creates authentic tape stereo width without phase cancellation.

### Auto-Gain Compensation

When Drive increases, Volume automatically decreases to maintain constant monitoring level.

## Design Philosophy

**This plugin is not meant to be pushed hard.**

Many tape plugins flatten dynamics when driven—the harder you push, the more everything gets squashed. This can sound impressive in isolation but kills transients in a mix.

LOWTHD takes the opposite approach: extremely level-dependent saturation curves tuned for realistic input levels (-6dB to +3dB). The processing stages blend in progressively, preserving dynamics at normal operating levels.

The cumulative effect of many small colorations creates the "tape sound":

- Hysteresis: ~0.01-0.05% THD with frequency-dependent phase
- Asymmetric saturation: controlled harmonics at <1%
- De/re-emphasis: ~3-4dB frequency-dependent saturation difference
- Head bump: +1-2dB low frequency lift per machine
- Phase smear: 10-21μs transient softening
- Azimuth delay: 8-12μs stereo decorrelation

Individually, nearly imperceptible. Together, fuller bass, cohesive mids, smooth highs, and stereo width.

**Recommended usage:**
- Keep Drive between -6dB and +3dB for mixing
- Use +6dB to +9dB only for intentional color
- Stack on multiple tracks at low drive for cumulative effect

## Controls

| Control | Range | Default | Function |
|---------|-------|---------|----------|
| **Mode** | Master / Tracks | Master | Ampex ATR-102 or Studer A820 |
| **Drive** | -12dB to +18dB | -6dB | Input level into saturation |
| **Volume** | -20dB to +9.5dB | 0dB | Final output level |

## Signal Flow

```
                           PLUGIN WRAPPER
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  Input → Drive → [2x Oversample] → TAPE PROCESSOR → [Downsample]    │
│                                           │                          │
│                                           ↓                          │
│                              Volume (+6dB makeup) → Output           │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘

                    TAPE PROCESSOR (HybridTapeProcessor)
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  Input                                                               │
│    ↓                                                                 │
│  De-emphasis (CCIR 30 IPS) ──── Cut highs before saturation         │
│    │                                                                 │
│    ├─────────────────────────────┐                                   │
│    ↓                             ↓                                   │
│  J-A Hysteresis            Asymmetric Tanh                          │
│  (magnetic memory)         (primary THD)                             │
│    │                             │                                   │
│    │                             ↓                                   │
│    │                       Level-Dependent Atan                      │
│    │                       (knee steepening)                         │
│    ↓                             ↓                                   │
│  Level-Dependent Blend ←─────────┘                                   │
│    ↓                                                                 │
│  Re-emphasis (CCIR 30 IPS) ──── Restore highs                       │
│    ↓                                                                 │
│  HF Dispersive Allpass (4-stage)                                    │
│    ↓                                                                 │
│  Machine EQ (head bump, always-on per mode)                         │
│    ↓                                                                 │
│  DC Block (4th-order Butterworth @ 5Hz)                             │
│    ↓                                                                 │
│  Output (+ Azimuth Delay on right channel)                          │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

## THD Specifications

THD targets are intentionally approximate—we traded some accuracy to let the Jiles-Atherton hysteresis play a bigger role. The frequency-dependent behavior and magnetic "memory" matter more than hitting exact distortion percentages.

### Ampex ATR-102 (Master Mode)
| Level | Measured THD | Target | E/O Ratio |
|-------|--------------|--------|-----------|
| -12 dB | 0.012% | 0.025% | 0.53 |
| -6 dB | 0.025% | 0.050% | 0.53 |
| 0 dB | 0.082% | 0.100% | 0.53 |
| +3 dB | 0.160% | 0.200% | 0.53 |
| +6 dB | 0.365% | 0.425% | 0.53 |
| +9 dB | 0.902% | 0.900% | 0.53 |

### Studer A820 (Tracks Mode)
| Level | Measured THD | Target | E/O Ratio |
|-------|--------------|--------|-----------|
| -12 dB | 0.046% | 0.035% | 1.09 |
| -6 dB | 0.097% | 0.070% | 1.09 |
| 0 dB | 0.242% | 0.275% | 1.09 |
| +3 dB | 0.438% | 0.575% | 1.09 |
| +6 dB | 1.067% | 1.300% | 1.09 |
| +9 dB | 2.001% | 2.750% | 1.09 |

## Technical Details

### Oversampling

**2x minimum-phase IIR** was a deliberate choice:

- **Why 2x?** The saturation is gentle enough that 2x provides sufficient anti-aliasing without the CPU cost of 4x or 8x
- **Why minimum-phase?** Linear-phase preserves transients perfectly, but real tape doesn't. Minimum-phase adds subtle transient softening that matches the emulation
- **Why not 4x or 8x?** More filter passes can over-soften transients, making plugins sound flat or lifeless

### Near-Zero Latency

An unintended benefit of this design: the 2x minimum-phase IIR oversampling adds approximately 7 samples—less than 0.2ms at 44.1kHz.

Most physics-based tape simulations require significant oversampling to handle nonlinear hysteresis without aliasing. ChowTape, the other major Jiles-Atherton implementation, recommends "as much oversampling as your CPU will allow" and offers up to 16x. This produces excellent results but introduces latency unsuitable for real-time monitoring.

LOWTHD's subtle saturation keeps nonlinear content low enough that 2x suffices. The hysteresis is still physically accurate—it simply operates in a regime where extreme oversampling isn't necessary.

The result: a physics-based tape simulation suitable for tracking and live monitoring, not just mixing.

### Low CPU Usage

The same design choices minimize CPU consumption. Single 2x oversampling stage, one Jiles-Atherton pass, straightforward filters. No iterative solvers at 16x sample rate, no neural network inference, no convolution.

Multiple instances run simultaneously without significant system impact.

### Jiles-Atherton Parameters

The J-A model simulates magnetic domain behavior. Low frequencies have slower zero-crossings, giving domains more time to "stick"—this is why tape makes bass feel thicker without EQ.

**Ampex (Master):**
- Domain density (a): 200 — Nearly linear
- Coercivity (k): 0.001 — Minimal stickiness
- Reversibility (c): 0.9999 — Domains flip easily

**Studer (Tracks):**
- Domain density (a): 120 — Softer knee
- Coercivity (k): 0.003 — More stickiness
- Reversibility (c): 0.995 — Slight domain memory ("bass glue")

### Saturation Parameters

**Ampex ATR-102:**
```
Tanh:  drive=0.095, asymmetry=1.08
Atan:  drive=5.0, mixMax=0.65, threshold=0.5, width=2.5, symmetric
J-A:   blendMax=1.0, threshold=0.77, width=1.5
```

**Studer A820:**
```
Tanh:  drive=0.14, asymmetry=1.18
Atan:  drive=5.5, mixMax=0.72, threshold=0.4, width=2.5, asymmetric=1.25
J-A:   blendMax=1.0, threshold=0.60, width=1.2
```

### Unity Gain at Defaults

- Default Drive: -6dB (0.5x)
- Final makeup: +6dB (2.0x)
- Net result: Unity gain with headroom before saturation

## Building from Source

```bash
cd Plugin
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
```

### Requirements

- CMake 3.22+
- C++17 compiler
- macOS 10.13+
- JUCE 8.0.4 (fetched automatically)

## Project Structure

```
LOWTHD/
├── Source/DSP/
│   ├── HybridTapeProcessor.cpp/h   # Main saturation engine
│   ├── JilesAthertonCore.h         # Physics-based hysteresis
│   ├── PreEmphasis.cpp/h           # CCIR 30 IPS EQ
│   └── MachineEQ.cpp/h             # Head bump EQ per machine
└── Plugin/Source/
    ├── PluginProcessor.cpp/h       # JUCE wrapper, oversampling
    └── PluginEditor.cpp/h          # UI
```

## Credits

Developed by Ben Sandoval

DSP based on Jiles-Atherton magnetic hysteresis, asymmetric tanh/atan saturation, CCIR 30 IPS equalization, and dispersive allpass filtering.
