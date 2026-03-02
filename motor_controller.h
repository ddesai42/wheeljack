// motor_controller.h - Header file with class declarations and std::bitset

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
    
    bool isTestMode() const;
    bool openPort();
    void closePort();
    bool sendCommand(const std::string& cmd, bool readResponse = false, bool silent = false);
    bool relayOn(int relayNum, bool silent = false);
    bool relayOff(int relayNum, bool silent = false);
    bool allOff();
    void showStatus();
    bool getVersion();
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
    int getMotorBase(int motorNum);

public:
    MotorController(NumatoRelayController& r);
    
    bool motorStop(int motorNum);
    bool motorForward(int motorNum, bool checkSafety = true);
    bool motorReverse(int motorNum, bool checkSafety = true);
    bool motorBrake(int motorNum);
    void stopAll();
    void showMotorStatus();
    void explainWiring();
};

// Main interactive function - call this from your program
// testMode: true = test mode (default), false = operating mode
void runMotorController(bool testMode = true);

// Automated motor control function using std::bitset
// pattern: 4-bit bitset representing motor states (e.g., std::bitset<4>("1010"))
//   Bit 3 (leftmost) = motor 0, Bit 2 = motor 1, Bit 1 = motor 2, Bit 0 (rightmost) = motor 3
//   1 = motor runs, 0 = motor stopped
//   Example: std::bitset<4>("1010") = motor 0 runs, motor 1 stopped, motor 2 runs, motor 3 stopped
//   Example: std::bitset<4>("1111") = all motors run
//   Example: std::bitset<4>(0b1010) = binary literal notation
// isReverse: true = all motors run in reverse, false = all motors run forward (default)
// testMode: true = test mode (default), false = operating mode
// timeout: number of seconds EACH motor runs (0 = manual stop, default = 0)
//   Note: Each motor runs sequentially for 'timeout' seconds, then stops before the next starts
// delay: delay in milliseconds between each motor activation (default = 0)
void runMotorController(const std::bitset<4>& pattern, bool isReverse = false, bool testMode = true, int timeout = 0, int delay = 0);

#endif // MOTOR_CONTROLLER_H