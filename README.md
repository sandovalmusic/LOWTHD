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
| **Master** | Ampex ATR-102 | Master bus | Cleaner, faster | ~0.38% | 0.54 (odd) |
| **Tracks** | Studer A820 | Individual tracks | Softer, warmer | ~0.49% | 1.20 (even) |

### AC Bias Simulation

Professional tape machines use AC bias—a high-frequency signal (approximately 150 kHz) combined with the audio before recording. This bias current keeps the magnetic particles in constant motion, linearizing their response at normal operating levels. The characteristic nonlinearity only appears when signal levels approach tape saturation.

The Jiles-Atherton hysteresis model is a physics-based description of ferromagnetic behavior, but the standard implementation models *unbiased* magnetic recording—appropriate for consumer-grade equipment, not professional machines running calibrated bias.

LOWTHD simulates the effect of proper bias through three mechanisms:

**Frequency-Selective Saturation (AC Bias Shielding)**

Real tape machines use a ~150kHz AC bias signal that keeps the magnetic particles in constant motion, linearizing their response. At low and mid frequencies, the bias is fully effective—but as audio wavelengths approach the bias wavelength, the shielding effect breaks down and saturation increases.

Since we can't actually inject 150kHz bias into a digital model, we simulate its *effect* on the frequency response. Before saturation, we apply machine-specific de-emphasis curves that model where bias stops being effective:

| Machine | Flat Through | Rolloff Start | @ 20kHz |
|---------|-------------|---------------|---------|
| **Ampex ATR-102** | 6kHz | 7kHz | -12dB |
| **Studer A820** | 7kHz | 8kHz | -10dB |

The wider head gap on the Ampex means bias loses effectiveness at slightly lower frequencies. The Studer's narrower multitrack heads and higher bias oscillator frequency keep bias effective slightly longer.

After saturation, we restore the highs with the exact inverse curve—frequencies that were cut experience less distortion (protected by the simulated bias), while the lows and mids get the full saturation character.

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
| **Ampex** | Cleaner, tighter | 12db/oct HP @ 20Hz | +1.15dB @ 40Hz, +0.3db @105hz |
| **Studer** | Fuller, warmer | 18dB/oct HP @ 27Hz | +0.7dB @ 50Hz, +1.2dB @ 110Hz |

### HF Phase Smear

Real tape heads create frequency-dependent phase shifts—higher frequencies experience slightly more delay due to head gap geometry. This subtly softens transients without dulling the highs.

The physics: phase smear onset frequency relates to head gap width via **f_null = tape_speed / gap_width**. Wider gaps create earlier phase effects.

We emulate this with a 4-stage dispersive allpass cascade, with corner frequencies based on actual head gap specifications:

| Machine | Head Gap | Corner Freq | Character |
|---------|----------|-------------|-----------|
| **Ampex ATR-102** | 0.25μm ceramic | 10kHz | Minimal smear |
| **Studer A820** | 3μm (1.317 playback) | 2.8kHz | More pronounced HF smear |

The ATR-102's ceramic mastering head (Flux Magnetics spec) has a gap 12× narrower than the Studer's playback head, resulting in phase effects only at the very top of the audio spectrum.

**Note:** LOWTHD models a *transformerless* ATR-102 configuration. Many studios modified their ATR-102s to bypass the input/output transformers for a cleaner signal path—this was a common high-end modification, not stock. The transformerless config further reduces reactive contributions to phase smear.

### Stereo Processing

- **Left channel**: Direct processing
- **Right channel**: Processing + azimuth delay (Ampex: 8μs, Studer: 12μs)
- **Studer only**: Mono crosstalk filter (-50dB) simulates adjacent track bleed

Creates authentic tape stereo width without phase cancellation. The Studer crosstalk adds subtle cohesion typical of multitrack machines.

### Head Bump Modulation (Wow)

Real tape transport mechanisms exhibit subtle speed variations called "wow"—low-frequency oscillations in the capstan motor and tape tension. This causes amplitude modulation in the head bump frequency region as the effective tape-to-head speed varies.

LOWTHD models this with three incommensurate LFO frequencies (0.31Hz, 0.63Hz, 1.07Hz) combined for organic, non-repeating modulation:

| Mode | Bump Center | Modulation Depth |
|------|-------------|------------------|
| **Ampex** | 40Hz | ±0.08dB |
| **Studer** | 75Hz | ±0.12dB |

The LFO phases are randomized per plugin instance, so multiple instances won't pulse in sync. This adds subtle, realistic movement to the low end without obvious pumping.

### Channel Tolerance Variation

Professional tape machines were precision instruments, but manufacturing tolerances meant no two channels measured identically. The Studer A820 specified ±1dB from 60Hz-20kHz and ±2dB at the frequency extremes.

LOWTHD models this with randomized shelving EQ at the output stage, using machine-specific tolerances representing freshly calibrated machines ready for a session:

| Machine | Low Shelf | High Shelf |
|---------|-----------|------------|
| **Ampex ATR-102** | 60Hz ±4Hz, ±0.10dB | 16kHz ±400Hz, ±0.12dB |
| **Studer A820** | 75Hz ±6Hz, ±0.15dB | 15kHz ±500Hz, ±0.18dB |

The ATR-102 has tighter tolerances—it was THE precision mastering deck. The A820 multitrack shows slightly more channel-to-channel variation, even when freshly aligned.

Each plugin instance gets unique random values at instantiation. Stereo instances have independent L/R tolerances; mono instances use the same tolerance for both channels.

This creates subtle track-to-track frequency response differences when using multiple plugin instances—just like running audio through different channels of a real multitrack machine.

### Print-Through (Studer Only)

Print-through is magnetic bleed between adjacent tape layers on a wound reel. When tape sits in storage, the magnetic flux from one layer gradually transfers to the layers above and below it, creating ghost copies of the audio. On playback, you hear a faint pre-echo of loud passages approximately 65ms before they occur (the time offset comes from tape layer spacing at 30 IPS).

Multitrack reels were more susceptible than 2-track masters—more layers in contact, often stored longer between sessions, and the economics of studio time meant tapes weren't always stored "tails out" (rewound, which reduces print-through).

LOWTHD models signal-dependent print-through in Studer mode, calibrated for GP9 tape:

| Parameter | Value | Notes |
|-----------|-------|-------|
| **Delay** | 65ms | Tape layer spacing at 30 IPS (1.5 mil tape) |
| **Base Level** | -58dB | GP9 spec (~3dB less than older 456 formulation) |
| **Scaling** | Quadratic | Louder signals = proportionally more print-through |
| **Noise Gate** | -60dB | Prevents artifacts on quiet signals |

The level scales with signal amplitude because magnetic bleed is proportional to recorded flux—louder passages magnetize the tape more strongly, creating more transfer to adjacent layers. This is why print-through is most noticeable before loud transients like drum hits or vocal attacks.

### Auto-Gain Compensation

When Drive increases, Volume automatically decreases to maintain constant monitoring level.

## Design Philosophy

**This plugin is not meant to be pushed hard.**

Many tape plugins flatten dynamics when driven—the harder you push, the more everything gets squashed. This can sound impressive in isolation but kills transients in a mix.

LOWTHD takes the opposite approach: extremely level-dependent saturation curves tuned for realistic input levels (-6dB to +3dB). The processing stages blend in progressively, preserving dynamics at normal operating levels.

The cumulative effect of many small colorations creates the "tape sound":

- Hysteresis: ~0.01-0.05% THD with frequency-dependent phase
- Asymmetric saturation: controlled harmonics at <1%
- De/re-emphasis: up to 12dB frequency-dependent saturation difference (AC bias shielding)
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
INPUT (from DAW)
     │
     ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│                         PLUGIN PROCESSOR                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  1. INPUT TRIM (Drive)                                                      │
│     └─ Multiply by inputTrimValue (0.25x to 8.0x, default 0.5x = -6dB)     │
│     └─ Peak level measured here for metering                                │
│                                                                             │
│  2. UPSAMPLE 2x (JUCE Oversampling)                                        │
│     └─ Half-band polyphase IIR filter (minimum phase)                       │
│     └─ Sample rate: 48kHz → 96kHz (or whatever 2x base rate)               │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                    HYBRID TAPE PROCESSOR (per channel)                      │
│                    Runs at 2x oversampled rate                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  3. ENVELOPE FOLLOWER                                                       │
│     └─ Track signal level for blend decisions                               │
│     └─ Attack: 0.002 coeff (~3ms), Release: 0.020 coeff (~30ms)            │
│                                                                             │
│  4. DE-EMPHASIS (AC Bias Shielding)                                        │
│     └─ 5-stage biquad EQ modeling where bias stops being effective         │
│     └─ Ampex: Flat to 6kHz, -4.5dB@10kHz, -12dB@20kHz                      │
│     └─ Studer: Flat to 7kHz, -3dB@10kHz, -10dB@20kHz                       │
│                                                                             │
│  5. SATURATION PATH A: JILES-ATHERTON HYSTERESIS                           │
│     │  └─ Physics-based magnetic domain model                               │
│     │  └─ Newton-Raphson solver for M(H) relationship                       │
│     │  └─ Ampex: a=50, k=0.005, c=0.95, α=1e-6                             │
│     │  └─ Studer: a=35, k=0.01, c=0.92, α=1e-5                             │
│     │  └─ Input scaled by jaInputScale (1.0)                               │
│     │  └─ Output scaled by jaOutputScale (Ampex:150, Studer:105)           │
│     │                                                                       │
│  6. SATURATION PATH B: ASYMMETRIC TANH                                     │
│     │  └─ asymmetricTanh(x) = (tanh(drive*(x+bias)) - dcOffset) * normFactor│
│     │  └─ Ampex: drive=0.20, asymmetry=1.12 (odd-dominant)                 │
│     │  └─ Studer: drive=0.12, asymmetry=1.38 (even-dominant)               │
│     │                                                                       │
│  7. LEVEL-DEPENDENT ATAN (series after tanh)                               │
│     │  └─ Blends in at high levels for knee steepening                     │
│     │  └─ Ampex: symmetric atan, drive=6.5, mixMax=0.75                    │
│     │  └─ Studer: asymmetric atan, drive=5.5, mixMax=0.72, asym=1.42       │
│     │  └─ Crossfade based on envelope vs threshold                          │
│     │                                                                       │
│  8. PARALLEL BLEND (J-A + Tanh paths)                                      │
│     └─ jaBlend = smoothstep based on envelope level                         │
│     └─ Ampex: threshold=0.50, width=2.0, max=1.0                           │
│     └─ Studer: threshold=0.45, width=2.5, max=1.0                          │
│     └─ output = jaPath * jaBlend + tanhPath * (1 - jaBlend)                │
│                                                                             │
│  9. RE-EMPHASIS (AC Bias Shielding Inverse)                                │
│     └─ 5-stage biquad EQ (exact inverse of de-emphasis)                    │
│     └─ Ampex: +4.5dB@10kHz, +12dB@20kHz                                    │
│     └─ Studer: +3dB@10kHz, +10dB@20kHz                                     │
│                                                                             │
│ 10. MACHINE EQ (Head Bump - always on)                                     │
│     └─ AMPEX ATR-102:                                                       │
│     │  └─ HP 20.8Hz Q=0.707                                                │
│     │  └─ Bell 28Hz Q=2.5 +1.0dB                                           │
│     │  └─ Bell 40Hz Q=1.8 +1.35dB (head bump)                              │
│     │  └─ Bell 70Hz Q=3.0 -0.1dB                                           │
│     │  └─ Bell 105Hz Q=2.0 +0.3dB                                          │
│     │  └─ Bell 150Hz Q=2.0 +0.1dB                                          │
│     │  └─ Bell 300Hz Q=0.8 -0.5dB                                          │
│     │  └─ Bell 1200Hz Q=1.5 -0.2dB                                         │
│     │  └─ Bell 3000Hz Q=1.2 -0.4dB                                         │
│     │  └─ Bell 16000Hz Q=1.5 -0.4dB                                        │
│     │  └─ Bell 20000Hz Q=0.6 +0.45dB                                       │
│     │  └─ LP 40000Hz (6dB/oct)                                             │
│     │                                                                       │
│     └─ STUDER A820:                                                         │
│        └─ HP 27Hz Q=1.0 (12dB/oct)                                         │
│        └─ HP 27Hz (6dB/oct) → total 18dB/oct                               │
│        └─ Bell 49.5Hz Q=1.5 +0.6dB (head bump 1)                           │
│        └─ Bell 72Hz Q=2.07 -1.0dB (dip)                                    │
│        └─ Bell 110Hz Q=1.0 +1.8dB (head bump 2)                            │
│        └─ Bell 180Hz Q=1.0 -0.7dB                                          │
│        └─ Bell 400Hz Q=1.5 +0.1dB                                          │
│        └─ Bell 2000Hz Q=1.5 +0.15dB                                        │
│        └─ Bell 10000Hz Q=2.5 0dB                                           │
│        └─ Bell 20000Hz Q=1.2 +0.35dB                                       │
│                                                                             │
│ 11. HF DISPERSIVE ALLPASS (4 stages)                                       │
│     └─ Creates frequency-dependent phase shift (head gap smear)            │
│     └─ Ampex: corner=10kHz, stages at 10k, 14.1k, 20k, 28.3kHz            │
│     └─ Studer: corner=2.8kHz, stages at 2.8k, 4k, 5.6k, 7.9kHz            │
│     └─ Each stage: 1st-order allpass (half-octave spacing)                 │
│                                                                             │
│ 12. DC BLOCKING (4th-order Butterworth @ 5Hz)                              │
│     └─ Two cascaded 2nd-order highpass biquads                             │
│     └─ Removes any DC offset from asymmetric saturation                    │
│                                                                             │
│ 13. AZIMUTH DELAY (Right channel only)                                     │
│     └─ Linear interpolated fractional delay                                 │
│     └─ Ampex: 8μs delay                                                    │
│     └─ Studer: 12μs delay                                                  │
│     └─ Buffer size: 8 samples (supports up to 384kHz)                      │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                    BACK TO PLUGIN PROCESSOR                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 14. DOWNSAMPLE 2x (JUCE Oversampling)                                      │
│     └─ Half-band polyphase IIR filter (minimum phase)                       │
│     └─ Sample rate: 96kHz → 48kHz                                          │
│                                                                             │
│ 15. CROSSTALK (Studer mode only, stereo only)                              │
│     └─ mono = (L + R) * 0.5                                                │
│     └─ Highpass 100Hz Q=0.707                                              │
│     └─ Lowpass 8000Hz Q=0.707                                              │
│     └─ Multiply by 0.00316 (-50dB, Studer A820 spec)                       │
│     └─ Add to both L and R channels                                         │
│                                                                             │
│ 16. HEAD BUMP MODULATION (Both modes)                                      │
│     └─ LFO: sin(0.63Hz)*0.5 + sin(1.07Hz)*0.3 + sin(0.31Hz)*0.2           │
│     └─ Updated once per block (block-rate, ~0.6Hz effective)               │
│     └─ Bandpass filter isolates bump region                                 │
│     │  └─ Ampex: 40Hz center, Q=0.7                                        │
│     │  └─ Studer: 75Hz center, Q=0.7                                       │
│     └─ Modulation depth:                                                    │
│     │  └─ Ampex: ±0.08dB (±0.009 linear)                                   │
│     │  └─ Studer: ±0.12dB (±0.014 linear)                                  │
│     └─ bumpSignal = bandpass(input)                                        │
│     └─ output = input + bumpSignal * (modGain - 1.0)                       │
│                                                                             │
│ 17. TOLERANCE EQ (Machine-specific, randomized per instance)               │
│     └─ Models channel-to-channel frequency response variations             │
│     └─ Freshly calibrated machine tolerances:                              │
│     │  └─ Ampex: 60Hz ±4Hz ±0.10dB, 16kHz ±400Hz ±0.12dB                  │
│     │  └─ Studer: 75Hz ±6Hz ±0.15dB, 15kHz ±500Hz ±0.18dB                 │
│     └─ Stereo: L and R get independent random values                       │
│     └─ Mono: Same tolerance applied to both channels                       │
│     └─ Randomized once per plugin instantiation (unique per instance)      │
│                                                                             │
│ 18. PRINT-THROUGH (Studer mode only, GP9 tape)                             │
│     └─ Simulates magnetic bleed between tape layers on the reel            │
│     └─ Creates subtle pre-echo 65ms before the main signal                 │
│     └─ Signal-dependent: louder passages create more print-through         │
│     │  └─ Base coefficient: -58dB at unity (0.00126, GP9 spec)             │
│     │  └─ Level scales quadratically with signal amplitude                 │
│     │  └─ Noise floor gate at -60dB prevents artifacts on quiet signals    │
│     └─ Multitrack tape (Studer) had more print-through than 2-track        │
│     └─ Real-world phenomenon from tape reels sitting in storage            │
│                                                                             │
│ 19. OUTPUT TRIM (Volume)                                                   │
│     └─ Multiply by outputTrimValue (0.1x to 3.0x, default 1.0x)           │
│     └─ Auto-linked to input trim for gain compensation                      │
│                                                                             │
│ 20. FINAL MAKEUP GAIN                                                      │
│     └─ Fixed +6dB (2.0x) to compensate for default -6dB input trim         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
     │
     ▼
OUTPUT (to DAW)
```

**Total latency:** ~7 samples at 44.1kHz (~0.16ms) from the 2x IIR oversampling.

## THD Specifications

Tuned for realistic MOL (Maximum Output Level) and E/O (Even/Odd harmonic ratio) targets based on real machine measurements. Full Jiles-Atherton hysteresis blends in at high levels for authentic magnetic behavior.

### Ampex ATR-102 (Master Mode)
**Target: MOL @ +12dB (3% THD), E/O = 0.503 (odd-dominant)**

| Level | Measured THD | E/O Ratio |
|-------|--------------|-----------|
| -12 dB | 0.07% | 4.71 |
| -6 dB | 0.16% | 2.35 |
| 0 dB | 0.38% | 1.17 |
| +3 dB | 0.63% | 0.83 |
| +6 dB | 1.13% | 0.54 |
| +9 dB | 2.14% | 0.34 |
| +12 dB | 2.74% | 0.22 |

### Studer A820 (Tracks Mode)
**Target: MOL @ +9dB (3% THD), E/O = 1.122 (even-dominant)**

| Level | Measured THD | E/O Ratio |
|-------|--------------|-----------|
| -12 dB | 0.12% | 10.14 |
| -6 dB | 0.24% | 5.08 |
| 0 dB | 0.49% | 2.54 |
| +3 dB | 0.79% | 1.79 |
| +6 dB | 1.75% | 1.20 |
| +9 dB | 2.98% | 0.75 |
| +12 dB | 2.38% | 0.47 |

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

The same design choices minimize CPU consumption. Single 2x oversampling stage, one Jiles-Atherton pass (using a Newton-Raphson solver with 8-iteration limit), and efficient biquad filters. The bias shielding curves use 5 cascaded biquads per channel—no neural network inference or convolution.

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
Tanh:  drive=0.20, asymmetry=1.12
Atan:  drive=6.5, mixMax=0.75, threshold=0.35, width=2.0, symmetric
J-A:   blendMax=1.0, threshold=0.50, width=2.0
```

**Studer A820:**
```
Tanh:  drive=0.12, asymmetry=1.38
Atan:  drive=5.5, mixMax=0.72, threshold=0.35, width=2.5, asymmetric=1.42
J-A:   blendMax=1.0, threshold=0.45, width=2.5
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
│   ├── BiasShielding.cpp/h         # AC bias shielding curves per machine
│   └── MachineEQ.cpp/h             # Head bump EQ per machine
└── Plugin/Source/
    ├── PluginProcessor.cpp/h       # JUCE wrapper, oversampling
    └── PluginEditor.cpp/h          # UI
```

## Credits

Developed by Ben Sandoval

DSP based on Jiles-Atherton magnetic hysteresis, asymmetric tanh/atan saturation, AC bias shielding modeling, and dispersive allpass filtering.
