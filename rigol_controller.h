// rigol_controller.h
// Rigol DP831 Programmable DC Power Supply Controller Header
// with Test Mode Support

#ifndef RIGOL_CONTROLLER_H
#define RIGOL_CONTROLLER_H

#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>
#include <visa.h>

class RigolDP831 {
public:
    // Structure to hold measurement data
    struct Measurements {
        double voltage;
        double current;
        double power;
    };
    
    // Structure to hold test mode state
    struct TestState {
        std::map<int, double> setVoltage;
        std::map<int, double> setCurrent;
        std::map<int, bool> outputEnabled;
        std::map<int, double> actualVoltage;
        std::map<int, double> actualCurrent;
    };
    
    // Constructor
    // If testMode is true, no actual device connection is made
    RigolDP831(const std::string& resourceString = "", bool testMode = true);
    
    // Destructor
    ~RigolDP831();
    
    // Channel control methods
    void setVoltage(int channel, double voltage);
    void setCurrent(int channel, double current);
    void enableOutput(int channel, bool state);
    
    // Measurement methods
    double getVoltage(int channel);
    double getCurrent(int channel);
    double getPower(int channel);
    Measurements getAllMeasurements(int channel);
    
    // Query set values
    double getSetVoltage(int channel);
    double getSetCurrent(int channel);
    bool getOutputState(int channel);
    
    // Check if running in test mode
    bool isTestMode() const { return testMode; }
    
private:
    // VISA handles
    ViSession defaultRM;
    ViSession instrument;
    ViStatus status;
    
    // Test mode flag and state
    bool testMode;
    TestState testState;
    
    // Internal methods
    void write(const std::string& command);
    std::string query(const std::string& command);
    std::string findDevice();
    std::string queryTemp(ViSession instr, const std::string& command);
    void close();
    
    // Test mode methods
    void initializeTestState();
    void updateTestMeasurements(int channel);
};

#endif // RIGOL_CONTROLLER_H