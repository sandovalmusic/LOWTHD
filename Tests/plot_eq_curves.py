#!/usr/bin/env python3
"""
Plot MachineEQ frequency response curves for Ampex ATR-102 and Studer A820
"""

import numpy as np
import matplotlib.pyplot as plt

# Measured frequency response data from Test_MachineEQ output (at 96kHz sample rate)

frequencies = [20, 30, 40, 50, 63, 72, 80, 100, 125, 160, 200, 250, 315,
               400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150,
               4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000]

ampex_response = [-2.58, -0.17, 1.09, 0.80, 0.08, 0.23, 0.32, 0.52, 0.23, -0.18,
                  -0.45, -0.58, -0.58, -0.47, -0.36, -0.27, -0.21, -0.19, -0.20,
                  -0.24, -0.30, -0.37, -0.46, -0.53, -0.57, -0.56, -0.50, -0.40,
                  -0.26, -0.06, 0.11]

# Updated Studer response: fine-tuned to match Pro-Q4 reference targets
# Targets: 30Hz=-2dB, 38Hz=0dB, 49.5Hz=+0.55dB, 69.5Hz=+0.1dB, 110Hz=+1.2dB, 260Hz=+0.05dB
studer_response = [-8.35, -1.87, -0.08, 0.36, 0.20, 0.24, 0.29, 1.22, 1.14,
                   0.43, 0.04, -0.02, 0.03, 0.12, 0.18, 0.20, 0.18, 0.15, 0.13,
                   0.14, 0.14, 0.13, 0.11, 0.10, 0.09, 0.05, -0.02, -0.07, -0.06,
                   -0.03, -0.02]

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
ax.annotate('Head Bump\n+1.1dB @ 40Hz', xy=(40, 1.09), xytext=(80, 2.5),
            arrowprops=dict(arrowstyle='->', color='blue', alpha=0.7),
            fontsize=9, color='blue')

ax.annotate('Head Bump 1\n+0.65dB @ 50Hz', xy=(50, 0.70), xytext=(90, 2.5),
            arrowprops=dict(arrowstyle='->', color='red', alpha=0.7),
            fontsize=9, color='red')

ax.annotate('Head Bump 2\n+1.2dB @ 110Hz', xy=(110, 1.22), xytext=(200, 2.5),
            arrowprops=dict(arrowstyle='->', color='red', alpha=0.7),
            fontsize=9, color='red')

ax.annotate('18dB/oct HP', xy=(30, -1.87), xytext=(50, -4),
            arrowprops=dict(arrowstyle='->', color='red', alpha=0.7),
            fontsize=9, color='red')

ax.annotate('Mid dip\n-0.6dB', xy=(250, -0.58), xytext=(400, -2),
            arrowprops=dict(arrowstyle='->', color='blue', alpha=0.7),
            fontsize=9, color='blue')

plt.tight_layout()

# Save to file
output_path = '/Users/bensandoval/Desktop/MachineEQ_Curves.png'
plt.savefig(output_path, dpi=150, bbox_inches='tight')
print(f"Saved plot to: {output_path}")

# Also try to show it
plt.show()
