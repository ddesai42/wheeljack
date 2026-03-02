// motor_controller.cpp - Implementation file with std::bitset pattern

#include "motor_controller.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <bitset>

// ========================================
// NumatoRelayController Implementation
// ========================================

NumatoRelayController::NumatoRelayController(const std::string& port, bool test) 
    : portName(port), hComm(INVALID_HANDLE_VALUE), testMode(test) {
    for (int i = 0; i < 16; i++) {
        relayStates[i] = false;
    }
}

NumatoRelayController::~NumatoRelayController() {
    closePort();
}

bool NumatoRelayController::isTestMode() const {
    return testMode;
}

bool NumatoRelayController::openPort() {
    if (testMode) {
        std::cout << "\n*** TEST MODE - No hardware connection required ***" << std::endl;
        std::cout << "All commands will be simulated\n" << std::endl;
        return true;
    }
    
    std::string cleanPort;
    for (char c : portName) {
        if (isalnum(c)) {
            cleanPort += toupper(c);
        }
    }
    
    if (cleanPort.length() > 0 && isdigit(cleanPort[0])) {
        cleanPort = "COM" + cleanPort;
    }
    
    std::string formattedPort = "\\\\.\\" + cleanPort;
    std::wstring widePort(formattedPort.begin(), formattedPort.end());
    
    hComm = CreateFileW(
        widePort.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hComm == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        std::cerr << "Error opening " << cleanPort << ". Error code: " << error << std::endl;
        
        switch(error) {
            case ERROR_FILE_NOT_FOUND:
                std::cerr << "Port does not exist." << std::endl;
                break;
            case ERROR_ACCESS_DENIED:
                std::cerr << "Access denied - port may be in use." << std::endl;
                break;
        }
        return false;
    }

    DCB dcbSerialParams = {0};
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hComm, &dcbSerialParams)) {
        std::cerr << "Error getting COM port state" << std::endl;
        closePort();
        return false;
    }

    dcbSerialParams.BaudRate = CBR_19200;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hComm, &dcbSerialParams)) {
        std::cerr << "Error setting COM port state" << std::endl;
        closePort();
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 100;
    timeouts.ReadTotalTimeoutConstant = 500;
    timeouts.WriteTotalTimeoutConstant = 500;

    if (!SetCommTimeouts(hComm, &timeouts)) {
        std::cerr << "Error setting timeouts" << std::endl;
        closePort();
        return false;
    }

    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);
    
    std::cout << "Successfully opened " << cleanPort << " at 19200 baud" << std::endl;
    
    if (!sendCommand("ver", true)) {
        std::cerr << "Warning: Could not verify connection to Numato relay module" << std::endl;
    }
    
    return true;
}

void NumatoRelayController::closePort() {
    if (hComm != INVALID_HANDLE_VALUE) {
        CloseHandle(hComm);
        hComm = INVALID_HANDLE_VALUE;
    }
}

bool NumatoRelayController::sendCommand(const std::string& cmd, bool readResponse, bool silent) {
    if (testMode) {
        if (!silent) {
            std::cout << "[TEST] TX: " << cmd << std::endl;
        }
        return true;
    }
    
    if (hComm == INVALID_HANDLE_VALUE) {
        std::cerr << "Port not open!" << std::endl;
        return false;
    }

    PurgeComm(hComm, PURGE_RXCLEAR | PURGE_TXCLEAR);

    std::string fullCmd = cmd + "\r";
    
    if (!silent) {
        std::cout << "TX: " << cmd << std::endl;
    }

    DWORD bytesWritten;
    bool success = WriteFile(hComm, fullCmd.c_str(), fullCmd.length(), &bytesWritten, NULL);

    if (!success) {
        std::cerr << "Error writing to COM port" << std::endl;
        return false;
    }

    FlushFileBuffers(hComm);
    Sleep(50);

    if (readResponse) {
        char response[256];
        DWORD bytesRead;
        if (ReadFile(hComm, response, sizeof(response) - 1, &bytesRead, NULL) && bytesRead > 0) {
            response[bytesRead] = '\0';
            if (!silent) {
                std::cout << "RX: " << response;
            }
        }
    }

    return true;
}

bool NumatoRelayController::relayOn(int relayNum, bool silent) {
    if (relayNum < 0 || relayNum > 15) {
        std::cerr << "Invalid relay number. Must be 0-15" << std::endl;
        return false;
    }

    if (!silent) {
        if (testMode) {
            std::cout << "[TEST] Turning ON relay " << relayNum << std::endl;
        } else {
            std::cout << "Turning ON relay " << relayNum << std::endl;
        }
    }
    
    std::stringstream ss;
    ss << "relay on " << relayNum;
    
    bool success = sendCommand(ss.str(), false, silent);
    if (success) {
        relayStates[relayNum] = true;
    }
    return success;
}

bool NumatoRelayController::relayOff(int relayNum, bool silent) {
    if (relayNum < 0 || relayNum > 15) {
        std::cerr << "Invalid relay number. Must be 0-15" << std::endl;
        return false;
    }

    if (!silent) {
        if (testMode) {
            std::cout << "[TEST] Turning OFF relay " << relayNum << std::endl;
        } else {
            std::cout << "Turning OFF relay " << relayNum << std::endl;
        }
    }
    
    std::stringstream ss;
    ss << "relay off " << relayNum;
    
    bool success = sendCommand(ss.str(), false, silent);
    if (success) {
        relayStates[relayNum] = false;
    }
    return success;
}

bool NumatoRelayController::allOff() {
    if (testMode) {
        std::cout << "[TEST] Turning OFF all relays (0-15)" << std::endl;
    } else {
        std::cout << "Turning OFF all relays (0-15)" << std::endl;
    }
    bool success = sendCommand("relay off all");
    if (success) {
        for (int i = 0; i < 16; i++) {
            relayStates[i] = false;
        }
    }
    return success;
}

void NumatoRelayController::showStatus() {
    std::cout << "\n--- Relay Status ";
    if (testMode) {
        std::cout << "(TEST MODE) ";
    }
    std::cout << "---" << std::endl;
    for (int i = 0; i < 16; i++) {
        std::cout << "Relay " << std::setw(2) << i << ": " 
                  << (relayStates[i] ? "ON " : "OFF") << "  ";
        if ((i + 1) % 4 == 0) std::cout << std::endl;
    }
    std::cout << std::endl;
}

bool NumatoRelayController::getVersion() {
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
    for (int i = 0; i < 4; i++) {
        motorStates[i] = MOTOR_STOPPED;
    }
}

int MotorController::getMotorBase(int motorNum) {
    return motorNum * 4;
}

bool MotorController::motorStop(int motorNum) {
    if (motorNum < 0 || motorNum > 3) {
        std::cerr << "Invalid motor number. Must be 0-3" << std::endl;
        return false;
    }
    
    int base = getMotorBase(motorNum);
    
    std::cout << "\n--- STOPPING Motor " << motorNum << " ---" << std::endl;
    
    relay.relayOff(base + 0, true);
    relay.relayOff(base + 1, true);
    relay.relayOff(base + 2, true);
    relay.relayOff(base + 3, true);
    
    motorStates[motorNum] = MOTOR_STOPPED;
    
    std::cout << "Motor " << motorNum << " stopped (all relays OFF)" << std::endl;
    std::cout << "  Relays " << base << ", " << (base+1) << ", " << (base+2) << ", " << (base+3) << " = OFF" << std::endl;
    return true;
}

bool MotorController::motorForward(int motorNum, bool checkSafety) {
    if (motorNum < 0 || motorNum > 3) {
        std::cerr << "Invalid motor number. Must be 0-3" << std::endl;
        return false;
    }
    
    int base = getMotorBase(motorNum);
    
    std::cout << "\n--- FORWARD: Motor " << motorNum << " ---" << std::endl;
    
    if (checkSafety && motorStates[motorNum] == MOTOR_REVERSE) {
        std::cout << "Safety: Stopping motor before reversing direction..." << std::endl;
        motorStop(motorNum);
        Sleep(100);
    }
    
    relay.relayOff(base + 1, true);
    relay.relayOff(base + 2, true);
    Sleep(100);
    
    std::cout << "Activating relays " << (base+0) << " and " << (base+3) << std::endl;
    relay.relayOn(base + 0, true);
    relay.relayOn(base + 3, true);
    
    motorStates[motorNum] = MOTOR_FORWARD;
    
    std::cout << "Motor " << motorNum << " FORWARD" << std::endl;
    std::cout << "  Active relays: R" << (base+0) << "=ON, R" << (base+3) << "=ON" << std::endl;
    std::cout << "  Current path: V+ -> R" << (base+0) << " -> Motor_A -> Motor_B -> R" << (base+3) << " -> GND" << std::endl;
    return true;
}

bool MotorController::motorReverse(int motorNum, bool checkSafety) {
    if (motorNum < 0 || motorNum > 3) {
        std::cerr << "Invalid motor number. Must be 0-3" << std::endl;
        return false;
    }
    
    int base = getMotorBase(motorNum);
    
    std::cout << "\n--- REVERSE: Motor " << motorNum << " ---" << std::endl;
    
    if (checkSafety && motorStates[motorNum] == MOTOR_FORWARD) {
        std::cout << "Safety: Stopping motor before reversing direction..." << std::endl;
        motorStop(motorNum);
        Sleep(100);
    }
    
    relay.relayOff(base + 0, true);
    relay.relayOff(base + 3, true);
    Sleep(100);
    
    std::cout << "Activating relays " << (base+1) << " and " << (base+2) << std::endl;
    relay.relayOn(base + 1, true);
    relay.relayOn(base + 2, true);
    
    motorStates[motorNum] = MOTOR_REVERSE;
    
    std::cout << "Motor " << motorNum << " REVERSE" << std::endl;
    std::cout << "  Active relays: R" << (base+1) << "=ON, R" << (base+2) << "=ON" << std::endl;
    std::cout << "  Current path: V+ -> R" << (base+1) << " -> Motor_B -> Motor_A -> R" << (base+2) << " -> GND" << std::endl;
    return true;
}

bool MotorController::motorBrake(int motorNum) {
    if (motorNum < 0 || motorNum > 3) {
        std::cerr << "Invalid motor number. Must be 0-3" << std::endl;
        return false;
    }
    
    int base = getMotorBase(motorNum);
    
    std::cout << "\n--- BRAKING Motor " << motorNum << " ---" << std::endl;
    
    motorStop(motorNum);
    Sleep(50);
    
    relay.relayOn(base + 2, true);
    relay.relayOn(base + 3, true);
    
    std::cout << "Motor " << motorNum << " BRAKING" << std::endl;
    std::cout << "  Active relays: R" << (base+2) << "=ON, R" << (base+3) << "=ON" << std::endl;
    std::cout << "  Both motor terminals shorted to GND for dynamic braking" << std::endl;
    return true;
}

void MotorController::stopAll() {
    std::cout << "\n--- STOP All Motors ---" << std::endl;
    for (int i = 0; i < 4; i++) {
        motorStop(i);
    }
    std::cout << "All motors stopped!" << std::endl;
}

void MotorController::showMotorStatus() {
    std::cout << "\n--- Motor Status ---" << std::endl;
    for (int i = 0; i < 4; i++) {
        std::cout << "Motor " << i << ": ";
        switch (motorStates[i]) {
            case MOTOR_STOPPED:
                std::cout << "STOPPED";
                break;
            case MOTOR_FORWARD:
                std::cout << "FORWARD";
                break;
            case MOTOR_REVERSE:
                std::cout << "REVERSE";
                break;
        }
        
        int base = getMotorBase(i);
        std::cout << " (Relays " << base << "-" << (base+3) << ")";
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

void MotorController::explainWiring() {
    std::cout << "\n--- H-BRIDGE WIRING CONFIGURATION ---" << std::endl;
    std::cout << "\nEach motor uses 4 relays in H-bridge configuration:\n" << std::endl;
    
    for (int motor = 0; motor < 4; motor++) {
        int base = getMotorBase(motor);
        std::cout << "MOTOR " << motor << " (Relays " << base << "-" << (base+3) << "):" << std::endl;
        std::cout << "  Relay " << (base+0) << " (K1): COM=V+, NO=Motor_A" << std::endl;
        std::cout << "  Relay " << (base+1) << " (K2): COM=V+, NO=Motor_B" << std::endl;
        std::cout << "  Relay " << (base+2) << " (K3): COM=GND, NO=Motor_A" << std::endl;
        std::cout << "  Relay " << (base+3) << " (K4): COM=GND, NO=Motor_B" << std::endl;
        std::cout << std::endl;
    }
    
    std::cout << "OPERATION:" << std::endl;
    std::cout << "  Forward:  R" << 0 << "+R" << 3 << " ON (current: V+->A->B->GND)" << std::endl;
    std::cout << "  Reverse:  R" << 1 << "+R" << 2 << " ON (current: V+->B->A->GND)" << std::endl;
    std::cout << "  Stop:     All relays OFF" << std::endl;
    std::cout << "  Brake:    R" << 2 << "+R" << 3 << " ON (both terminals to GND)" << std::endl;
    std::cout << "\nWARNING: NEVER turn on K1+K3 or K2+K4 simultaneously!" << std::endl;
    std::cout << "         This creates a short circuit between V+ and GND!\n" << std::endl;
}

// ========================================
// Interactive Motor Controller Function
// ========================================

static void printMenu() {
    std::cout << "\n--- Motor Control Menu ---" << std::endl;
    std::cout << "\nMotor Commands:" << std::endl;
    std::cout << "  up <N>      - Move motor N up (forward)" << std::endl;
    std::cout << "  down <N>    - Move motor N down (reverse)" << std::endl;
    std::cout << "  stop <N>    - Stop motor N" << std::endl;
    std::cout << "  stopall     - Stop all motors" << std::endl;
    std::cout << "\nStatus Commands:" << std::endl;
    std::cout << "  motors      - Show motor status" << std::endl;
    std::cout << "  relays      - Show relay status" << std::endl;
    std::cout << "  status      - Show both motors and relays" << std::endl;
    std::cout << "\nOther Commands:" << std::endl;
    std::cout << "  wiring      - Show wiring diagram" << std::endl;
    std::cout << "  help        - Show this menu" << std::endl;
    std::cout << "  exit        - Exit program" << std::endl;
    std::cout << std::endl;
}

void runMotorController(bool testMode) {
    std::cout << "\n--- Motor Controller ---" << std::endl;
    std::cout << "--- Author: Daniel Desai ---\n" << std::endl;
    
    // Mode already selected by parameter
    std::string port;
    
    if (testMode) {
        port = "TEST";
        std::cout << "Mode: TEST" << std::endl;
    } else {
        std::cout << "Mode: OPERATING" << std::endl;
        std::cout << "Enter COM port (e.g., COM5 or just 5): ";
        std::getline(std::cin, port);
    }
    
    // Initialize controller
    NumatoRelayController relay(port, testMode);
    
    if (!relay.openPort()) {
        std::cerr << "\nFailed to open controller!" << std::endl;
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return;
    }
    
    MotorController motors(relay);
    
    std::cout << "\n--- Motor Assignments ---" << std::endl;
    std::cout << "Motor 0: Relays 0-3   |  Motor 1: Relays 4-7" << std::endl;
    std::cout << "Motor 2: Relays 8-11  |  Motor 3: Relays 12-15" << std::endl;
    std::cout << "\nMotors 0-3 available for control" << std::endl;
    
    printMenu();
    
    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        
        if (input.empty()) continue;
        
        // Convert to lowercase for parsing
        std::string lowerInput = input;
        for (char& c : lowerInput) {
            c = tolower(c);
        }
        
        // Parse command and arguments
        size_t spacePos = lowerInput.find(' ');
        std::string cmd = (spacePos != std::string::npos) ? 
                          lowerInput.substr(0, spacePos) : lowerInput;
        std::string args = (spacePos != std::string::npos) ? 
                           lowerInput.substr(spacePos + 1) : "";
        
        // Process commands
        if (cmd == "exit" || cmd == "quit" || cmd == "q") {
            std::cout << "\nStopping all motors and exiting..." << std::endl;
            motors.stopAll();
            return;
        }
        else if (cmd == "help" || cmd == "h" || cmd == "?") {
            printMenu();
        }
        else if (cmd == "up" || cmd == "u") {
            if (!args.empty()) {
                int num = atoi(args.c_str());
                if (num >= 0 && num <= 3) {
                    motors.motorForward(num);
                    std::cout << "\n[ACTIVE] Motor " << num << " moving UP" << std::endl;
                } else {
                    std::cout << "Invalid motor number. Use 0-3" << std::endl;
                }
            } else {
                std::cout << "Usage: up <motor number 0-3>" << std::endl;
            }
        }
        else if (cmd == "down" || cmd == "d") {
            if (!args.empty()) {
                int num = atoi(args.c_str());
                if (num >= 0 && num <= 3) {
                    motors.motorReverse(num);
                    std::cout << "\n[ACTIVE] Motor " << num << " moving DOWN" << std::endl;
                } else {
                    std::cout << "Invalid motor number. Use 0-3" << std::endl;
                }
            } else {
                std::cout << "Usage: down <motor number 0-3>" << std::endl;
            }
        }
        else if (cmd == "stop" || cmd == "s") {
            if (!args.empty()) {
                int num = atoi(args.c_str());
                if (num >= 0 && num <= 3) {
                    motors.motorStop(num);
                    std::cout << "[STOPPED] Motor " << num << std::endl;
                } else {
                    std::cout << "Invalid motor number. Use 0-3" << std::endl;
                }
            } else {
                std::cout << "Usage: stop <motor number 0-3>" << std::endl;
            }
        }
        else if (cmd == "stopall" || cmd == "emergency" || cmd == "estop") {
            motors.stopAll();
            std::cout << "\n[ALL STOPPED]" << std::endl;
        }
        else if (cmd == "motors" || cmd == "m") {
            motors.showMotorStatus();
        }
        else if (cmd == "relays" || cmd == "r") {
            relay.showStatus();
        }
        else if (cmd == "status" || cmd == "st") {
            std::cout << "\n========================================" << std::endl;
            std::cout << "       SYSTEM STATUS" << std::endl;
            std::cout << "========================================" << std::endl;
            motors.showMotorStatus();
            relay.showStatus();
            
            // Summary
            std::cout << "--- Quick Summary ---" << std::endl;
            std::cout << "Mode: " << (testMode ? "TEST (Simulated)" : "OPERATING (Hardware)") << std::endl;
            if (!testMode) {
                std::cout << "Port: " << port << std::endl;
            }
            std::cout << "Commands: 'up <N>', 'down <N>', 'stop <N>', 'help'" << std::endl;
            std::cout << std::endl;
        }
        else if (cmd == "wiring" || cmd == "wire" || cmd == "w") {
            motors.explainWiring();
        }
        else {
            std::cout << "Unknown command. Type 'help' for available commands." << std::endl;
        }
    }
}

// ========================================
// Automated Motor Control Function
// ========================================

void runMotorController(const std::bitset<4>& pattern, bool isReverse, bool testMode, int timeout, int delay) {
    std::cout << "\n--- Automated Motor Controller ---" << std::endl;
    std::cout << "--- Pattern: " << pattern << " (Binary: " << pattern.to_string() << ") ---" << std::endl;
    if (timeout > 0) {
        std::cout << "--- Runtime per motor: " << timeout << " seconds ---" << std::endl;
    } else {
        std::cout << "--- No timeout (manual stop required) ---" << std::endl;
    }
    if (delay > 0) {
        std::cout << "--- Delay: " << delay << " ms between motors ---\n" << std::endl;
    } else {
        std::cout << "--- No delay between motors ---\n" << std::endl;
    }
    
    // Mode already selected by parameter
    std::string port;
    
    if (testMode) {
        port = "TEST";
        std::cout << "Mode: TEST" << std::endl;
    } else {
        std::cout << "Mode: OPERATING" << std::endl;
        std::cout << "Enter COM port (e.g., COM5 or just 5): ";
        std::getline(std::cin, port);
    }
    
    // Initialize controller
    NumatoRelayController relay(port, testMode);
    
    if (!relay.openPort()) {
        std::cerr << "\nFailed to open controller!" << std::endl;
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return;
    }
    
    MotorController motors(relay);
    
    // Display the pattern
    std::cout << "\n--- Motor Control Pattern ---" << std::endl;
    std::cout << "Direction: " << (isReverse ? "REVERSE" : "FORWARD") << std::endl;
    std::cout << "Motor 0: " << (pattern[3] ? (isReverse ? "REVERSE (DOWN)" : "FORWARD (UP)") : "STOPPED") << std::endl;
    std::cout << "Motor 1: " << (pattern[2] ? (isReverse ? "REVERSE (DOWN)" : "FORWARD (UP)") : "STOPPED") << std::endl;
    std::cout << "Motor 2: " << (pattern[1] ? (isReverse ? "REVERSE (DOWN)" : "FORWARD (UP)") : "STOPPED") << std::endl;
    std::cout << "Motor 3: " << (pattern[0] ? (isReverse ? "REVERSE (DOWN)" : "FORWARD (UP)") : "STOPPED") << std::endl;
    std::cout << std::endl;
    
    // Apply the pattern with individual motor timers
    std::cout << "Applying motor pattern..." << std::endl;
    
    // Motor 0 (bit 3 - leftmost in display)
    if (pattern[3]) {
        std::cout << "\n=== Starting Motor 0 ===" << std::endl;
        if (isReverse) {
            motors.motorReverse(0);
        } else {
            motors.motorForward(0);
        }
        
        if (timeout > 0) {
            std::cout << "Motor 0 will run for " << timeout << " seconds..." << std::endl;
            for (int i = timeout; i > 0; i--) {
                std::cout << "\rMotor 0 time remaining: " << i << "s " << std::flush;
                Sleep(1000);
            }
            std::cout << "\n>>> Motor 0 timeout reached <<<" << std::endl;
            motors.motorStop(0);
        }
        
        if (delay > 0) {
            std::cout << "\nWaiting " << delay << " ms before next motor..." << std::endl;
            Sleep(delay);
        }
    } else {
        std::cout << "\n=== Motor 0 SKIPPED (stopped) ===" << std::endl;
        if (delay > 0) {
            std::cout << "Waiting " << delay << " ms before next motor..." << std::endl;
            Sleep(delay);
        }
    }
    
    // Motor 1 (bit 2)
    if (pattern[2]) {
        std::cout << "\n=== Starting Motor 1 ===" << std::endl;
        if (isReverse) {
            motors.motorReverse(1);
        } else {
            motors.motorForward(1);
        }
        
        if (timeout > 0) {
            std::cout << "Motor 1 will run for " << timeout << " seconds..." << std::endl;
            for (int i = timeout; i > 0; i--) {
                std::cout << "\rMotor 1 time remaining: " << i << "s " << std::flush;
                Sleep(1000);
            }
            std::cout << "\n>>> Motor 1 timeout reached <<<" << std::endl;
            motors.motorStop(1);
        }
        
        if (delay > 0) {
            std::cout << "\nWaiting " << delay << " ms before next motor..." << std::endl;
            Sleep(delay);
        }
    } else {
        std::cout << "\n=== Motor 1 SKIPPED (stopped) ===" << std::endl;
        if (delay > 0) {
            std::cout << "Waiting " << delay << " ms before next motor..." << std::endl;
            Sleep(delay);
        }
    }
    
    // Motor 2 (bit 1)
    if (pattern[1]) {
        std::cout << "\n=== Starting Motor 2 ===" << std::endl;
        if (isReverse) {
            motors.motorReverse(2);
        } else {
            motors.motorForward(2);
        }
        
        if (timeout > 0) {
            std::cout << "Motor 2 will run for " << timeout << " seconds..." << std::endl;
            for (int i = timeout; i > 0; i--) {
                std::cout << "\rMotor 2 time remaining: " << i << "s " << std::flush;
                Sleep(1000);
            }
            std::cout << "\n>>> Motor 2 timeout reached <<<" << std::endl;
            motors.motorStop(2);
        }
        
        if (delay > 0) {
            std::cout << "\nWaiting " << delay << " ms before next motor..." << std::endl;
            Sleep(delay);
        }
    } else {
        std::cout << "\n=== Motor 2 SKIPPED (stopped) ===" << std::endl;
        if (delay > 0) {
            std::cout << "Waiting " << delay << " ms before next motor..." << std::endl;
            Sleep(delay);
        }
    }
    
    // Motor 3 (bit 0 - rightmost)
    if (pattern[0]) {
        std::cout << "\n=== Starting Motor 3 ===" << std::endl;
        if (isReverse) {
            motors.motorReverse(3);
        } else {
            motors.motorForward(3);
        }
        
        if (timeout > 0) {
            std::cout << "Motor 3 will run for " << timeout << " seconds..." << std::endl;
            for (int i = timeout; i > 0; i--) {
                std::cout << "\rMotor 3 time remaining: " << i << "s " << std::flush;
                Sleep(1000);
            }
            std::cout << "\n>>> Motor 3 timeout reached <<<" << std::endl;
            motors.motorStop(3);
        }
    } else {
        std::cout << "\n=== Motor 3 SKIPPED (stopped) ===" << std::endl;
    }
    
    if (timeout > 0) {
        std::cout << "\n========================================" << std::endl;
        std::cout << "All motors completed run cycles" << std::endl;
        std::cout << "========================================\n" << std::endl;
    } else {
        // No timeout - wait for user to press Enter
        std::cout << "\nPress Enter to stop all motors and exit...";
        std::cin.get();
    }
    
    std::cout << "\n\nStopping all motors..." << std::endl;
    motors.stopAll();
    
    std::cout << "\nReturning to main program..." << std::endl;
    std::cout << "Program complete." << std::endl;
}