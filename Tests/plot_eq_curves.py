#!/usr/bin/env python3
"""
Plot MachineEQ frequency response curves for Ampex ATR-102 and Studer A820
"""

import numpy as np
import matplotlib.pyplot as plt

# Measured frequency response data from Test_MachineEQ output (at 96kHz sample rate)

# From Test_MachineEQ output at 96kHz (current tuning)
frequencies = [20, 28, 30, 38, 40, 49.5, 50, 63, 69.5, 70, 72, 80, 100, 105, 110,
               125, 150, 160, 200, 250, 260, 315, 350, 400, 500, 630, 800, 1000,
               1200, 1250, 1600, 2000, 2500, 3000, 3150, 4000, 5000, 6300, 8000,
               10000, 12500, 16000, 20000, 21500]

# Ampex ATR-102 response (fine-tuned to Pro-Q4 reference)
ampex_response = [-2.67, -0.08, 0.22, 1.17, 1.15, 0.88, 0.85, 0.26, 0.15, 0.15,
                  0.23, 0.16, 0.31, 0.29, 0.25, 0.17, 0.03, -0.05, -0.25, -0.42,
                  -0.44, -0.48, -0.46, -0.41, -0.31, -0.23, -0.21, -0.26, -0.30,
                  -0.30, -0.25, -0.27, -0.37, -0.41, -0.40, -0.26, -0.13, -0.04,
                  0.01, 0.04, -0.01, -0.11, 0.04, 0.05]

# Studer A820 response (fine-tuned to Pro-Q4 reference)
# Features: 18dB/oct HP at 27Hz, head bumps at 49.5Hz and 110Hz
studer_response = [-8.36, -3.08, -1.90, -0.13, 0.30, 0.57, 0.63, 0.20, 0.09, 0.13,
                   0.29, 0.34, 1.23, 1.25, 1.24, 1.14, 0.64, 0.42, 0.02, -0.04,
                   -0.05, 0.01, 0.05, 0.08, 0.06, 0.03, 0.03, 0.03, 0.05, 0.05,
                   0.11, 0.15, 0.11, 0.06, 0.05, 0.03, 0.03, 0.03, 0.04, 0.06,
                   0.11, 0.24, 0.35, 0.33]

# Create the plot
fig, ax = plt.subplots(figsize=(12, 6))

# Plot both curves
ax.semilogx(frequencies, ampex_response, 'b-', linewidth=2, label='Ampex ATR-102 (Master)', marker='o', markersize=4)
ax.semilogx(frequencies, studer_response, 'r-', linewidth=2, label='Studer A820 (Tracks)', marker='s', markersize=4)

# Add reference line at 0dB
ax.axhline(y=0, color='gray', linestyle='--', linewidth=0.5, alpha=0.7)

# Formatting
ax.set_xlabel('Frequency (Hz)', fontsize=12)
ax.set_ylabel('Gain (dB)', fontsize=12)
ax.set_title('Machine EQ Frequency Response\n(Jack Endino Measurements)', fontsize=14, fontweight='bold')
ax.set_xlim(20, 20000)
ax.set_ylim(-10, 4)
ax.grid(True, which='both', linestyle='-', alpha=0.3)
ax.legend(loc='lower right', fontsize=11)

# Add frequency markers
ax.set_xticks([20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000])
ax.set_xticklabels(['20', '50', '100', '200', '500', '1k', '2k', '5k', '10k', '20k'])

# Add annotations for key features
ax.annotate('Head Bump\n+1.15dB @ 40Hz', xy=(40, 1.15), xytext=(80, 2.5),
            arrowprops=dict(arrowstyle='->', color='blue', alpha=0.7),
            fontsize=9, color='blue')

ax.annotate('Head Bump 1\n+0.6dB @ 50Hz', xy=(50, 0.63), xytext=(90, 2.0),
            arrowprops=dict(arrowstyle='->', color='red', alpha=0.7),
            fontsize=9, color='red')

ax.annotate('Head Bump 2\n+1.24dB @ 110Hz', xy=(110, 1.24), xytext=(200, 2.5),
            arrowprops=dict(arrowstyle='->', color='red', alpha=0.7),
            fontsize=9, color='red')

ax.annotate('18dB/oct HP\n-1.9dB @ 30Hz', xy=(30, -1.90), xytext=(50, -4),
            arrowprops=dict(arrowstyle='->', color='red', alpha=0.7),
            fontsize=9, color='red')

ax.annotate('Mid dip\n-0.46dB @ 350Hz', xy=(350, -0.46), xytext=(600, -2),
            arrowprops=dict(arrowstyle='->', color='blue', alpha=0.7),
            fontsize=9, color='blue')

ax.annotate('HF rise\n+0.35dB @ 20kHz', xy=(20000, 0.35), xytext=(8000, 1.5),
            arrowprops=dict(arrowstyle='->', color='red', alpha=0.7),
            fontsize=9, color='red')

plt.tight_layout()

# Save to file
output_path = '/Users/bensandoval/Desktop/MachineEQ_Curves.png'
plt.savefig(output_path, dpi=150, bbox_inches='tight')
print(f"Saved plot to: {output_path}")

# Also try to show it
plt.show()
