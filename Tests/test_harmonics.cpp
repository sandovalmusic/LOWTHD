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

int main() {
    double sr = 96000;
    
    std::cout << "HARMONIC DECAY ANALYSIS\n";
    std::cout << "=======================\n";
    std::cout << "Target: ~6-10dB decay per harmonic\n\n";
    
    for (int m = 0; m < 2; ++m) {
        double testLevel = (m == 0) ? 12.0 : 9.0;  // Ampex +12dB, Studer +9dB (at MOL)
        
        HybridTapeProcessor p;
        p.setSampleRate(sr);
        p.setParameters(m==0 ? 0.5 : 0.8, 1.0);
        p.reset();
        
        double amp = std::pow(10.0, testLevel/20.0);
        size_t N = 32768;
        std::vector<double> out(N);
        
        for (size_t i = 0; i < 8192; ++i) 
            p.processSample(amp * std::sin(2*M_PI*1000*i/sr));
        for (size_t i = 0; i < N; ++i) 
            out[i] = p.processSample(amp * std::sin(2*M_PI*1000*(8192+i)/sr));
        for (size_t i = 0; i < N; ++i) 
            out[i] *= 0.5*(1-std::cos(2*M_PI*i/N));
        
        std::vector<double> im(N, 0);
        fft(out, im);
        
        double bw = sr/N;
        auto mag = [&](double f) { 
            int b = std::round(f/bw); 
            return std::sqrt(out[b]*out[b]+im[b]*im[b])/(N/2); 
        };
        
        double h1=mag(1000), h2=mag(2000), h3=mag(3000), h4=mag(4000), h5=mag(5000), h6=mag(6000), h7=mag(7000);
        double h1dB = 20*std::log10(h1);
        
        std::cout << (m==0 ? "AMPEX @ +12dB (MOL):\n" : "\nSTUDER @ +9dB (MOL):\n");
        std::cout << "Harmonic   Level(dB)   Rel H1    Decay\n";
        std::cout << "----------------------------------------\n";
        
        double prev = h1dB;
        auto print = [&](const char* n, double h) {
            double dB = 20*std::log10(h + 1e-12);
            double rel = dB - h1dB;
            double decay = prev - dB;
            std::cout << "  " << n << "      " << std::fixed << std::setprecision(1) 
                      << std::setw(6) << dB << "    " << std::setw(6) << rel 
                      << "    " << std::setw(5) << decay << "\n";
            prev = dB;
        };
        
        std::cout << "  H1      " << std::fixed << std::setprecision(1) << std::setw(6) << h1dB << "      0.0    (ref)\n";
        print("H2", h2);
        print("H3", h3);
        print("H4", h4);
        print("H5", h5);
        print("H6", h6);
        print("H7", h7);
    }
    return 0;
}
