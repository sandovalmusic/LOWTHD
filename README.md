# Low THD Tape Simulator

Professional tape saturation plugin emulating the **Ampex ATR-102** (mastering) and **Studer A820** (tracking) tape machines with physics-based modeling.

## Features

### Two Machine Modes

| Mode | Machine | Use On | Character | THD @ 0dB | E/O Ratio |
|------|---------|--------|-----------|-----------|-----------|
| **Master** | Ampex ATR-102 | Master bus | Ultra-clean, transparent | ~0.08% | 0.53 (odd) |
| **Tracks** | Studer A820 | Individual tracks | Warm, punchy, musical | ~0.24% | 1.09 (even) |

**Tracks mode** emulates a Studer A820 24-track machine—the workhorse of multitrack recording. Use it on individual tracks (drums, bass, vocals, guitars) for warmth and punch.

**Master mode** emulates an Ampex ATR-102 stereo mastering deck—the gold standard for mixdown. Use it on your master bus for subtle glue and cohesion without coloring the mix.

### Hybrid Saturation Model

Combines three complementary saturation stages:

1. **Jiles-Atherton Hysteresis** - Physics-based magnetic domain modeling
   - History-dependent behavior ("tape memory")
   - Level-dependent blend (silent at low levels, fades in at high levels)
   - Adds magnetic character without affecting low-level transparency

2. **Asymmetric Tanh Saturation** - Primary harmonic generation
   - DC-bias approach for controlled even/odd harmonic balance
   - Ampex: 0.095 drive, 1.08 asymmetry (odd-dominant)
   - Studer: 0.14 drive, 1.18 asymmetry (even-dominant)

3. **Level-Dependent Atan** - High-level knee steepening
   - Blends in at high levels to hit MOL targets
   - Ampex: Symmetric atan (preserves odd-dominant character)
   - Studer: Asymmetric atan (preserves even-dominant character)

### Frequency-Dependent Saturation

Real tape machines use high-frequency bias—an ultrasonic signal (~100kHz) added during recording that linearizes the magnetic response for treble frequencies. This bias current "protects" high frequencies from the nonlinear saturation that affects bass and midrange.

The result: bass saturates and compresses while highs remain clean and extended. This is why tape sounds "warm" rather than "dull"—the saturation is frequency-selective by design.

We emulate this behavior with CCIR 30 IPS equalization curves:

- **De-emphasis** before saturation (cut highs ~7-9dB)
- Saturation processes the bass-heavy signal
- **Re-emphasis** after saturation (restore highs to original level)

Combined with the Jiles-Atherton hysteresis (which also affects bass more due to slower zero-crossings), this creates the authentic tape frequency response: warm, saturated low-end with open, airy highs.

### Philosophy: Subtle by Design

**This plugin is not meant to be pushed hard.**

Many tape plugins use relatively linear saturation curves that flatten dynamics when driven—the harder you push, the more everything gets squashed to the same level. This can sound impressive in isolation but kills transients and punch in a mix.

This plugin takes the opposite approach: **extremely level-dependent saturation curves** carefully tuned for realistic input levels. At -6dB to +3dB, you get the subtle nonlinearities that make tape sound like tape. The saturation stages (tanh, atan, J-A) only blend in progressively as level increases, preserving dynamics at normal operating levels.

Slamming the gain defeats the entire design. If you want aggressive tape "smash," other plugins will do that better. This one is built for the cumulative effect of many small colorations:

- **Hysteresis** adds ~0.01-0.05% THD with frequency-dependent phase shifts
- **Asymmetric saturation** generates controlled even/odd harmonics at <1%
- **De-emphasis/re-emphasis** creates ~3-4dB of frequency-dependent saturation difference
- **Azimuth delay** adds 8-12μs of stereo decorrelation

Individually, these are nearly imperceptible. Together, they create the "tape sound"—fuller bass, cohesive mids, open highs, and stereo width that makes sources sit better in a mix.

**Recommended usage:**
- Keep Drive between -6dB and +3dB for mixing
- Use +6dB to +9dB only for intentional color
- Stack on multiple tracks at low drive for cumulative effect
- The plugin does the most when you notice it the least

### Auto-Gain Compensation

When you turn **Drive** up, **Volume** automatically comes down to maintain constant monitoring level. You hear more saturation without volume changes.

### Stereo Processing

- **Left channel**: Direct processing
- **Right channel**: Processing + azimuth delay
  - Ampex: 8 microseconds
  - Studer: 12 microseconds
- Creates authentic tape stereo width and "air"

## Controls

| Control | Range | Default | Function |
|---------|-------|---------|----------|
| **Machine Mode** | Master / Tracks | Master | Ampex ATR-102 or Studer A820 |
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
│    │                                                                 │
│    ↓                                                                 │
│  De-emphasis (CCIR 30 IPS) ──── Cut highs before saturation         │
│    │                                                                 │
│    ├─────────────────────────────┐                                   │
│    │                             │                                   │
│    ↓                             ↓                                   │
│  J-A Hysteresis            Asymmetric Tanh                          │
│  (magnetic memory)         (primary THD)                             │
│    │                             │                                   │
│    │                             ↓                                   │
│    │                       Level-Dependent Atan                      │
│    │                       (knee steepening)                         │
│    │                             │                                   │
│    ↓                             ↓                                   │
│  Level-Dependent Blend ←─────────┘                                   │
│    │                                                                 │
│    ↓                                                                 │
│  Re-emphasis (CCIR 30 IPS) ──── Restore highs after blend           │
│    │                                                                 │
│    ↓                                                                 │
│  DC Block (4th-order Butterworth @ 5Hz)                             │
│    │                                                                 │
│    ↓                                                                 │
│  Output                                                              │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘

                    RIGHT CHANNEL ONLY
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  Output → Azimuth Delay → Final Output                              │
│           (Ampex: 8μs, Studer: 12μs)                                │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

## THD Specifications

### Ampex ATR-102 (Master Mode)
| Level | Measured THD | Target (Median) | E/O Ratio |
|-------|--------------|-----------------|-----------|
| -12 dB | 0.012% | 0.025% | 0.53 |
| -6 dB | 0.025% | 0.050% | 0.53 |
| 0 dB | 0.082% | 0.100% | 0.53 |
| +3 dB | 0.160% | 0.200% | 0.53 |
| +6 dB | 0.365% | 0.425% | 0.53 |
| +9 dB | 0.902% | 0.900% | 0.53 |

### Studer A820 (Tracks Mode)
| Level | Measured THD | Target (Median) | E/O Ratio |
|-------|--------------|-----------------|-----------|
| -12 dB | 0.046% | 0.035% | 1.09 |
| -6 dB | 0.097% | 0.070% | 1.09 |
| 0 dB | 0.242% | 0.275% | 1.09 |
| +3 dB | 0.438% | 0.575% | 1.09 |
| +6 dB | 1.067% | 1.300% | 1.09 |
| +9 dB | 2.001% | 2.750% | 1.09 |

## Technical Details

### Saturation Parameters

**Ampex ATR-102 (Master):**
```
Tanh:  drive=0.095, asymmetry=1.08
Atan:  drive=5.0, mixMax=0.65, threshold=0.5, width=2.5
       symmetric (odd harmonics only)
J-A:   blendMax=1.0, threshold=0.77, width=1.5
```

**Studer A820 (Tracks):**
```
Tanh:  drive=0.14, asymmetry=1.18
Atan:  drive=5.5, mixMax=0.72, threshold=0.4, width=2.5
       asymmetric=1.25 (preserves even harmonics)
J-A:   blendMax=1.0, threshold=0.60, width=1.2
```

### Oversampling

**2x minimum-phase IIR** was a deliberate choice:

- **Why 2x?** This plugin is low-THD by design—the saturation is gentle enough that 2x provides sufficient anti-aliasing without the CPU cost of 4x or 8x
- **Why minimum-phase?** Linear-phase filters preserve transients perfectly, but real tape doesn't. Minimum-phase adds subtle transient softening that brings us closer to the machine emulations
- **The tradeoff:** We get natural-sounding transient behavior *and* adequate aliasing suppression in one stage

Latency is reported to DAW for automatic compensation.

### Unity Gain at Defaults

- Default Drive: -6dB (0.5x)
- Final makeup: +6dB (2.0x)
- Net result: Unity gain with clean headroom before saturation

### Jiles-Atherton Hysteresis: Bass "Stickiness"

The Jiles-Atherton model simulates how magnetic domains in tape oxide behave. When the audio signal changes direction, the magnetic particles don't respond instantly—they exhibit *hysteresis*, a lag that depends on the signal's history.

**Why this matters for bass:**
- Low frequencies have slower zero-crossings, giving domains more time to "stick"
- The coercivity (k) parameter controls how much force is needed to flip domains
- Higher coercivity = more stickiness = bass notes feel "thicker" and more sustained
- This is why tape makes kicks and bass guitar sound fuller without EQ

**How the blend works:**
- At low levels: Hysteresis path is silent (clean, transparent response)
- At high levels: Hysteresis fades in via envelope follower
- Result: Clean quiet passages, magnetic warmth on loud transients

**Ampex (Master):**
- Domain density (a): 200 - Nearly linear response
- Coercivity (k): 0.001 - Minimal stickiness (clean mastering character)
- Reversibility (c): 0.9999 - Domains flip easily

**Studer (Tracks):**
- Domain density (a): 120 - Softer saturation knee
- Coercivity (k): 0.003 - More stickiness (punchy tracking character)
- Reversibility (c): 0.995 - Slight domain memory = bass "glue"

## Pre-Built Plugins

Pre-built macOS plugins are available in the `Builds/macOS/` directory:
- `Low THD Tape Sim.vst3`
- `Low THD Tape Sim.component` (AU)

Copy to your plugin folders:
```bash
cp -R "Builds/macOS/Low THD Tape Sim.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "Builds/macOS/Low THD Tape Sim.component" ~/Library/Audio/Plug-Ins/Components/
```

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
- JUCE 8.0.4 (fetched automatically via CMake)

## Project Structure

```
LOWTHD/
├── README.md
├── Source/DSP/
│   ├── HybridTapeProcessor.cpp    # Main saturation engine
│   ├── HybridTapeProcessor.h
│   ├── JilesAthertonCore.h        # Physics-based hysteresis
│   ├── PreEmphasis.cpp            # CCIR 30 IPS EQ (Re-emphasis/De-emphasis)
│   └── PreEmphasis.h
├── Plugin/
│   ├── CMakeLists.txt
│   └── Source/
│       ├── PluginProcessor.cpp    # JUCE wrapper, auto-gain, oversampling
│       ├── PluginProcessor.h
│       ├── PluginEditor.cpp       # UI
│       └── PluginEditor.h
└── Tests/
    ├── run_all_tests.sh           # Master test runner
    ├── Test_THDAccuracy.cpp       # THD vs level validation
    ├── Test_HarmonicBalance.cpp   # E/O ratio validation
    ├── Test_FrequencyResponse.cpp # Frequency response tests
    ├── Test_Transparency.cpp      # Low-level purity tests
    ├── Test_PhaseCoherence.cpp    # Parallel path validation
    ├── Test_Stereo.cpp            # Stereo and azimuth tests
    └── Test_Stability.cpp         # Edge case validation
```

## Credits

Developed by Ben Sandoval

DSP algorithms based on:
- Jiles-Atherton magnetic hysteresis modeling for tape memory effects
- Asymmetric tanh/atan saturation with DC-bias for even/odd harmonic control
- CCIR 30 IPS de-emphasis/re-emphasis for frequency-dependent saturation
