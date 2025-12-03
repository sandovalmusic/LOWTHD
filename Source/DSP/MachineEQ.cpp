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
    ampexHP.setHighPass(20.0, 0.7071, fs);      // 20 Hz, 12 dB/oct Butterworth
    ampexBell1.setBell(40.0, 1.58, 1.4, fs);
    ampexBell2.setBell(65.0, 1.265, -2.0, fs);
    ampexBell3.setBell(75.0, 0.8, 2.0, fs);
    ampexBell4.setBell(230.0, 0.6, -0.8, fs);
    ampexBell5.setBell(6000.0, 0.4, -0.6, fs);

    // === Studer A820 "Tracks" EQ ===
    studerHP1.setHighPass(30.0, 0.7071, fs);    // 30 Hz, 12 dB/oct
    studerHP2.setHighPass(30.0, fs);            // 30 Hz, 6 dB/oct (total 18 dB/oct)
    studerBell1.setBell(32.0, 1.5, 0.4, fs);
    studerBell2.setBell(72.0, 2.07, -2.7, fs);
    studerBell3.setBell(85.0, 1.0, 3.2, fs);
    studerBell4.setBell(180.0, 1.0, -0.8, fs);
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
