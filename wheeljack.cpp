// <Filename>: <wheeljack.cpp>
// <Author>:   <DANIEL DESAI>
// <Updated>:  <2026-03-04>
// <Version>:  <0.0.2>

#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <vector>
#include <bitset>
#include <string>
#include <array>
#include <limits>
#include "motor_controller.h"
#include "rigol_controller.h"

// ========================================
// Structs
// ========================================

struct TestModeConfig {
    bool powerSupplyTest;
    bool motorControlTest;
};

// ========================================
// UI Functions
// ========================================

auto test_mode_ui() -> TestModeConfig {
    auto config = TestModeConfig{};

    std::cout << "Select test mode configuration:" << std::endl;
    std::cout << "  1. Normal mode (both devices required)" << std::endl;
    std::cout << "  2. Test power supply only (no Rigol required)" << std::endl;
    std::cout << "  3. Test motor control only (no relay board required)" << std::endl;
    std::cout << "  4. Test both (no hardware required)" << std::endl;
    std::cout << "Enter selection (1-4): ";

    auto selection = int{};
    std::cin >> selection;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    switch (selection) {
        case 1:
            config.powerSupplyTest  = false;
            config.motorControlTest = false;
            std::cout << "\n>>> NORMAL MODE <<<" << std::endl;
            std::cout << "  - Connecting to physical Rigol power supply" << std::endl;
            std::cout << "  - Using physical relay control board\n" << std::endl;
            break;
        case 2:
            config.powerSupplyTest  = true;
            config.motorControlTest = false;
            std::cout << "\n>>> POWER SUPPLY TEST MODE <<<" << std::endl;
            std::cout << "  - Simulating Rigol power supply (no device required)" << std::endl;
            std::cout << "  - Using physical relay control board\n" << std::endl;
            break;
        case 3:
            config.powerSupplyTest  = false;
            config.motorControlTest = true;
            std::cout << "\n>>> MOTOR CONTROL TEST MODE <<<" << std::endl;
            std::cout << "  - Connecting to physical Rigol power supply" << std::endl;
            std::cout << "  - Simulating relay control board (no device required)\n" << std::endl;
            break;
        case 4:
            config.powerSupplyTest  = true;
            config.motorControlTest = true;
            std::cout << "\n>>> FULL TEST MODE <<<" << std::endl;
            std::cout << "  - Simulating Rigol power supply (no device required)" << std::endl;
            std::cout << "  - Simulating relay control board (no device required)" << std::endl;
            std::cout << "  - All hardware interactions will be simulated\n" << std::endl;
            break;
        default:
            std::cout << "\n>>> Invalid selection, defaulting to NORMAL MODE <<<" << std::endl;
            config.powerSupplyTest  = false;
            config.motorControlTest = false;
            break;
    }

    return config;
}

auto comport_ui() -> std::string {
    auto port = std::string{};
    std::cout << "Enter COM port (e.g., COM6 or just 6): ";
    std::getline(std::cin, port);
    return port;
}

auto pattern_ui() -> std::bitset<4> {
    auto input = std::string{};
    std::cout << "Enter motor run pattern (4-bit binary, e.g., 1010 or 0b1010): ";
    std::cin >> input;

    if (input.substr(0, 2) == "0b" || input.substr(0, 2) == "0B")
        input = input.substr(2);

    if (input.length() != 4) {
        std::cout << "Invalid input length. Using default pattern 0000" << std::endl;
        return std::bitset<4>("0000");
    }

    for (auto c : input) {
        if (c != '0' && c != '1') {
            std::cout << "Invalid characters. Using default pattern 0000" << std::endl;
            return std::bitset<4>("0000");
        }
    }

    return std::bitset<4>(input);
}

auto direction_ui() -> bool {
    auto input = std::string{};
    std::cout << "Enter direction (f=forward, r=reverse): ";
    std::cin >> input;

    if (!input.empty() && tolower(input[0]) == 'r')
        return true;
    return false;
}

auto voltage_ui() -> float {
    auto number = float{};
    std::cout << "Enter motor voltage: ";
    std::cin >> number;
    return number;
}

auto current_ui() -> float {
    auto number = float{};
    std::cout << "Enter motor current limit: ";
    std::cin >> number;
    return number;
}

auto runtime_ui() -> float {
    auto number = float{};
    std::cout << "Enter motor runtime (seconds): ";
    std::cin >> number;
    return number;
}

// ========================================
// Core Functions
// ========================================

auto jack_up(const std::bitset<4>& pattern, bool isReverse, float voltage, float current,
             float runtime, const std::string& comPort, TestModeConfig testConfig) -> int {
    try {
        auto ps      = RigolDP831("", testConfig.powerSupplyTest);
        auto channel = int{2};

        auto isOff = ps.getOutputState(channel);
        std::cout << "  Output State: " << (isOff ? "ON" : "OFF") << std::endl;

        auto meas = ps.getAllMeasurements(channel);
        std::cout << "\nChannel " << channel << " Initial Measurements:" << std::endl;
        std::cout << "  Voltage: " << meas.voltage << " V" << std::endl;
        std::cout << "  Current: " << meas.current << " A" << std::endl;
        std::cout << "  Power:   " << meas.power   << " W" << std::endl;

        std::cout << "\nEnabling power supply..." << std::endl;
        ps.enableOutput(channel, true);
        ps.setVoltage(channel, voltage);
        ps.setCurrent(channel, current);

        auto setVoltage = ps.getSetVoltage(channel);
        auto setCurrent = ps.getSetCurrent(channel);
        std::cout << "\nChannel " << channel << " Settings:" << std::endl;
        std::cout << "  Set Voltage: "       << setVoltage << " V" << std::endl;
        std::cout << "  Set Current Limit: " << setCurrent << " A" << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::cout << "\nStarting motor with pattern: " << pattern
                  << " (" << pattern.to_string() << ")" << std::endl;
        std::cout << "Direction: " << (isReverse ? "REVERSE" : "FORWARD") << std::endl;
        std::cout << "Runtime: "   << runtime << " seconds" << std::endl;

        auto start_time    = std::chrono::steady_clock::now();
        auto motor_running = std::atomic<bool>{true};

        auto motor_thread = std::thread([pattern, isReverse, runtime, comPort, &motor_running, &testConfig]() {
            if (testConfig.motorControlTest) {
                std::cout << "[MOTOR TEST MODE] Simulating motor pattern " << pattern
                          << " (" << (isReverse ? "reverse" : "forward") << ") for "
                          << runtime << " seconds" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(runtime * 1000)));
            } else {
                runMotorController(pattern, isReverse, false, comPort, static_cast<int>(runtime), 0);
            }
            motor_running = false;
        });

        std::cout << "\n=== Monitoring Started (updates every 1 second) ===" << std::endl;
        std::cout << "Stopping conditions: Runtime exceeded OR current limiting detected" << std::endl;
        std::cout << std::fixed << std::setprecision(3);

        constexpr auto CURRENT_WINDOW_SIZE     = int{5};
        constexpr auto REQUIRED_STABLE_SAMPLES = int{4};
        const     auto CURRENT_LIMIT           = 0.95 * setCurrent;

        auto current_history    = std::vector<double>{};
        auto stable_count       = int{0};
        auto limit_hit_count    = int{0};
        auto monitoring_started = bool{false};
        auto sample_count       = int{0};
        auto elapsed_seconds    = float{0.0f};
        auto should_stop        = bool{false};
        auto stop_reason        = std::string{};

        while (!should_stop) {
            sample_count++;

            auto current_time = std::chrono::steady_clock::now();
            auto elapsed      = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time);
            elapsed_seconds   = elapsed.count() / 1000.0f;

            if (elapsed_seconds >= runtime) {
                should_stop = true;
                stop_reason = "Runtime reached";
                std::cout << "\n>>> Runtime limit reached, stopping monitoring <<<\n" << std::endl;
            }

            auto meas2    = ps.getAllMeasurements(channel);
            auto outputOn = ps.getOutputState(channel);

            current_history.push_back(meas2.current);

            if (static_cast<int>(current_history.size()) >= CURRENT_WINDOW_SIZE && !should_stop) {
                if (static_cast<int>(current_history.size()) > CURRENT_WINDOW_SIZE)
                    current_history.erase(current_history.begin());

                if (!monitoring_started) {
                    monitoring_started = true;
                    std::cout << ">>> Starting current stabilization monitoring <<<" << std::endl;
                }

                auto min_current = current_history[0];
                auto max_current = current_history[0];
                auto sum_current = double{0.0};

                for (const auto& curr : current_history) {
                    if (curr < min_current) min_current = curr;
                    if (curr > max_current) max_current = curr;
                    sum_current += curr;
                }

                auto avg_current = sum_current / static_cast<double>(current_history.size());

                limit_hit_count = 0;
                for (const auto& curr : current_history) {
                    if (curr >= CURRENT_LIMIT) limit_hit_count++;
                }

                if (limit_hit_count >= 3) {
                    stable_count++;
                    std::cout << ">>> Current limiting detected: " << stable_count
                              << "/" << REQUIRED_STABLE_SAMPLES
                              << " (" << limit_hit_count << "/5 samples at limit, avg: "
                              << avg_current << "A) <<<" << std::endl;

                    if (stable_count >= REQUIRED_STABLE_SAMPLES) {
                        should_stop = true;
                        stop_reason = "Current limiting";
                        std::cout << "\n>>> CURRENT LIMITING - MOTOR STALLED - STOPPING PERMANENTLY <<<\n" << std::endl;
                    }
                } else {
                    if (stable_count > 0) {
                        std::cout << ">>> Current normal (" << limit_hit_count
                                  << "/5 at limit, avg: " << avg_current
                                  << "A), resetting counter <<<" << std::endl;
                    }
                    stable_count = 0;
                }

                std::cout << "[Sample " << std::setw(4) << sample_count << "] "
                          << "Time: "    << std::setw(6) << elapsed_seconds << "s  "
                          << "V: "       << std::setw(7) << meas2.voltage   << "V  "
                          << "I: "       << std::setw(7) << meas2.current   << "A  "
                          << "AvgI: "    << std::setw(6) << avg_current     << "A  "
                          << "AtLimit: " << limit_hit_count                 << "/5  "
                          << "P: "       << std::setw(7) << meas2.power     << "W  "
                          << "Stable: "  << std::setw(2) << stable_count
                          << "/" << REQUIRED_STABLE_SAMPLES << "  "
                          << "Output: "  << (outputOn ? "ON " : "OFF")
                          << std::endl;
            } else {
                std::cout << "[Sample " << std::setw(4) << sample_count << "] "
                          << "Time: "   << std::setw(6) << elapsed_seconds << "s  "
                          << "V: "      << std::setw(7) << meas2.voltage   << "V  "
                          << "I: "      << std::setw(7) << meas2.current   << "A  "
                          << "P: "      << std::setw(7) << meas2.power     << "W  "
                          << "Output: " << (outputOn ? "ON " : "OFF")
                          << std::endl;
            }

            if (should_stop) {
                std::cout << "\n>>> Breaking out of monitoring loop <<<\n" << std::endl;
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        std::cout << "\n>>> Exited monitoring loop <<<\n" << std::endl;
        std::cout << "\n=== " << stop_reason << " (" << elapsed_seconds << "s) ===" << std::endl;

        if (stop_reason == "Current limiting")
            std::cout << "Motor stopped because current limiting detected (met resistance)" << std::endl;
        else if (stop_reason == "Runtime reached")
            std::cout << "Motor stopped because runtime limit was reached" << std::endl;

        if (motor_thread.joinable())
            motor_thread.join();

        std::cout << "Stopping motor..." << std::endl;
        if (testConfig.motorControlTest) {
            std::cout << "[MOTOR TEST MODE] Simulating motor stop" << std::endl;
        } else {
            auto stopPattern = std::bitset<4>("0000");
            runMotorController(stopPattern, false, false, comPort, 0, 0);
        }

        std::cout << "Disabling power supply..." << std::endl;
        ps.setVoltage(channel, 0.0);
        ps.setCurrent(channel, 0.0);
        ps.enableOutput(channel, false);

        auto finalState = ps.getOutputState(channel);
        std::cout << "  Final Output State: " << (finalState ? "ON" : "OFF") << std::endl;
        std::cout << "Connection closed." << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

void emergency_shutoff(TestModeConfig testConfig) {
    try {
        std::cout << "=== EMERGENCY SHUTOFF: Disabling power supply ===" << std::endl;
        auto ps      = RigolDP831("", testConfig.powerSupplyTest);
        auto channel = int{2};

        ps.enableOutput(channel, false);
        ps.setVoltage(channel, 0.0);
        ps.setCurrent(channel, 0.0);

        std::cout << "Power supply output DISABLED for safety." << std::endl;
        auto isOff = ps.getOutputState(channel);
        std::cout << "Output State: " << (isOff ? "ON" : "OFF") << std::endl;
        std::cout << "=== Safe to proceed with user input ===\n" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error during emergency shutoff: " << e.what() << std::endl;
    }
}

// ========================================
// Main
// ========================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Power Supply/Motor Controller System  " << std::endl;
    std::cout << "========================================\n" << std::endl;

    auto testConfig = test_mode_ui();
    emergency_shutoff(testConfig);

    // Only prompt for COM port if relay board hardware is needed
    auto comPort = std::string{"TEST"};
    if (!testConfig.motorControlTest)
        comPort = comport_ui();

    auto pattern   = pattern_ui();
    auto isReverse = direction_ui();
    auto voltage   = voltage_ui();
    auto current   = current_ui();
    auto runtime   = runtime_ui();

    return jack_up(pattern, isReverse, voltage, current, runtime, comPort, testConfig);
}