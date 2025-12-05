# LOWTHD Target Specifications

Comprehensive reference document for all calibration targets used in LOWTHD tape emulation.

---

## THD (Total Harmonic Distortion) Targets

Based on published specifications and measurements of real tape machines at 30 IPS.

### Ampex ATR-102 (Master Mode)

| Level | Target THD | Achieved | Error |
|-------|-----------|----------|-------|
| -12 dB | ~0.005% | 0.005% | — |
| -6 dB | 0.02% | 0.024% | +21% |
| 0 dB | 0.08% | 0.078% | -2.5% |
| +3 dB | ~0.16% | 0.140% | — |
| +6 dB | 0.40% | 0.381% | -4.7% |
| +12 dB (MOL) | 3.0% | — | — |

**Curve Shape (THD ratio per 3dB):**
- Target: ~2x per 3dB (cubic behavior)
- Achieved: -6→0dB: 3.2x, 0→+3dB: 1.8x, +3→+6dB: 2.7x

### Studer A820 (Tracks Mode)

| Level | Target THD | Achieved | Error |
|-------|-----------|----------|-------|
| -12 dB | ~0.02% | 0.024% | +20% |
| -6 dB | 0.07% | 0.068% | -2.3% |
| 0 dB | 0.25% | 0.280% | +12% |
| +3 dB | ~0.50% | 0.488% | — |
| +6 dB | 1.25% | 1.130% | -9.6% |
| +9 dB (MOL) | 3.0% | — | — |

**Curve Shape (THD ratio per 3dB):**
- Target: ~2x per 3dB (cubic behavior)
- Achieved: -6→0dB: 4.1x, 0→+3dB: 1.7x, +3→+6dB: 2.3x

---

## Even/Odd Harmonic Ratio (E/O) Targets

Ratio of 2nd harmonic (H2) to 3rd harmonic (H3) measured at 0dB input level.

| Machine | Target E/O | Achieved | Error | Character |
|---------|-----------|----------|-------|-----------|
| **Ampex ATR-102** | 0.50 | 0.54 | +7.7% | Odd-dominant |
| **Studer A820** | 1.12 | 1.17 | +4.0% | Even-dominant |

**Implementation:** Global DC input bias applied before all saturation stages:
- Ampex: inputBias = 0.06 (small bias, odd-dominant)
- Studer: inputBias = 0.22 (larger bias, even-dominant)

---

## AC Bias Shielding (HF Saturation Reduction)

The AC bias frequency determines how much high-frequency content is "shielded" from saturation.

### Ampex ATR-102 (432 kHz bias)

| Frequency | Target Reduction | Achieved | Error |
|-----------|-----------------|----------|-------|
| 1 kHz | 0.0 dB | 0.03 dB | — |
| 5 kHz | 0.0 dB | 0.51 dB | — |
| 6 kHz | -0.5 dB | 0.36 dB | — |
| 8 kHz | -2.0 dB | -1.76 dB | ±0.24 dB |
| 10 kHz | -4.5 dB | -4.73 dB | ±0.23 dB |
| 12 kHz | -6.5 dB | -7.69 dB | ±1.19 dB |
| 14 kHz | -8.5 dB | -9.96 dB | ±1.46 dB |
| 16 kHz | -10.0 dB | -11.58 dB | ±1.58 dB |
| 20 kHz | -12.0 dB | -13.31 dB | ±1.31 dB |

**Tolerance:** ±2.0 dB (PASS)

### Studer A820 (153.6 kHz bias)

| Frequency | Target Reduction | Achieved | Error |
|-----------|-----------------|----------|-------|
| 1 kHz | 0.0 dB | 0.03 dB | — |
| 6 kHz | 0.0 dB | 0.42 dB | — |
| 7 kHz | -0.3 dB | -0.35 dB | ±0.05 dB |
| 8 kHz | -1.0 dB | -1.47 dB | ±0.47 dB |
| 10 kHz | -3.0 dB | -4.10 dB | ±1.10 dB |
| 12 kHz | -5.0 dB | -6.61 dB | ±1.61 dB |
| 14 kHz | -6.5 dB | -8.35 dB | ±1.85 dB |
| 16 kHz | -8.0 dB | -9.52 dB | ±1.52 dB |
| 20 kHz | -10.0 dB | -10.81 dB | ±0.81 dB |

**Tolerance:** ±2.0 dB (PASS)

---

## Machine EQ (Head Bump)

Based on Jack Endino and EMC published specifications at 30 IPS.

### Ampex ATR-102

| Frequency | Target | Notes |
|-----------|--------|-------|
| 20 Hz | -2.7 dB | HP rolloff |
| 28 Hz | 0.0 dB | Reference |
| 40 Hz | +1.15 dB | Head bump peak |
| 70 Hz | +0.17 dB | Post-bump |
| 105 Hz | +0.3 dB | Secondary lift |
| 150 Hz | 0.0 dB | Flat |
| 300 Hz | -0.5 dB | Mid scoop |
| 1 kHz | 0.0 dB | Reference |
| 3 kHz | -0.45 dB | Presence cut |
| 5 kHz | 0.0 dB | Flat |
| 10 kHz | 0.0 dB | Flat |
| 16 kHz | -0.25 dB | Air rolloff |

### Studer A820

| Frequency | Target | Notes |
|-----------|--------|-------|
| 20 Hz | -5.0 dB | 18dB/oct HP rolloff |
| 28 Hz | -2.5 dB | HP rolloff |
| 40 Hz | 0.0 dB | Reference |
| 50 Hz | +0.55 dB | First head bump |
| 70 Hz | +0.1 dB | Post-bump dip |
| 110 Hz | +1.2 dB | Second head bump |
| 160 Hz | -0.5 dB | Post-bump dip |
| 2 kHz | +0.05 dB | Subtle presence |
| 10 kHz | -0.1 dB | Air rolloff |

---

## HF Phase Smear (Dispersive Allpass)

Head gap geometry creates frequency-dependent phase shifts.

| Machine | Head Gap | Corner Frequency | Phase @ 10kHz |
|---------|----------|-----------------|---------------|
| **Ampex** | 0.25μm (Flux Magnetics ceramic) | 10 kHz | ~29° |
| **Studer** | 3μm (1.317 playback head) | 2.8 kHz | ~43° |

**Implementation:** 4-stage cascaded allpass filters with corner frequencies at:
- Base frequency × 2^(n × 0.5) for n = 0, 1, 2, 3

---

## Stereo Processing

### Azimuth Delay

| Machine | Target Delay | Achieved Samples @ 96kHz |
|---------|-------------|-------------------------|
| **Ampex** | 8 μs | 0.768 samples |
| **Studer** | 12 μs | 1.152 samples |

**Phase at 10kHz:**
- Ampex: 28.8° (target ~29°)
- Studer: 43.2° (target ~43°)

### Crosstalk (Studer only)

| Frequency | Target Level |
|-----------|-------------|
| 50 Hz | < -55 dB (HP active) |
| 1 kHz | -55 dB ±1 dB |
| 12 kHz | < -55 dB (LP active) |

---

## Print-Through (Studer only)

Based on GP9 tape specifications.

| Parameter | Target | Achieved |
|-----------|--------|----------|
| Level @ unity input | -58 dB | -57.7 dB |
| Pre-echo timing | -65ms | Correct |
| Signal dependency | 100:1 ratio | 100:1 ratio |
| Noise floor gate | Active below -60dB | Active |

---

## Analog Variations

### Wow (Both machines)

| Parameter | Ampex | Studer |
|-----------|-------|--------|
| Depth | ±0.08 dB | ±0.12 dB |
| LFO 1 Rate | 0.5 Hz | 0.5 Hz |
| LFO 2 Rate | 0.33 Hz | 0.33 Hz |
| LFO 3 Rate | 0.15 Hz | 0.15 Hz |

### Channel Tolerance

| Parameter | Range |
|-----------|-------|
| Shelving EQ variation | ±0.10-0.18 dB |
| Per-instance randomization | Unique seed per plugin |

---

## Saturation Parameters

### Ampex ATR-102

```
Global Bias: 0.06

Jiles-Atherton:
  a = 50.0      (domain wall density)
  k = 0.005     (pinning constant)
  c = 0.96      (domain wall flexibility)
  α = 2.0e-7    (interdomain coupling)
  blendMax = 0.005
  blendThreshold = 0.05
  blendWidth = 0.45

Atan:
  drive = 0.6
  mix = 0.25
  threshold = 0.18
  width = 2.2
```

### Studer A820

```
Global Bias: 0.22

Jiles-Atherton:
  a = 45.0      (domain wall density)
  k = 0.008     (pinning constant)
  c = 0.92      (domain wall flexibility)
  α = 5.0e-6    (interdomain coupling)
  blendMax = 0.012
  blendThreshold = 0.02
  blendWidth = 0.48

Atan:
  drive = 0.95
  mix = 0.35
  threshold = 0.20
  width = 1.8
```

---

## DC Blocking

| Parameter | Target |
|-----------|--------|
| High-pass frequency | 5 Hz |
| Filter order | 4th-order Butterworth |
| DC rejection | > -160 dB |
| 100 Hz passthrough | 0.0 dB |

---

## References

- Ampex ATR-102 Service Manual
- Studer A820 Technical Specifications
- Jack Endino frequency response measurements
- EMC Published Specs for 30 IPS operation
- GP9 tape specifications (print-through)
- Flux Magnetics head specifications
