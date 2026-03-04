// <Filename>: <motor_controller.cpp>
// <Authors>:  <DANIEL DESAI, NOAH VAWTER>
// <Updated>:  <2026-03-04>
// <Version>:  <0.0.2>
//
// Truth Table:
//   Motor 0 Forward  -> Relay 1
//   Motor 0 Reverse  -> Relays 1, 2, 3
//   Motor 1 Forward  -> Relay 4
//   Motor 1 Reverse  -> Relays 4, 5, 6
//   Motor 2 Forward  -> Relay 7
//   Motor 2 Reverse  -> Relays 7, 8, 9
//   Motor 3 Forward  -> Relay 10
//   Motor 3 Reverse  -> Relays 10, 11, 12
//
// Relay roles per motor N (board relay numbers, 1-indexed to match truth table):
//   getMotorBase(N) = (N*3)+1  <- enable / forward relay
//   base+1                     <- reverse relay A
//   base+2                     <- reverse relay B
//
// Motor sequencing:
//   Forward:  1. set direction (base+1 OFF, base+2 OFF)
//             2. enable relay ON (base)
//             3. power supply ON (caller)
//   Reverse:  1. set direction (base+1 ON, base+2 ON)
//             2. enable relay ON (base)
//             3. power supply ON (caller)
//   Stop:     1. power supply OFF (caller)
//             2. enable relay OFF (base)
//             3. direction relays unchanged

#include "motor_controller.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <bitset>
#include <array>

// ========================================
// NumatoRelayController Implementation
// ========================================

NumatoRelayController::NumatoRelayController(const std::string& port, bool test)
    : portName(port), hComm(INVALID_HANDLE_VALUE), testMode(test) {
    for (auto i = 0; i < 16; i++) relayStates[i] = false;
}

NumatoRelayController::~NumatoRelayController() {
    closePort();
}

auto NumatoRelayController::isTestMode() const -> bool {
    return testMode;
}

auto NumatoRelayController::openPort() -> bool {
    if (testMode) {
        std::cout << "\n*** TEST MODE - No hardware connection required ***" << std::endl;
        std::cout << "All commands will be simulated\n" << std::endl;
        return true;
    }

    auto cleanPort = std::string{};
    for (auto c : portName) {
        if (isalnum(c)) cleanPort += toupper(c);
    }
    if (!cleanPort.empty() && isdigit(cleanPort[0]))
        cleanPort = "COM" + cleanPort;

    auto formattedPort = "\\\\.\\" + cleanPort;
    auto widePort = std::wstring(formattedPort.begin(), formattedPort.end());

    hComm = CreateFileW(widePort.c_str(), GENERIC_READ | GENERIC_WRITE,
                        0, NULL, OPEN_EXISTING, 0, NULL);

    if (hComm == INVALID_HANDLE_VALUE) {
        auto error = GetLastError();
        std::cerr << "Error opening " << cleanPort << ". Error code: " << error << std::endl;
        switch (error) {
            case ERROR_FILE_NOT_FOUND: std::cerr << "Port does not exist." << std::endl; break;
            case ERROR_ACCESS_DENIED:  std::cerr << "Access denied - port may be in use." << std::endl; break;
        }
        return false;
    }

    auto dcbSerialParams = DCB{0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hComm, &dcbSerialParams)) {
        std::cerr << "Error getting COM port state" << std::endl;
        closePort(); return false;
    }

    dcbSerialParams.BaudRate    = CBR_19200;
    dcbSerialParams.ByteSize    = 8;
    dcbSerialParams.StopBits    = ONESTOPBIT;
    dcbSerialParams.Parity      = NOPARITY;
    dcbSerialParams.fDtrControl = DTR_CONTROL_ENABLE;
    dcbSerialParams.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(hComm, &dcbSerialParams)) {
        std::cerr << "Error setting COM port state" << std::endl;
        closePort(); return false;
    }

    auto timeouts = COMMTIMEOUTS{0};
    timeouts.ReadIntervalTimeout        = 100;
    timeouts.ReadTotalTimeoutConstant   = 500;
    timeouts.WriteTotalTimeoutConstant  = 500;
    if (!SetCommTimeouts(hComm, &timeouts)) {
        std::cerr << "Error setting timeouts" << std::endl;
        closePort(); return false;
    }

    Sleep(100);  // give board time to recognise DTR
    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
    std::cout << "Successfully opened " << cleanPort << " at 19200 baud" << std::endl;

    if (!sendCommand("ver", true))
        std::cerr << "Warning: Could not verify connection to Numato relay module" << std::endl;

    return true;
}

void NumatoRelayController::closePort() {
    if (hComm != INVALID_HANDLE_VALUE) {
        CloseHandle(hComm);
        hComm = INVALID_HANDLE_VALUE;
    }
}

auto NumatoRelayController::sendCommand(const std::string& cmd, bool readResponse, bool silent) -> bool {
    if (testMode) {
        if (!silent) std::cout << "[TEST] TX: " << cmd << std::endl;
        return true;
    }

    if (hComm == INVALID_HANDLE_VALUE) {
        std::cerr << "Port not open!" << std::endl;
        return false;
    }

    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
    auto fullCmd = cmd + "\r";
    if (!silent) std::cout << "TX: " << cmd << std::endl;

    auto bytesWritten = DWORD{};
    if (!WriteFile(hComm, fullCmd.c_str(), fullCmd.length(), &bytesWritten, NULL)) {
        std::cerr << "Error writing to COM port" << std::endl;
        return false;
    }

    FlushFileBuffers(hComm);
    Sleep(50);

    if (readResponse) {
        auto response = std::array<char, 256>{};
        auto bytesRead = DWORD{};
        if (ReadFile(hComm, response.data(), response.size() - 1, &bytesRead, NULL) && bytesRead > 0) {
            response[bytesRead] = '\0';
            if (!silent) std::cout << "RX: " << response.data();
        }
    }
    return true;
}

auto NumatoRelayController::relayOn(int relayNum, bool silent) -> bool {
    if (relayNum < 0 || relayNum > 15) {
        std::cerr << "Invalid relay number. Must be 0-15" << std::endl;
        return false;
    }
    if (!silent)
        std::cout << (testMode ? "[TEST] " : "") << "Turning ON relay " << relayNum << std::endl;

    auto ss = std::ostringstream{};
    ss << "relay on " << std::setw(2) << std::setfill('0') << relayNum;
    auto success = sendCommand(ss.str(), false, silent);
    if (success) relayStates[relayNum] = true;
    return success;
}

auto NumatoRelayController::relayOff(int relayNum, bool silent) -> bool {
    if (relayNum < 0 || relayNum > 15) {
        std::cerr << "Invalid relay number. Must be 0-15" << std::endl;
        return false;
    }
    if (!silent)
        std::cout << (testMode ? "[TEST] " : "") << "Turning OFF relay " << relayNum << std::endl;

    auto ss = std::ostringstream{};
    ss << "relay off " << std::setw(2) << std::setfill('0') << relayNum;
    auto success = sendCommand(ss.str(), false, silent);
    if (success) relayStates[relayNum] = false;
    return success;
}

auto NumatoRelayController::allOff() -> bool {
    std::cout << (testMode ? "[TEST] " : "") << "Turning OFF all relays (0-15)" << std::endl;
    auto success = sendCommand("relay off all");
    if (success) {
        for (auto i = 0; i < 16; i++) relayStates[i] = false;
    }
    return success;
}

void NumatoRelayController::showStatus() {
    std::cout << "\n--- Relay Status ";
    if (testMode) std::cout << "(TEST MODE) ";
    std::cout << "---" << std::endl;
    for (auto i = 0; i < 16; i++) {
        std::cout << "Relay " << std::setw(2) << i << ": "
                  << (relayStates[i] ? "ON " : "OFF") << "  ";
        if ((i + 1) % 4 == 0) std::cout << std::endl;
    }
    std::cout << std::endl;
}

auto NumatoRelayController::getVersion() -> bool {
    if (testMode) {
        std::cout << "[TEST] Device version: Simulated Numato 16-Channel v1.0" << std::endl;
        return true;
    }
    std::cout << "Getting device version..." << std::endl;
    return sendCommand("ver", true);
}

// ========================================
// MotorController Implementation
// ========================================

MotorController::MotorController(NumatoRelayController& r) : relay(r) {
    for (auto i = 0; i < 4; i++) motorStates[i] = MOTOR_STOPPED;
}

// Returns the board relay number for the base (enable) relay of the given motor.
// Matches the truth table exactly (1-indexed):
//   Motor 0 -> relay 1, Motor 1 -> relay 4, Motor 2 -> relay 7, Motor 3 -> relay 10
auto MotorController::getMotorBase(int motorNum) -> int {
    return (motorNum * 3) + 1;
}

auto MotorController::motorStop(int motorNum) -> bool {
    if (motorNum < 0 || motorNum > 3) {
        std::cerr << "Invalid motor number. Must be 0-3" << std::endl;
        return false;
    }

    auto base = getMotorBase(motorNum);

    std::cout << "\n--- STOPPING Motor " << motorNum << " ---" << std::endl;

    // Step 1: Power supply disabled by caller before motorStop() is called.
    // Step 2: Disable enable relay only — direction relays are left as-is.
    Sleep(100);   // ensure power supply has settled off before opening enable relay
    relay.relayOff(base, false);

    motorStates[motorNum] = MOTOR_STOPPED;

    std::cout << "Motor " << motorNum << " stopped" << std::endl;
    std::cout << "  Enable relay R" << base << " = OFF (direction relays unchanged)" << std::endl;
    return true;
}

auto MotorController::motorForward(int motorNum, bool checkSafety) -> bool {
    if (motorNum < 0 || motorNum > 3) {
        std::cerr << "Invalid motor number. Must be 0-3" << std::endl;
        return false;
    }

    auto base = getMotorBase(motorNum);

    std::cout << "\n--- FORWARD: Motor " << motorNum << " ---" << std::endl;

    if (checkSafety && motorStates[motorNum] == MOTOR_REVERSE) {
        std::cout << "Safety: Stopping motor before changing direction..." << std::endl;
        motorStop(motorNum);
        Sleep(100);
    }

    // Sequence: 1. set direction  2. enable relay ON  3. power supply ON (caller)
    relay.relayOff(base + 1, false);
    relay.relayOff(base + 2, false);
    Sleep(100);   // wait for direction relays to fully open
    relay.relayOn(base, false);

    motorStates[motorNum] = MOTOR_FORWARD;

    std::cout << "Motor " << motorNum << " FORWARD" << std::endl;
    std::cout << "  Direction: R" << (base+1) << ", R" << (base+2) << " = OFF" << std::endl;
    std::cout << "  Enable:    R" << base << " = ON" << std::endl;
    return true;
}

auto MotorController::motorReverse(int motorNum, bool checkSafety) -> bool {
    if (motorNum < 0 || motorNum > 3) {
        std::cerr << "Invalid motor number. Must be 0-3" << std::endl;
        return false;
    }

    auto base = getMotorBase(motorNum);

    std::cout << "\n--- REVERSE: Motor " << motorNum << " ---" << std::endl;

    if (checkSafety && motorStates[motorNum] == MOTOR_FORWARD) {
        std::cout << "Safety: Stopping motor before changing direction..." << std::endl;
        motorStop(motorNum);
        Sleep(100);
    }

    // Sequence: 1. set direction  2. enable relay ON  3. power supply ON (caller)
    relay.relayOn(base + 1, false);
    relay.relayOn(base + 2, false);
    Sleep(100);   // wait for direction relays to fully close
    relay.relayOn(base, false);

    motorStates[motorNum] = MOTOR_REVERSE;

    std::cout << "Motor " << motorNum << " REVERSE" << std::endl;
    std::cout << "  Direction: R" << (base+1) << ", R" << (base+2) << " = ON" << std::endl;
    std::cout << "  Enable:    R" << base << " = ON" << std::endl;
    return true;
}

auto MotorController::motorBrake(int motorNum) -> bool {
    if (motorNum < 0 || motorNum > 3) {
        std::cerr << "Invalid motor number. Must be 0-3" << std::endl;
        return false;
    }
    std::cout << "\n--- BRAKING Motor " << motorNum
              << " (stopping - no brake state in truth table) ---" << std::endl;
    return motorStop(motorNum);
}

void MotorController::stopAll() {
    std::cout << "\n--- STOP All Motors ---" << std::endl;
    for (auto i = 0; i < 4; i++) motorStop(i);
    std::cout << "All motors stopped!" << std::endl;
}

void MotorController::showMotorStatus() {
    std::cout << "\n--- Motor Status ---" << std::endl;
    for (auto i = 0; i < 4; i++) {
        auto base = getMotorBase(i);
        std::cout << "Motor " << i << ": ";
        switch (motorStates[i]) {
            case MOTOR_STOPPED: std::cout << "STOPPED"; break;
            case MOTOR_FORWARD: std::cout << "FORWARD"; break;
            case MOTOR_REVERSE: std::cout << "REVERSE"; break;
        }
        std::cout << "  |  Relays: " << base << ", " << (base+1) << ", " << (base+2) << std::endl;
    }
    std::cout << std::endl;
}

void MotorController::explainWiring() {
    std::cout << "\n--- TRUTH TABLE RELAY CONFIGURATION ---" << std::endl;
    std::cout << "\nEach motor uses 3 relays.\n" << std::endl;
    std::cout << std::left
              << std::setw(8)  << "Motor"
              << std::setw(12) << "Direction"
              << "Relays ON"
              << std::endl;
    std::cout << std::string(40, '-') << std::endl;

    const auto dirs = std::array<const char*, 2>{ "Forward", "Reverse" };
    for (auto m = 0; m < 4; m++) {
        auto base = getMotorBase(m);
        for (auto d = 0; d < 2; d++) {
            std::cout << std::setw(8) << m << std::setw(12) << dirs[d];
            if (d == 0) {
                std::cout << base << std::endl;
            } else {
                std::cout << base << ", " << (base+1) << ", " << (base+2) << std::endl;
            }
        }
        if (m < 3) std::cout << std::endl;
    }

    std::cout << "\nRelay roles per motor N:" << std::endl;
    std::cout << "  (N*3)+1 = Enable / Forward relay" << std::endl;
    std::cout << "  (N*3)+2 = Reverse relay A" << std::endl;
    std::cout << "  (N*3)+3 = Reverse relay B\n" << std::endl;

    std::cout << "Operation summary:" << std::endl;
    std::cout << "  Forward  ->  Direction OFF, Enable ON, Power ON" << std::endl;
    std::cout << "  Reverse  ->  Direction ON,  Enable ON, Power ON" << std::endl;
    std::cout << "  Stop     ->  Power OFF, Enable OFF, Direction unchanged\n" << std::endl;
}

// ========================================
// Interactive Motor Controller Function
// ========================================

static void printMenu() {
    std::cout << "\n--- Motor Control Menu ---" << std::endl;
    std::cout << "\nMotor Commands:" << std::endl;
    std::cout << "  up <N>      - Move motor N forward" << std::endl;
    std::cout << "  down <N>    - Move motor N in reverse" << std::endl;
    std::cout << "  stop <N>    - Stop motor N" << std::endl;
    std::cout << "  stopall     - Stop all motors" << std::endl;
    std::cout << "\nStatus Commands:" << std::endl;
    std::cout << "  motors      - Show motor status" << std::endl;
    std::cout << "  relays      - Show relay status" << std::endl;
    std::cout << "  status      - Show both motors and relays" << std::endl;
    std::cout << "\nOther Commands:" << std::endl;
    std::cout << "  wiring      - Show truth table wiring reference" << std::endl;
    std::cout << "  help        - Show this menu" << std::endl;
    std::cout << "  exit        - Exit program" << std::endl;
    std::cout << std::endl;
}

void runMotorController(bool testMode, const std::string& comPort) {
    std::cout << "\n--- Motor Controller ---" << std::endl;

    auto port = testMode ? std::string{"TEST"} : comPort;
    std::cout << "Mode: " << (testMode ? "TEST" : "OPERATING on " + port) << std::endl;

    auto relay = NumatoRelayController(port, testMode);
    if (!relay.openPort()) {
        std::cerr << "\nFailed to open controller!" << std::endl;
        return;
    }

    auto motors = MotorController(relay);

    std::cout << "\n--- Motor / Relay Assignments (Truth Table) ---" << std::endl;
    std::cout << "Motor 0: Relays 1-3   |  Motor 1: Relays 4-6" << std::endl;
    std::cout << "Motor 2: Relays 7-9   |  Motor 3: Relays 10-12" << std::endl;

    printMenu();

    auto input = std::string{};
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (input.empty()) continue;

        auto lower = input;
        for (auto& c : lower) c = tolower(c);

        auto sp   = lower.find(' ');
        auto cmd  = (sp != std::string::npos) ? lower.substr(0, sp) : lower;
        auto args = (sp != std::string::npos) ? lower.substr(sp + 1) : "";

        if (cmd == "exit" || cmd == "quit" || cmd == "q") {
            std::cout << "\nStopping all motors and exiting..." << std::endl;
            motors.stopAll();
            return;
        }
        else if (cmd == "help" || cmd == "h" || cmd == "?") { printMenu(); }
        else if (cmd == "up"   || cmd == "u") {
            if (!args.empty()) {
                auto n = atoi(args.c_str());
                if (n >= 0 && n <= 3) { motors.motorForward(n); std::cout << "\n[ACTIVE] Motor " << n << " FORWARD" << std::endl; }
                else std::cout << "Invalid motor number. Use 0-3" << std::endl;
            } else std::cout << "Usage: up <motor number 0-3>" << std::endl;
        }
        else if (cmd == "down" || cmd == "d") {
            if (!args.empty()) {
                auto n = atoi(args.c_str());
                if (n >= 0 && n <= 3) { motors.motorReverse(n); std::cout << "\n[ACTIVE] Motor " << n << " REVERSE" << std::endl; }
                else std::cout << "Invalid motor number. Use 0-3" << std::endl;
            } else std::cout << "Usage: down <motor number 0-3>" << std::endl;
        }
        else if (cmd == "stop" || cmd == "s") {
            if (!args.empty()) {
                auto n = atoi(args.c_str());
                if (n >= 0 && n <= 3) { motors.motorStop(n); std::cout << "[STOPPED] Motor " << n << std::endl; }
                else std::cout << "Invalid motor number. Use 0-3" << std::endl;
            } else std::cout << "Usage: stop <motor number 0-3>" << std::endl;
        }
        else if (cmd == "stopall" || cmd == "emergency" || cmd == "estop") {
            motors.stopAll();
            std::cout << "\n[ALL STOPPED]" << std::endl;
        }
        else if (cmd == "motors" || cmd == "m")           { motors.showMotorStatus(); }
        else if (cmd == "relays" || cmd == "r")           { relay.showStatus(); }
        else if (cmd == "status" || cmd == "st")          { motors.showMotorStatus(); relay.showStatus(); }
        else if (cmd == "wiring" || cmd == "wire" || cmd == "w") { motors.explainWiring(); }
        else { std::cout << "Unknown command. Type 'help' for available commands." << std::endl; }
    }
}

// ========================================
// Automated Motor Control Function
// ========================================

void runMotorController(const std::bitset<4>& pattern, bool isReverse, bool testMode, const std::string& comPort, int timeout, int delay) {
    std::cout << "\n--- Automated Motor Controller ---" << std::endl;
    std::cout << "--- Pattern: "   << pattern << " ---" << std::endl;
    std::cout << "--- Direction: " << (isReverse ? "REVERSE" : "FORWARD") << " ---" << std::endl;
    if (timeout > 0)
        std::cout << "--- Runtime per motor: " << timeout << " seconds ---" << std::endl;
    else
        std::cout << "--- No timeout (manual stop required) ---" << std::endl;
    if (delay > 0)
        std::cout << "--- Delay between motors: " << delay << " ms ---\n" << std::endl;
    else
        std::cout << "--- No delay between motors ---\n" << std::endl;

    auto port = testMode ? std::string{"TEST"} : comPort;
    std::cout << "Mode: " << (testMode ? "TEST" : "OPERATING on " + port) << std::endl;

    auto relay = NumatoRelayController(port, testMode);
    if (!relay.openPort()) {
        std::cerr << "\nFailed to open controller!" << std::endl;
        return;
    }

    auto motors = MotorController(relay);

    // Bit mapping: bit 3 = Motor 0, bit 2 = Motor 1, bit 1 = Motor 2, bit 0 = Motor 3
    std::cout << "\n--- Motor Control Pattern ---" << std::endl;
    for (auto m = 0; m < 4; m++) {
        auto bit   = 3 - m;
        auto base  = motors.getMotorBase(m);
        std::cout << "Motor " << m << " (Relays " << base << "-" << (base+2) << "): ";
        std::cout << (pattern[bit] ? (isReverse ? "REVERSE" : "FORWARD") : "STOPPED") << std::endl;
    }
    std::cout << std::endl;

    auto runMotor = [&](int motorNum, bool applyDelay) {
        auto bit = 3 - motorNum;
        if (pattern[bit]) {
            std::cout << "\n=== Starting Motor " << motorNum << " ===" << std::endl;
            if (isReverse) motors.motorReverse(motorNum);
            else           motors.motorForward(motorNum);

            if (timeout > 0) {
                std::cout << "Motor " << motorNum << " will run for " << timeout << " seconds..." << std::endl;
                for (auto i = timeout; i > 0; i--) {
                    std::cout << "\rMotor " << motorNum << " time remaining: " << i << "s  " << std::flush;
                    Sleep(1000);
                }
                std::cout << "\n>>> Motor " << motorNum << " timeout reached <<<" << std::endl;
                motors.motorStop(motorNum);
            }
        } else {
            std::cout << "\n=== Motor " << motorNum << " SKIPPED (stopped) ===" << std::endl;
        }

        if (applyDelay && delay > 0) {
            std::cout << "Waiting " << delay << " ms before next motor..." << std::endl;
            Sleep(delay);
        }
    };

    std::cout << "Applying motor pattern..." << std::endl;
    runMotor(0, true);
    runMotor(1, true);
    runMotor(2, true);
    runMotor(3, false);

    if (timeout > 0) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "All motors completed run cycles" << std::endl;
        std::cout << "========================================\n" << std::endl;
    } else {
        std::cout << "\nPress Enter to stop all motors and exit...";
        std::cin.get();
    }

    std::cout << "\nStopping all motors..." << std::endl;
    motors.stopAll();
    std::cout << "\nProgram complete." << std::endl;
}