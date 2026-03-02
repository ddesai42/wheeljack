#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <vector>
#include <bitset>
#include <string>
#include "motor_controller.h"
#include "rigol_controller.h"

// Structure to hold test mode configuration
struct TestModeConfig {
    bool powerSupplyTest;
    bool motorControlTest;
};

// Function to get test mode selection
TestModeConfig test_mode_ui() {
    TestModeConfig config;
    std::string input;
    
    std::cout << "Select test mode configuration:" << std::endl;
    std::cout << "  1. Normal mode (both devices required)" << std::endl;
    std::cout << "  2. Test power supply only (no Rigol required)" << std::endl;
    std::cout << "  3. Test motor control only (no relay board required)" << std::endl;
    std::cout << "  4. Test both (no hardware required)" << std::endl;
    std::cout << "Enter selection (1-4): ";
    
    int selection;
    std::cin >> selection;
    
    switch(selection) {
        case 1:
            config.powerSupplyTest = false;
            config.motorControlTest = false;
            std::cout << "\n>>> NORMAL MODE <<<" << std::endl;
            std::cout << "  - Connecting to physical Rigol power supply" << std::endl;
            std::cout << "  - Using physical relay control board\n" << std::endl;
            break;
            
        case 2:
            config.powerSupplyTest = true;
            config.motorControlTest = false;
            std::cout << "\n>>> POWER SUPPLY TEST MODE <<<" << std::endl;
            std::cout << "  - Simulating Rigol power supply (no device required)" << std::endl;
            std::cout << "  - Using physical relay control board\n" << std::endl;
            break;
            
        case 3:
            config.powerSupplyTest = false;
            config.motorControlTest = true;
            std::cout << "\n>>> MOTOR CONTROL TEST MODE <<<" << std::endl;
            std::cout << "  - Connecting to physical Rigol power supply" << std::endl;
            std::cout << "  - Simulating relay control board (no device required)\n" << std::endl;
            break;
            
        case 4:
            config.powerSupplyTest = true;
            config.motorControlTest = true;
            std::cout << "\n>>> FULL TEST MODE <<<" << std::endl;
            std::cout << "  - Simulating Rigol power supply (no device required)" << std::endl;
            std::cout << "  - Simulating relay control board (no device required)" << std::endl;
            std::cout << "  - All hardware interactions will be simulated\n" << std::endl;
            break;
            
        default:
            std::cout << "\n>>> Invalid selection, defaulting to NORMAL MODE <<<" << std::endl;
            config.powerSupplyTest = false;
            config.motorControlTest = false;
            break;
    }
    
    return config;
}

// Function to get motor pattern as bitset
std::bitset<4> pattern_ui() {
    std::string input;
    std::cout << "Enter motor run pattern (4-bit binary, e.g., 1010 or 0b1010): ";
    std::cin >> input;
    
    // Handle both "1010" and "0b1010" formats
    if (input.substr(0, 2) == "0b" || input.substr(0, 2) == "0B") {
        input = input.substr(2);
    }
    
    // Validate input
    if (input.length() != 4) {
        std::cout << "Invalid input length. Using default pattern 0000" << std::endl;
        return std::bitset<4>("0000");
    }
    
    for (char c : input) {
        if (c != '0' && c != '1') {
            std::cout << "Invalid characters. Using default pattern 0000" << std::endl;
            return std::bitset<4>("0000");
        }
    }
    
    return std::bitset<4>(input);
}

// Function to get direction
bool direction_ui() {
    std::string input;
    std::cout << "Enter direction (f=forward, r=reverse): ";
    std::cin >> input;
    
    if (input.length() > 0) {
        char c = tolower(input[0]);
        if (c == 'r') {
            return true;  // reverse
        }
    }
    return false;  // forward (default)
}

float voltage_ui() {
    float number;
    std::cout << "Enter motor voltage: ";
    std::cin >> number;
    return number;
}

float current_ui() {
    float number;
    std::cout << "Enter motor current limit: ";
    std::cin >> number;
    return number;
}

float runtime_ui() {
    float number;
    std::cout << "Enter motor runtime (seconds): ";
    std::cin >> number;
    return number;
}

int jack_up(const std::bitset<4>& pattern, bool isReverse, float voltage, float current, float runtime, TestModeConfig testConfig) {
    try {
        // Connect to power supply with test mode flag
        RigolDP831 ps("", testConfig.powerSupplyTest);
        
        // Select channel to work with
        int channel = 2;

        // Verify output is off
        bool isOff = ps.getOutputState(channel);
        std::cout << "  Output State: " << (isOff ? "ON" : "OFF") << std::endl;
        
        // Read initial measurements
        RigolDP831::Measurements meas = ps.getAllMeasurements(channel);
        std::cout << "\nChannel " << channel << " Initial Measurements:" << std::endl;
        std::cout << "  Voltage: " << meas.voltage << " V" << std::endl;
        std::cout << "  Current: " << meas.current << " A" << std::endl;
        std::cout << "  Power:   " << meas.power << " W" << std::endl;

        // Enable output and energize
        std::cout << "\nEnabling power supply..." << std::endl;
        ps.enableOutput(channel, true);
        ps.setVoltage(channel, voltage);
        ps.setCurrent(channel, current);
        
        // Read set values
        double setVoltage = ps.getSetVoltage(channel);
        double setCurrent = ps.getSetCurrent(channel);
        std::cout << "\nChannel " << channel << " Settings:" << std::endl;
        std::cout << "  Set Voltage: " << setVoltage << " V" << std::endl;
        std::cout << "  Set Current Limit: " << setCurrent << " A" << std::endl;
        
        // Wait for power to stabilize
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Start motor
        std::cout << "\nStarting motor with pattern: " << pattern << " (" << pattern.to_string() << ")" << std::endl;
        std::cout << "Direction: " << (isReverse ? "REVERSE" : "FORWARD") << std::endl;
        std::cout << "Runtime: " << runtime << " seconds" << std::endl;
        
        // Record start time
        auto start_time = std::chrono::steady_clock::now();
        
        // Start motor controller in separate thread
        std::atomic<bool> motor_running(true);
        std::thread motor_thread([pattern, isReverse, runtime, &motor_running, &testConfig]() {
            if (testConfig.motorControlTest) {
                std::cout << "[MOTOR TEST MODE] Simulating motor pattern " << pattern 
                          << " (" << (isReverse ? "reverse" : "forward") << ") for " << runtime << " seconds" << std::endl;
                // Simulate motor running
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(runtime * 1000)));
            } else {
                runMotorController(pattern, isReverse, false, runtime, 0);
            }
            motor_running = false;
        });
        
        std::cout << "\n=== Monitoring Started (updates every 1 second) ===" << std::endl;
        std::cout << "Stopping conditions: Runtime exceeded OR current limiting detected" << std::endl;
        std::cout << std::fixed << std::setprecision(3);

        // Current monitoring parameters
        const int CURRENT_WINDOW_SIZE = 5;
        std::vector<double> current_history;
        int stable_count = 0;
        const int REQUIRED_STABLE_SAMPLES = 4;
        const double CURRENT_LIMIT = 0.95 * setCurrent;  // 95% of set current limit
        int limit_hit_count = 0;
        bool monitoring_started = false;
        
        // Monitor with time-based and current-based shutoff
        int sample_count = 0;
        float elapsed_seconds = 0.0f;
        bool should_stop = false;
        std::string stop_reason = "";
        
        // Run monitoring loop
        while (!should_stop) {
            sample_count++;
            
            // Calculate elapsed time
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
            elapsed_seconds = elapsed.count() / 1000.0f;
            
            // Check if runtime exceeded
            if (elapsed_seconds >= runtime) {
                should_stop = true;
                stop_reason = "Runtime reached";
                std::cout << "\n>>> Runtime limit reached, stopping monitoring <<<\n" << std::endl;
            }
            
            // Get all measurements
            RigolDP831::Measurements meas2 = ps.getAllMeasurements(channel);
            bool outputOn = ps.getOutputState(channel);
            
            // Add current to history
            current_history.push_back(meas2.current);
            
            // Check current stabilization if we have enough samples
            if (current_history.size() >= CURRENT_WINDOW_SIZE && !should_stop) {
                // Keep only the last CURRENT_WINDOW_SIZE samples
                if (current_history.size() > CURRENT_WINDOW_SIZE) {
                    current_history.erase(current_history.begin());
                }
                
                // Start monitoring after initial samples
                if (!monitoring_started) {
                    monitoring_started = true;
                    std::cout << ">>> Starting current stabilization monitoring <<<" << std::endl;
                }
                
                // Calculate current statistics
                double min_current = current_history[0];
                double max_current = current_history[0];
                double sum_current = 0.0;
                
                for (const auto& curr : current_history) {
                    if (curr < min_current) min_current = curr;
                    if (curr > max_current) max_current = curr;
                    sum_current += curr;
                }
                
                double avg_current = sum_current / current_history.size();
                
                // Count how many samples in the window are at the current limit
                limit_hit_count = 0;
                for (const auto& curr : current_history) {
                    if (curr >= CURRENT_LIMIT) {
                        limit_hit_count++;
                    }
                }
                
                // Motor is stalled if 3 or more of the last 5 samples hit current limit
                if (limit_hit_count >= 3) {
                    stable_count++;
                    std::cout << ">>> Current limiting detected: " << stable_count << "/" << REQUIRED_STABLE_SAMPLES 
                              << " (" << limit_hit_count << "/5 samples at limit, avg: " << avg_current << "A) <<<" << std::endl;
                    
                    // If we've seen current limiting for 4 consecutive checks, motor is stalled
                    if (stable_count >= REQUIRED_STABLE_SAMPLES) {
                        should_stop = true;
                        stop_reason = "Current limiting";
                        std::cout << "\n>>> CURRENT LIMITING - MOTOR STALLED - STOPPING PERMANENTLY <<<\n" << std::endl;
                    }
                } else {
                    // Not enough samples at limit - motor running normally
                    if (stable_count > 0) {
                        std::cout << ">>> Current normal (" << limit_hit_count 
                                  << "/5 at limit, avg: " << avg_current << "A), resetting counter <<<" << std::endl;
                    }
                    stable_count = 0;
                }
                
                // Print with current monitoring info
                std::cout << "[Sample " << std::setw(4) << sample_count << "] "
                          << "Time: " << std::setw(6) << elapsed_seconds << "s  "
                          << "V: " << std::setw(7) << meas2.voltage << "V  "
                          << "I: " << std::setw(7) << meas2.current << "A  "
                          << "AvgI: " << std::setw(6) << avg_current << "A  "
                          << "AtLimit: " << limit_hit_count << "/5  "
                          << "P: " << std::setw(7) << meas2.power << "W  "
                          << "Stable: " << std::setw(2) << stable_count << "/" << REQUIRED_STABLE_SAMPLES << "  "
                          << "Output: " << (outputOn ? "ON " : "OFF")
                          << std::endl;
            } else {
                // Not enough samples yet
                std::cout << "[Sample " << std::setw(4) << sample_count << "] "
                          << "Time: " << std::setw(6) << elapsed_seconds << "s  "
                          << "V: " << std::setw(7) << meas2.voltage << "V  "
                          << "I: " << std::setw(7) << meas2.current << "A  "
                          << "P: " << std::setw(7) << meas2.power << "W  "
                          << "Output: " << (outputOn ? "ON " : "OFF")
                          << std::endl;
            }
            
            // If we should stop, break immediately
            if (should_stop) {
                std::cout << "\n>>> Breaking out of monitoring loop <<<\n" << std::endl;
                break;
            }
            
            // Wait 1 second before next reading
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        std::cout << "\n>>> Exited monitoring loop <<<\n" << std::endl;
        
        // Determine why we stopped and print message
        std::cout << "\n=== " << stop_reason << " (" << elapsed_seconds << "s) ===" << std::endl;
        
        if (stop_reason == "Current limiting") {
            std::cout << "Motor stopped because current limiting detected (met resistance)" << std::endl;
        } else if (stop_reason == "Runtime reached") {
            std::cout << "Motor stopped because runtime limit was reached" << std::endl;
        }
        
        // Wait for motor thread to finish
        if (motor_thread.joinable()) {
            motor_thread.join();
        }
        
        // Stop motor and disable power
        std::cout << "Stopping motor..." << std::endl;
        if (testConfig.motorControlTest) {
            std::cout << "[MOTOR TEST MODE] Simulating motor stop" << std::endl;
        } else {
            // Stop all motors using bitset with all zeros
            std::bitset<4> stopPattern("0000");
            runMotorController(stopPattern, false, false, 1, 0);
        }
        
        std::cout << "Disabling power supply..." << std::endl;
        ps.setVoltage(channel, 0.0);
        ps.setCurrent(channel, 0.0);
        ps.enableOutput(channel, false);
        
        // Check final output state
        bool finalState = ps.getOutputState(channel);
        std::cout << "  Final Output State: " << (finalState ? "ON" : "OFF") << std::endl;
        std::cout << "Connection closed." << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

// Function to safely shut off power supply at startup
void emergency_shutoff(TestModeConfig testConfig) {
    try {
        std::cout << "=== EMERGENCY SHUTOFF: Disabling power supply ===" << std::endl;
        RigolDP831 ps("", testConfig.powerSupplyTest);
        int channel = 2;
        
        ps.enableOutput(channel, false);
        ps.setVoltage(channel, 0.0);
        ps.setCurrent(channel, 0.0);
        
        std::cout << "Power supply output DISABLED for safety." << std::endl;
        bool isOff = ps.getOutputState(channel);
        std::cout << "Output State: " << (isOff ? "ON" : "OFF") << std::endl;
        std::cout << "=== Safe to proceed with user input ===\n" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during emergency shutoff: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Power Supply/Motor Controller System  " << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // FIRST: Ask about test mode
    TestModeConfig testConfig = test_mode_ui();
    
    // SECOND: Emergency shutoff with appropriate mode
    emergency_shutoff(testConfig);
    
    // NOW: Get user input for motor parameters
    auto pattern = pattern_ui();
    auto isReverse = direction_ui();
    auto voltage = voltage_ui();
    auto current = current_ui();
    auto runtime = runtime_ui();

    return jack_up(pattern, isReverse, voltage, current, runtime, testConfig);
}