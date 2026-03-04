// <Filename>: <motor_controller.h>
// <Authors>:  <DANIEL DESAI, NOAH VAWTER>
// <Updated>:  <2026-03-04>
// <Version>:  <0.0.2>

#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <windows.h>
#include <string>
#include <bitset>

class NumatoRelayController {
private:
    HANDLE hComm;
    std::string portName;
    bool relayStates[16];
    bool testMode;

public:
    NumatoRelayController(const std::string& port, bool test = false);
    ~NumatoRelayController();

    auto isTestMode() const -> bool;
    auto openPort()         -> bool;
    void closePort();
    auto sendCommand(const std::string& cmd, bool readResponse = false, bool silent = false) -> bool;
    auto relayOn(int relayNum, bool silent = false)  -> bool;
    auto relayOff(int relayNum, bool silent = false) -> bool;
    auto allOff()      -> bool;
    auto getVersion()  -> bool;
    void showStatus();
};

class MotorController {
private:
    NumatoRelayController& relay;

    enum MotorState {
        MOTOR_STOPPED,
        MOTOR_FORWARD,
        MOTOR_REVERSE
    };

    MotorState motorStates[4];

public:
    auto getMotorBase(int motorNum) -> int;   // public: used in runMotorController(bitset...)

    MotorController(NumatoRelayController& r);

    auto motorStop(int motorNum)                             -> bool;
    auto motorForward(int motorNum, bool checkSafety = true) -> bool;
    auto motorReverse(int motorNum, bool checkSafety = true) -> bool;
    auto motorBrake(int motorNum)                            -> bool;
    void stopAll();
    void showMotorStatus();
    void explainWiring();
};

// Main interactive function
// testMode: true = simulated (default), false = hardware
// comPort:  COM port to use in hardware mode (default "COM6")
void runMotorController(bool testMode = true, const std::string& comPort = "COM6");

// Automated motor control function using std::bitset
// pattern:  4-bit bitset — bit 3 = Motor 0, bit 2 = Motor 1, bit 1 = Motor 2, bit 0 = Motor 3
//           1 = motor runs, 0 = motor stopped
// isReverse: true = reverse, false = forward (default)
// testMode:  true = simulated (default), false = hardware
// comPort:   COM port to use in hardware mode (default "COM6")
// timeout:   seconds each motor runs sequentially (0 = manual stop, default)
// delay:     milliseconds between motor activations (default = 0)
void runMotorController(const std::bitset<4>& pattern, bool isReverse = false, bool testMode = true, const std::string& comPort = "COM6", int timeout = 0, int delay = 0);

#endif // MOTOR_CONTROLLER_H