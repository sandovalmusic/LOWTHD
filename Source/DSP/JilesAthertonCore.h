#pragma once

#include <cmath>
#include <algorithm>

namespace TapeHysteresis {

// Jiles-Atherton Hysteresis Model
// Based on "Real-Time Physical Modelling for Analog Tape Machines" (DAFx 2019)
class JilesAthertonCore {
public:
    struct Parameters {
        double M_s = 350000.0;   // Saturation magnetization
        double a = 22000.0;      // Domain wall density
        double k = 27500.0;      // Coercivity
        double c = 1.7e-1;       // Reversibility
        double alpha = 1.6e-3;   // Mean field parameter
    };

    JilesAthertonCore() { reset(); }

    void setParameters(const Parameters& p) {
        params = p;
        oneOverA = 1.0 / params.a;
        cAlpha = params.c * params.alpha;
    }

    void setSampleRate(double sr) {
        T = 1.0 / sr;
    }

    void reset() {
        M_n1 = 0.0;
        H_n1 = 0.0;
    }

    double process(double H) {
        double H_d = (H - H_n1) / T;
        double M = solveNR8(H, H_d);
        H_n1 = H;
        M_n1 = M;
        return M;
    }

private:
    Parameters params;
    double T = 1.0 / 48000.0;
    double M_n1 = 0.0;
    double H_n1 = 0.0;
    double oneOverA = 1.0 / 22000.0;
    double cAlpha = 0.0;

    double langevin(double x) const {
        if (std::abs(x) < 1e-4) return x / 3.0;
        return 1.0 / std::tanh(x) - 1.0 / x;
    }

    double langevinD(double x) const {
        if (std::abs(x) < 1e-4) return 1.0 / 3.0;
        double cothX = 1.0 / std::tanh(x);
        return 1.0 / (x * x) - cothX * cothX + 1.0;
    }

    double solveNR8(double H, double H_d) {
        double delta = (H_d >= 0.0) ? 1.0 : -1.0;
        double M = M_n1;
        double denom = 1.0 - cAlpha;

        for (int i = 0; i < 8; ++i) {
            double H_eff = H + params.alpha * M;
            double x = H_eff * oneOverA;
            double M_an = params.M_s * langevin(x);
            double dM_an_dM = params.M_s * langevinD(x) * oneOverA * params.alpha;
            double M_diff = M_an - M;
            double delta_k = delta * params.k;

            double dM_dH = (std::abs(M_diff) > 1e-12 && delta * M_diff > 0)
                ? (M_diff / (delta_k - params.alpha * M_diff) + params.c * dM_an_dM) / denom
                : params.c * dM_an_dM / denom;

            double f = M - M_n1 - T * dM_dH * H_d;
            double df_denom = delta_k - params.alpha * M_diff;
            double df_dM = (std::abs(df_denom) > 1e-12)
                ? (dM_an_dM - 1.0) / df_denom / denom
                : 0.0;
            double f_prime = 1.0 - T * H_d * df_dM;

            if (std::abs(f_prime) > 1e-12) M -= f / f_prime;
            M = std::clamp(M, -params.M_s, params.M_s);
        }
        return M;
    }
};

} // namespace TapeHysteresis
