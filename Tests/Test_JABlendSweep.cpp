// J-A Blend Threshold Sweep Test
// Tests how lowering jaBlendThreshold affects THD curve shape
// Goal: Get closer to 2.8x THD increase per +3dB (cubic behavior)

#include "HybridTapeProcessor.h"
#include <cmath>
#include <vector>
#include <iostream>
#include <iomanip>

using namespace TapeHysteresis;

void fft(std::vector<double>& real, std::vector<double>& imag) {
    size_t n = real.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(real[i], real[j]); std::swap(imag[i], imag[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        double angle = -2.0 * M_PI / len;
        double wR = std::cos(angle), wI = std::sin(angle);
        for (size_t i = 0; i < n; i += len) {
            double cR = 1.0, cI = 0.0;
            for (size_t j = 0; j < len/2; ++j) {
                size_t u = i+j, v = i+j+len/2;
                double tR = cR*real[v] - cI*imag[v], tI = cR*imag[v] + cI*real[v];
                real[v] = real[u]-tR; imag[v] = imag[u]-tI;
                real[u] += tR; imag[u] += tI;
                double nR = cR*wR - cI*wI; cI = cR*wI + cI*wR; cR = nR;
            }
        }
    }
}

double measureTHD(HybridTapeProcessor& p, double levelDb, double sr) {
    p.reset();
    double amp = std::pow(10.0, levelDb/20.0);
    size_t N = 16384;
    std::vector<double> out(N);
    for (size_t i = 0; i < 4096; ++i) p.processSample(amp * std::sin(2*M_PI*1000*i/sr));
    for (size_t i = 0; i < N; ++i) out[i] = p.processSample(amp * std::sin(2*M_PI*1000*(4096+i)/sr));
    for (size_t i = 0; i < N; ++i) out[i] *= 0.5*(1-std::cos(2*M_PI*i/N));
    std::vector<double> im(N, 0);
    fft(out, im);
    double bw = sr/N;
    auto mag = [&](double f) { int b = std::round(f/bw); return std::sqrt(out[b]*out[b]+im[b]*im[b])/(N/2); };
    double h1=mag(1000), h2=mag(2000), h3=mag(3000), h4=mag(4000), h5=mag(5000);
    return 100.0 * std::sqrt(h2*h2+h3*h3+h4*h4+h5*h5) / h1;
}

double measureEO(HybridTapeProcessor& p, double levelDb, double sr) {
    p.reset();
    double amp = std::pow(10.0, levelDb/20.0);
    size_t N = 16384;
    std::vector<double> out(N);
    for (size_t i = 0; i < 4096; ++i) p.processSample(amp * std::sin(2*M_PI*1000*i/sr));
    for (size_t i = 0; i < N; ++i) out[i] = p.processSample(amp * std::sin(2*M_PI*1000*(4096+i)/sr));
    for (size_t i = 0; i < N; ++i) out[i] *= 0.5*(1-std::cos(2*M_PI*i/N));
    std::vector<double> im(N, 0);
    fft(out, im);
    double bw = sr/N;
    auto mag = [&](double f) { int b = std::round(f/bw); return std::sqrt(out[b]*out[b]+im[b]*im[b])/(N/2); };
    double h2=mag(2000), h3=mag(3000), h4=mag(4000), h5=mag(5000);
    double even = h2 + h4;
    double odd = h3 + h5;
    return (odd > 1e-10) ? even / odd : 0.0;
}

int main() {
    double sr = 96000;

    std::cout << "CURRENT BASELINE (before any changes)\n";
    std::cout << "======================================\n\n";

    std::cout << "Cubic target: ~2.8x THD increase per +3dB\n";
    std::cout << "Current behavior: ~1.5x per +3dB (too gentle)\n\n";

    // Test current settings
    for (int m = 0; m < 2; ++m) {
        HybridTapeProcessor p;
        p.setSampleRate(sr);
        p.setParameters(m==0 ? 0.5 : 0.8, 1.0);

        std::cout << (m==0 ? "AMPEX (threshold=0.77, width=1.5):\n" : "\nSTUDER (threshold=0.60, width=1.2):\n");
        std::cout << "Level      THD%      Ratio    E/O\n";
        std::cout << "------------------------------------\n";

        double prevTHD = 0;
        for (double lv = -6; lv <= 12; lv += 3) {
            double thd = measureTHD(p, lv, sr);
            double eo = measureEO(p, lv, sr);
            double ratio = (prevTHD > 0.001) ? thd / prevTHD : 0;
            std::cout << std::setw(4) << std::showpos << (int)lv << " dB    "
                      << std::noshowpos << std::fixed << std::setprecision(3) << std::setw(6) << thd << "%    ";
            if (lv > -6) std::cout << std::setw(4) << std::setprecision(2) << ratio << "x   ";
            else std::cout << "  --    ";
            std::cout << std::setprecision(2) << eo << "\n";
            prevTHD = thd;
        }
    }

    std::cout << "\n\n";
    std::cout << "ANALYSIS: What does lowering J-A threshold do?\n";
    std::cout << "================================================\n\n";

    std::cout << "Current Ampex: J-A kicks in at envelope > 0.77 (~-2dB peaks)\n";
    std::cout << "Current Studer: J-A kicks in at envelope > 0.60 (~-4dB peaks)\n\n";

    std::cout << "The J-A hysteresis model IS the cubic physics.\n";
    std::cout << "Lowering threshold = more J-A contribution at lower levels\n";
    std::cout << "                   = more cubic curve shape\n";
    std::cout << "                   = steeper THD rise\n\n";

    std::cout << "CONSIDERATION: Lowering threshold will:\n";
    std::cout << "  + Give more realistic cubic THD curve\n";
    std::cout << "  + Increase bass 'glue' from hysteresis memory\n";
    std::cout << "  - May increase THD at all levels (shift MOL down)\n";
    std::cout << "  - May affect E/O ratio (J-A is odd-dominant)\n\n";

    std::cout << "RECOMMENDATION:\n";
    std::cout << "Try Ampex threshold 0.40 (from 0.77) - 4x lower\n";
    std::cout << "Try Studer threshold 0.30 (from 0.60) - 2x lower\n";
    std::cout << "May need to reduce tanhDrive to compensate for higher THD\n";

    return 0;
}
