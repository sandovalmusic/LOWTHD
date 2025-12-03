#pragma once

#include <cmath>
#include <algorithm>

namespace TapeHysteresis {

/**
 * Jiles-Atherton Hysteresis Model Core
 *
 * Based on: "Real-Time Physical Modelling for Analog Tape Machines" (DAFx 2019)
 * by Jatin Chowdhury
 *
 * This is a clean implementation focused on the physics:
 * - Langevin function for anhysteretic magnetization
 * - Differential equation for magnetization dynamics
 * - Newton-Raphson solver for implicit integration
 *
 * Parameters:
 * - M_s: Saturation magnetization
 * - a: Domain wall density (shape of anhysteretic curve)
 * - k: Coercivity (hysteresis loop width)
 * - c: Reversibility (ratio of reversible to irreversible magnetization)
 * - alpha: Mean field parameter (domain coupling)
 */
class JilesAthertonCore {
public:
    // J-A Model Parameters
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
        // Precompute derived values
        oneOverA = 1.0 / params.a;
        Q = (1.0 - params.c) / (1.0 - params.c * params.alpha);
        cAlpha = params.c * params.alpha;
    }

    void setSampleRate(double sr) {
        sampleRate = sr;
        T = 1.0 / sr;
    }

    /**
     * Modulate the 'a' parameter (domain density) for level-dependent linearity
     * Higher 'a' = more linear response (used at low input levels)
     * Lower 'a' = more nonlinear/saturating response (used at higher levels)
     * This mimics how AC bias linearizes the response at low signal levels
     */
    void modulateLinearity(double newA) {
        params.a = newA;
        oneOverA = 1.0 / newA;
    }

    void reset() {
        M_n1 = 0.0;
        H_n1 = 0.0;
        H_d_n1 = 0.0;
    }

    /**
     * Process a single sample through J-A model
     * @param H Magnetic field (input signal)
     * @return M Magnetization (output signal)
     */
    double process(double H) {
        // Derivative of H (for differential equation)
        double H_d = (H - H_n1) / T;

        // Solve using Newton-Raphson (8 iterations for accuracy)
        double M = solveNR8(H, H_d);

        // Update state
        H_n1 = H;
        H_d_n1 = H_d;
        M_n1 = M;

        return M;
    }

    // Expose for testing
    double getAnhystereticMagnetization(double H) {
        return langevin(H + params.alpha * M_n1) * params.M_s;
    }

private:
    Parameters params;
    double sampleRate = 48000.0;
    double T = 1.0 / 48000.0;

    // State variables
    double M_n1 = 0.0;   // Previous magnetization
    double H_n1 = 0.0;   // Previous field
    double H_d_n1 = 0.0; // Previous field derivative

    // Precomputed values
    double oneOverA = 1.0 / 22000.0;
    double Q = 1.0;
    double cAlpha = 0.0;

    /**
     * Langevin function: L(x) = coth(x) - 1/x
     * This defines the anhysteretic (centerline) magnetization curve
     */
    double langevin(double x) const {
        // Handle small x to avoid division by zero
        if (std::abs(x) < 1e-4) {
            return x / 3.0;  // Taylor series approximation
        }
        return 1.0 / std::tanh(x) - 1.0 / x;
    }

    /**
     * Derivative of Langevin function
     */
    double langevinD(double x) const {
        if (std::abs(x) < 1e-4) {
            return 1.0 / 3.0;
        }
        double cothX = 1.0 / std::tanh(x);
        return 1.0 / (x * x) - cothX * cothX + 1.0;
    }

    /**
     * Newton-Raphson solver for implicit J-A equation
     * Solves for M given H and dH/dt
     */
    double solveNR8(double H, double H_d) {
        // Sign of dH/dt (determines which branch of hysteresis)
        double delta = (H_d >= 0.0) ? 1.0 : -1.0;

        // Initial guess
        double M = M_n1;

        // Newton-Raphson iterations
        for (int i = 0; i < 8; ++i) {
            // Effective field
            double H_eff = H + params.alpha * M;

            // Anhysteretic magnetization
            double M_an = params.M_s * langevin(H_eff * oneOverA);

            // Derivative of anhysteretic w.r.t. M
            double dM_an_dM = params.M_s * langevinD(H_eff * oneOverA) *
                             oneOverA * params.alpha;

            // Magnetization difference
            double M_diff = M_an - M;

            // dM/dH (the core J-A differential equation)
            double denom = 1.0 - cAlpha;
            double delta_k = delta * params.k;

            double dM_dH;
            if (std::abs(M_diff) > 1e-12 && delta * M_diff > 0) {
                dM_dH = (M_diff / (delta_k - params.alpha * M_diff) +
                        params.c * dM_an_dM) / denom;
            } else {
                dM_dH = params.c * dM_an_dM / denom;
            }

            // Function to find root of (implicit equation)
            double f = M - M_n1 - T * dM_dH * H_d;

            // Derivative of f w.r.t. M
            double f_prime = 1.0 - T * H_d * computeDfDM(H, M, delta);

            // Newton-Raphson update
            if (std::abs(f_prime) > 1e-12) {
                M = M - f / f_prime;
            }

            // Clamp to physical limits
            M = std::clamp(M, -params.M_s, params.M_s);
        }

        return M;
    }

    /**
     * Compute derivative of dM/dH with respect to M
     * Needed for Newton-Raphson Jacobian
     */
    double computeDfDM(double H, double M, double delta) {
        double H_eff = H + params.alpha * M;
        double x = H_eff * oneOverA;

        double L = langevin(x);
        double Ld = langevinD(x);

        double M_an = params.M_s * L;
        double dM_an = params.M_s * Ld * oneOverA * params.alpha;

        double M_diff = M_an - M;
        double delta_k = delta * params.k;

        double denom = delta_k - params.alpha * M_diff;

        if (std::abs(denom) < 1e-12) {
            return 0.0;
        }

        // This is an approximation - full derivative is complex
        return (dM_an - 1.0) / denom / (1.0 - cAlpha);
    }
};

} // namespace TapeHysteresis
