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
    ampexBell9.reset();
    ampexBell10.reset();
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
    // Targets from Jack Endino and EMC Published Specs:
    // 20Hz=-2.7dB, 28Hz=0dB, 40Hz=+1.15dB, 70Hz=+0.17dB, 105Hz=+0.3dB, 150Hz=0dB,
    // 300Hz=-0.5dB, 1kHz=0dB, 3kHz=-0.45dB, 5kHz=0dB, 10kHz=0dB, 16kHz=-0.25dB
    ampexHP.setHighPass(20.8, 0.7071, fs);      // HP for -2.7dB @ 20Hz
    ampexBell1.setBell(28.0, 2.5, 0.4, fs);     // 28Hz lift
    ampexBell2.setBell(40.0, 1.8, 0.95, fs);    // +1.15dB @ 40Hz
    ampexBell3.setBell(70.0, 2.0, -0.3, fs);    // Cut for +0.17dB @ 70Hz
    ampexBell4.setBell(105.0, 2.0, 0.1, fs);    // +0.3dB @ 105Hz
    ampexBell5.setBell(150.0, 2.0, -0.2, fs);   // Cut for 0dB @ 150Hz
    ampexBell6.setBell(300.0, 0.7, -0.8, fs);   // -0.5dB @ 300Hz (wider Q, more gain)
    ampexBell7.setBell(1200.0, 1.5, -0.25, fs); // -0.3dB @ 1200Hz
    ampexBell8.setBell(3000.0, 1.0, -0.7, fs);  // -0.45dB @ 3kHz (wider Q, more gain)
    ampexBell9.setBell(7000.0, 1.0, -0.3, fs);  // Cut 5-10kHz excess
    ampexBell10.setBell(16000.0, 1.5, -0.4, fs);// -0.25dB @ 16kHz
    ampexLP.setLowPass(40000.0, fs);            // LP at 40kHz

    // === Studer A820 "Tracks" EQ ===
    // Targets from Jack Endino and EMC Published Specs:
    // 20Hz=-5dB, 28Hz=-2.5dB, 40Hz=0dB, 50Hz=+0.55dB, 70Hz=+0.1dB, 110Hz=+1.2dB
    // 18dB/oct HP tuned to hit both 20Hz and 28Hz targets
    studerHP1.setHighPass(22.0, 1.0, fs);       // 2nd order @ 22Hz
    studerHP2.setHighPass(22.0, fs);            // 1st order @ 22Hz (total 18 dB/oct)
    // Shape the rolloff and head bumps
    studerBell1.setBell(28.0, 1.0, -2.0, fs);   // Cut at 28Hz for -2.5dB target
    studerBell2.setBell(40.0, 2.0, 0.9, fs);    // Lift at 40Hz to counter HP rolloff
    studerBell3.setBell(50.0, 1.5, 0.6, fs);    // First head bump at 50Hz (+0.55dB target)
    studerBell4.setBell(70.0, 2.5, -0.6, fs);   // Dip at 70Hz
    studerBell5.setBell(110.0, 1.0, 1.5, fs);   // Second head bump (+1.2dB target)
    studerBell6.setBell(160.0, 1.5, -0.5, fs);  // Post-bump dip (moved lower)
    studerBell7.setBell(2000.0, 1.5, 0.05, fs); // Subtle 2kHz boost
    studerBell8.setBell(10000.0, 2.0, -0.1, fs);// Slight cut at 10kHz
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
        x = ampexBell9.process(x);
        x = ampexBell10.process(x);
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
