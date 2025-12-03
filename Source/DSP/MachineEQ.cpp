#include "MachineEQ.h"

namespace TapeHysteresis
{

MachineEQ::MachineEQ()
{
    updateCoefficients();
}

void MachineEQ::setSampleRate(double sampleRate)
{
    fs = sampleRate;
    updateCoefficients();
}

void MachineEQ::setMachine(Machine machine)
{
    currentMachine = machine;
}

void MachineEQ::reset()
{
    // Reset Ampex filters
    ampexHP.reset();
    ampexBell1.reset();
    ampexBell2.reset();
    ampexBell3.reset();
    ampexBell4.reset();
    ampexBell5.reset();
    ampexBell6.reset();
    ampexBell7.reset();
    ampexBell8.reset();
    ampexLP.reset();

    // Reset Studer filters
    studerHP1.reset();
    studerHP2.reset();
    studerBell1.reset();
    studerBell2.reset();
    studerBell3.reset();
    studerBell4.reset();
    studerBell5.reset();
    studerBell6.reset();
    studerBell7.reset();
    studerBell8.reset();
}

void MachineEQ::updateCoefficients()
{
    // === Ampex ATR-102 "Master" EQ ===
    // Fine-tuned to match Pro-Q4 reference:
    // Targets: 20Hz=-2.7dB, 28Hz=0dB, 40Hz=+1.15dB, 70Hz=+0.17dB, 105Hz=+0.3dB, 150Hz=0dB,
    //          350Hz=-0.5dB, 1200Hz=-0.3dB, 3kHz=-0.45dB, 10kHz=0dB, 16kHz=-0.25dB, 21.5kHz=0dB
    ampexHP.setHighPass(19.5, 0.7071, fs);      // Adjusted for -2.7dB @ 20Hz, ~0dB @ 28Hz
    ampexBell1.setBell(40.0, 1.58, 1.5, fs);    // +1.15dB @ 40Hz
    ampexBell2.setBell(65.0, 1.265, -2.3, fs);  // Dip to lower 105Hz to +0.3dB
    ampexBell3.setBell(75.0, 0.8, 2.1, fs);     // Bring up 70Hz to +0.17dB
    ampexBell4.setBell(230.0, 0.6, -0.7, fs);   // Mid dip for -0.5dB @ 350Hz
    ampexBell5.setBell(1500.0, 0.5, -0.2, fs);  // Broad mid-high dip for 1200Hz/3kHz
    ampexBell6.setBell(6000.0, 0.4, -0.25, fs); // HF dip
    ampexBell7.setBell(16000.0, 1.5, -0.15, fs);// 16kHz cut (reduced, was overcutting)
    ampexBell8.setBell(30000.0, 0.6, 2.2, fs);  // HF lift for 21.5kHz=0dB
    ampexLP.setLowPass(30000.0, fs);            // LP at 30kHz

    // === Studer A820 "Tracks" EQ ===
    // 18dB/oct Butterworth HP = 2nd order (Q=1.0) + 1st order cascaded
    // For 3rd order Butterworth, biquad Q = 1/(2*cos(60°)) = 1.0
    studerHP1.setHighPass(27.0, 1.0, fs);       // Lowered to 27Hz for -2dB @ 30Hz target
    studerHP2.setHighPass(27.0, fs);            // 6 dB/oct (total 18 dB/oct)
    // Head bumps at 49.5Hz and 110Hz per Pro-Q4 reference
    // Targets: 30Hz=-2dB, 38Hz=0dB, 49.5Hz=+0.55dB, 69.5Hz=+0.1dB, 110Hz=+1.2dB, 260Hz=+0.05dB
    studerBell1.setBell(49.5, 1.5, 0.7, fs);    // First head bump (reduced)
    studerBell2.setBell(72.0, 2.07, -1.1, fs);  // Dip between bumps (reduced for 69.5Hz)
    studerBell3.setBell(110.0, 1.0, 1.8, fs);   // Second head bump (slightly increased)
    studerBell4.setBell(180.0, 1.0, -0.7, fs);
    studerBell5.setBell(600.0, 0.8, 0.2, fs);
    studerBell6.setBell(2000.0, 1.0, 0.1, fs);
    studerBell7.setBell(5000.0, 1.0, 0.1, fs);
    studerBell8.setBell(10000.0, 1.0, -0.1, fs);
}

double MachineEQ::processSample(double input)
{
    double x = input;

    if (currentMachine == Machine::Ampex)
    {
        x = ampexHP.process(x);
        x = ampexBell1.process(x);
        x = ampexBell2.process(x);
        x = ampexBell3.process(x);
        x = ampexBell4.process(x);
        x = ampexBell5.process(x);
        x = ampexBell6.process(x);
        x = ampexBell7.process(x);
        x = ampexBell8.process(x);
        x = ampexLP.process(x);
    }
    else
    {
        x = studerHP1.process(x);
        x = studerHP2.process(x);
        x = studerBell1.process(x);
        x = studerBell2.process(x);
        x = studerBell3.process(x);
        x = studerBell4.process(x);
        x = studerBell5.process(x);
        x = studerBell6.process(x);
        x = studerBell7.process(x);
        x = studerBell8.process(x);
    }

    return x;
}

} // namespace TapeHysteresis
