// rigol_controller.cpp
// Rigol DP831 Programmable DC Power Supply Controller Implementation
// with Test Mode Support

#include "rigol_controller.h"
#include <thread>
#include <chrono>
#include <random>

// Constructor
RigolDP831::RigolDP831(const std::string& resourceString, bool testMode) 
    : testMode(testMode), defaultRM(0), instrument(0) {
    
    if (testMode) {
        std::cout << "=== TEST MODE ENABLED ===" << std::endl;
        std::cout << "Running without physical device connection" << std::endl;
        initializeTestState();
        return;
    }
    
    // Normal mode - connect to actual device
    status = viOpenDefaultRM(&defaultRM);
    if (status < VI_SUCCESS) {
        throw std::runtime_error("Could not open VISA Resource Manager");
    }
    
    std::string resource = resourceString;
    if (resource.empty()) {
        resource = findDevice();
    }
    
    status = viOpen(defaultRM, resource.c_str(), VI_NULL, VI_NULL, &instrument);
    if (status < VI_SUCCESS) {
        viClose(defaultRM);
        throw std::runtime_error("Could not open instrument: " + resource);
    }
    
    viSetAttribute(instrument, VI_ATTR_TMO_VALUE, 5000);
    
    std::string idn = query("*IDN?");
    std::cout << "Connected to: " << idn << std::endl;
}

// Destructor
RigolDP831::~RigolDP831() {
    close();
}

// Initialize test mode state
void RigolDP831::initializeTestState() {
    for (int i = 1; i <= 3; i++) {
        testState.setVoltage[i] = 0.0;
        testState.setCurrent[i] = 1.0;  // Default current limit
        testState.outputEnabled[i] = false;
        testState.actualVoltage[i] = 0.0;
        testState.actualCurrent[i] = 0.0;
    }
}

void RigolDP831::updateTestMeasurements(int channel) {
    if (!testMode) return;
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    if (testState.outputEnabled[channel]) {
        std::uniform_real_distribution<> voltNoise(-0.001, 0.001);
        std::uniform_real_distribution<> currNoise(-0.0001, 0.0001);
        
        testState.actualVoltage[channel] = testState.setVoltage[channel] + voltNoise(gen);
        testState.actualCurrent[channel] = testState.setCurrent[channel] + currNoise(gen);
    } else {
        testState.actualVoltage[channel] = 0.0;
        testState.actualCurrent[channel] = 0.0;
    }
}

std::string RigolDP831::findDevice() {
    ViFindList findList;
    ViUInt32 numInstrs;
    char instrDescr[VI_FIND_BUFLEN];
    
    status = viFindRsrc(defaultRM, "USB?*INSTR", &findList, &numInstrs, instrDescr);
    
    if (status < VI_SUCCESS) {
        throw std::runtime_error("No VISA resources found");
    }
    
    std::cout << "Available resources: " << numInstrs << std::endl;
    
    for (ViUInt32 i = 0; i < numInstrs; i++) {
        ViSession tempInstr;
        
        if (i > 0) {
            viFindNext(findList, instrDescr);
        }
        
        if (viOpen(defaultRM, instrDescr, VI_NULL, VI_NULL, &tempInstr) >= VI_SUCCESS) {
            std::string idn = queryTemp(tempInstr, "*IDN?");
            
            if (idn.find("RIGOL") != std::string::npos && 
                idn.find("DP831") != std::string::npos) {
                viClose(tempInstr);
                viClose(findList);
                return std::string(instrDescr);
            }
            viClose(tempInstr);
        }
    }
    
    viClose(findList);
    throw std::runtime_error("Rigol DP831 not found. Please specify resource string manually.");
}

std::string RigolDP831::queryTemp(ViSession instr, const std::string& command) {
    char buffer[256];
    ViUInt32 retCount;
    
    viWrite(instr, (ViBuf)command.c_str(), command.length(), &retCount);
    viRead(instr, (ViBuf)buffer, 255, &retCount);
    buffer[retCount] = '\0';
    
    std::string result(buffer);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
}

void RigolDP831::write(const std::string& command) {
    if (testMode) {
        std::cout << "[TEST] Write: " << command << std::endl;
        return;
    }
    
    ViUInt32 retCount;
    status = viWrite(instrument, (ViBuf)command.c_str(), command.length(), &retCount);
    if (status < VI_SUCCESS) {
        throw std::runtime_error("Write failed");
    }
}

std::string RigolDP831::query(const std::string& command) {
    if (testMode) {
        std::cout << "[TEST] Query: " << command << std::endl;
        
        // Simulate responses for common queries
        if (command == "*IDN?") {
            return "RIGOL TECHNOLOGIES,DP831,TEST123456,00.01.14";
        }
        
        return "[TEST MODE RESPONSE]";
    }
    
    char buffer[256];
    ViUInt32 retCount;
    
    status = viWrite(instrument, (ViBuf)command.c_str(), command.length(), &retCount);
    if (status < VI_SUCCESS) {
        throw std::runtime_error("Query write failed");
    }
    
    status = viRead(instrument, (ViBuf)buffer, 255, &retCount);
    if (status < VI_SUCCESS) {
        throw std::runtime_error("Query read failed");
    }
    
    buffer[retCount] = '\0';
    
    std::string result(buffer);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

void RigolDP831::setVoltage(int channel, double voltage) {
    if (testMode) {
        testState.setVoltage[channel] = voltage;
        updateTestMeasurements(channel);
        std::cout << "[TEST] Channel " << channel << " voltage set to " << voltage << "V" << std::endl;
        return;
    }
    
    std::ostringstream cmd;
    cmd << ":SOUR" << channel << ":VOLT " << voltage;
    write(cmd.str());
    std::cout << "Channel " << channel << " voltage set to " << voltage << "V" << std::endl;
}

void RigolDP831::setCurrent(int channel, double current) {
    if (testMode) {
        testState.setCurrent[channel] = current;
        updateTestMeasurements(channel);
        std::cout << "[TEST] Channel " << channel << " current limit set to " << current << "A" << std::endl;
        return;
    }
    
    std::ostringstream cmd;
    cmd << ":SOUR" << channel << ":CURR " << current;
    write(cmd.str());
    std::cout << "Channel " << channel << " current limit set to " << current << "A" << std::endl;
}

double RigolDP831::getVoltage(int channel) {
    if (testMode) {
        updateTestMeasurements(channel);
        return testState.actualVoltage[channel];
    }
    
    std::ostringstream cmd;
    cmd << ":MEAS:VOLT? CH" << channel;
    std::string response = query(cmd.str());
    return std::stod(response);
}

double RigolDP831::getCurrent(int channel) {
    if (testMode) {
        updateTestMeasurements(channel);
        return testState.actualCurrent[channel];
    }
    
    std::ostringstream cmd;
    cmd << ":MEAS:CURR? CH" << channel;
    try {
        std::string response = query(cmd.str());
        return std::stod(response);
    } catch (...) {
        std::ostringstream altCmd;
        altCmd << ":MEAS:ALL? CH" << channel;
        std::string response = query(altCmd.str());
        
        size_t firstComma = response.find(',');
        size_t secondComma = response.find(',', firstComma + 1);
        std::string currentStr = response.substr(firstComma + 1, secondComma - firstComma - 1);
        return std::stod(currentStr);
    }
}

double RigolDP831::getPower(int channel) {
    if (testMode) {
        updateTestMeasurements(channel);
        return testState.actualVoltage[channel] * testState.actualCurrent[channel];
    }
    
    std::ostringstream cmd;
    cmd << ":MEAS:POWE? CH" << channel;
    std::string response = query(cmd.str());
    return std::stod(response);
}

void RigolDP831::enableOutput(int channel, bool state) {
    if (testMode) {
        testState.outputEnabled[channel] = state;
        updateTestMeasurements(channel);
        std::string status = state ? "enabled" : "disabled";
        std::cout << "[TEST] Channel " << channel << " output " << status << std::endl;
        return;
    }
    
    std::ostringstream cmd;
    cmd << ":OUTP CH" << channel << "," << (state ? "ON" : "OFF");
    write(cmd.str());
    std::string status = state ? "enabled" : "disabled";
    std::cout << "Channel " << channel << " output " << status << std::endl;
}

bool RigolDP831::getOutputState(int channel) {
    if (testMode) {
        return testState.outputEnabled[channel];
    }
    
    std::ostringstream cmd;
    cmd << ":OUTP? CH" << channel;
    std::string response = query(cmd.str());
    return response == "ON";
}

RigolDP831::Measurements RigolDP831::getAllMeasurements(int channel) {
    if (testMode) {
        updateTestMeasurements(channel);
        Measurements meas;
        meas.voltage = testState.actualVoltage[channel];
        meas.current = testState.actualCurrent[channel];
        meas.power = testState.actualVoltage[channel] * testState.actualCurrent[channel];
        return meas;
    }
    
    std::ostringstream cmd;
    cmd << ":MEAS:ALL? CH" << channel;
    std::string response = query(cmd.str());
    
    std::vector<double> values;
    std::istringstream ss(response);
    std::string token;
    
    while (std::getline(ss, token, ',')) {
        values.push_back(std::stod(token));
    }
    
    Measurements meas;
    meas.voltage = values[0];
    meas.current = values[1];
    meas.power = values[2];
    
    return meas;
}

double RigolDP831::getSetVoltage(int channel) {
    if (testMode) {
        return testState.setVoltage[channel];
    }
    
    std::ostringstream cmd;
    cmd << ":SOUR" << channel << ":VOLT?";
    std::string response = query(cmd.str());
    return std::stod(response);
}

double RigolDP831::getSetCurrent(int channel) {
    if (testMode) {
        return testState.setCurrent[channel];
    }
    
    std::ostringstream cmd;
    cmd << ":SOUR" << channel << ":CURR?";
    std::string response = query(cmd.str());
    return std::stod(response);
}

void RigolDP831::close() {
    if (testMode) {
        std::cout << "[TEST] Closing connection" << std::endl;
        return;
    }
    
    if (instrument) {
        viClose(instrument);
        instrument = 0;
    }
    if (defaultRM) {
        viClose(defaultRM);
        defaultRM = 0;
    }
}