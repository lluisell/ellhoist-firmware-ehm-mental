#include "power_measurement.h"
#include <Wire.h>
#include <math.h>

#define PIN_AN_L1_L2_A   0 // Phase L1-L2
#define PIN_AN_L1_L2_B   1 // Phase L3-L2
#define PIN_AN_MOTOR_V   4
#define PIN_AN_MOTOR_U   5
#define PIN_AN_MOTOR_W   6
#define ADDR_INA226      0x40

// Single-ended AMC0330 output & board loading hardware scaling factor (380V / 140V)
#define VOLTAGE_CALIBRATION_FACTOR 2.714f

// Early Diagnostic Status Variables
bool allPhasesPresent = false;
String phaseSequenceStatus = "UNINITIALIZED";

// AC Line Telemetry
float vL1L2_RMS = 0.0f;
float vL3L2_RMS = 0.0f;
float vL1L3_RMS = 0.0f;
float phaseFrequencyHz = 0.0f;

// Motor Phase Currents
float motorCurrentU = 0.0f;
float motorCurrentV = 0.0f;
float motorCurrentW = 0.0f;

// INA226 Telemetry
float inaBusVoltage = 0.0f;
float inaCurrent = 0.0f;
float inaPower = 0.0f;

void initPowerMeasurement() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db); // Full 0 - 3.3V range
}

void analyzeACPhases() {
    const int NUM_SAMPLES = 500; // 500 samples (~60ms window = 3 full cycles at 50Hz)
    
    static float v12_mv[500];
    static float v32_mv[500];
    static uint32_t t_us[500];

    // 1. Timestamped Calibrated ADC Buffer Capture
    for (int i = 0; i < NUM_SAMPLES; i++) {
        t_us[i] = micros();
        v12_mv[i] = analogReadMilliVolts(PIN_AN_L1_L2_A);
        v32_mv[i] = analogReadMilliVolts(PIN_AN_L1_L2_B);
        delayMicroseconds(80); // ~120us total loop step (~8.3kHz sampling)
    }

    // 2. Dynamic DC Midpoint Offset (Averages out V_REFIN ~1650mV over 60ms)
    double sum12 = 0, sum32 = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum12 += v12_mv[i];
        sum32 += v32_mv[i];
    }
    float dc12 = sum12 / NUM_SAMPLES;
    float dc32 = sum32 / NUM_SAMPLES;

    // 3. True RMS Calculation
    static float v12_volts[500];
    static float v32_volts[500];
    double sumSqL1L2 = 0;
    double sumSqL3L2 = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        float ac12_mv = v12_mv[i] - dc12;
        float ac32_mv = v32_mv[i] - dc32;

        // Scale output mV up to high-voltage AC Line Volts with hardware factor
        v12_volts[i] = ac12_mv * AMC0330_VOLTAGE_SCALE * VOLTAGE_CALIBRATION_FACTOR;
        v32_volts[i] = ac32_mv * AMC0330_VOLTAGE_SCALE * VOLTAGE_CALIBRATION_FACTOR;

        sumSqL1L2 += (v12_volts[i] * v12_volts[i]);
        sumSqL3L2 += (v32_volts[i] * v32_volts[i]);
    }

    vL1L2_RMS = sqrt(sumSqL1L2 / NUM_SAMPLES);
    vL3L2_RMS = sqrt(sumSqL3L2 / NUM_SAMPLES);
    
    // Derived Line-to-Line V_L1-L3 vector calculation
    vL1L3_RMS = sqrt((vL1L2_RMS * vL1L2_RMS) + (vL3L2_RMS * vL3L2_RMS) - (vL1L2_RMS * vL3L2_RMS));

    // Check Phase Presence Threshold (> 50.0V AC RMS)
    if (vL1L2_RMS < 50.0f || vL3L2_RMS < 50.0f) {
        allPhasesPresent = false;
        phaseSequenceStatus = "MISSING_PHASE";
        phaseFrequencyHz = 0.0f;
        return;
    }
    allPhasesPresent = true;

    // 4. Robust Hysteresis State-Machine Zero Crossing & Frequency Analysis
    int idx_v12_first = -1;
    int idx_v12_second = -1;
    bool armed = false;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (v12_volts[i] < -20.0f) {
            armed = true; // Signal went sufficiently negative
        } else if (armed && v12_volts[i] > 20.0f) {
            armed = false; // Zero crossing rising transition detected
            if (idx_v12_first == -1) {
                idx_v12_first = i;
            } else if (idx_v12_second == -1) {
                // Enforce min 12ms gap (12,000us) between 50Hz zero crossings
                if ((t_us[i] - t_us[idx_v12_first]) > 12000) {
                    idx_v12_second = i;
                    break;
                }
            }
        }
    }

    if (idx_v12_first == -1 || idx_v12_second == -1) {
        phaseSequenceStatus = "SIGNAL_UNSTABLE";
        phaseFrequencyHz = 0.0f;
        return;
    }

    // Calculate Line Frequency using hardware timestamps
    uint32_t period_us = t_us[idx_v12_second] - t_us[idx_v12_first];
    phaseFrequencyHz = 1000000.0f / (float)period_us;

    // Search for next rising zero-crossing on V32 after idx_v12_first
    int idx_v32_next = -1;
    armed = false;

    for (int i = idx_v12_first; i < NUM_SAMPLES; i++) {
        if (t_us[i] - t_us[idx_v12_first] > period_us + 4000) break; // Search within 1 AC cycle
        if (v32_volts[i] < -20.0f) {
            armed = true;
        } else if (armed && v32_volts[i] > 20.0f) {
            idx_v32_next = i;
            break;
        }
    }

    if (idx_v32_next == -1) {
        phaseSequenceStatus = "SIGNAL_UNSTABLE";
        return;
    }

    // Calculate Relative Phase Displacement Angle
    uint32_t delta_us = t_us[idx_v32_next] - t_us[idx_v12_first];
    float phaseAngleDeg = ((float)delta_us / (float)period_us) * 360.0f;

    // Sequence Classification
    if (phaseAngleDeg >= 180.0f && phaseAngleDeg <= 340.0f) {
        phaseSequenceStatus = "NORMAL (L1-L2-L3)";
    } else if (phaseAngleDeg >= 20.0f && phaseAngleDeg <= 160.0f) {
        phaseSequenceStatus = "REVERSED (L3-L2-L1)";
    } else {
        phaseSequenceStatus = "PHASE_SHIFT_ERROR";
    }
}

void readTMCS1101Currents() {
    float voltU_mV = analogReadMilliVolts(PIN_AN_MOTOR_U);
    float voltV_mV = analogReadMilliVolts(PIN_AN_MOTOR_V);
    float voltW_mV = analogReadMilliVolts(PIN_AN_MOTOR_W);

    motorCurrentU = (voltU_mV - TMCS1101_ZERO_OFFSET_MV) / TMCS1101_SENSITIVITY_MV_A;
    motorCurrentV = (voltV_mV - TMCS1101_ZERO_OFFSET_MV) / TMCS1101_SENSITIVITY_MV_A;
    motorCurrentW = (voltW_mV - TMCS1101_ZERO_OFFSET_MV) / TMCS1101_SENSITIVITY_MV_A;
}

void readINA226Power() {
    inaBusVoltage = 0.0f;
    inaCurrent = 0.0f;

    Wire.beginTransmission(ADDR_INA226);
    Wire.write(0x02);
    if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)ADDR_INA226, (uint8_t)2) == 2) {
        uint16_t rawBus = (Wire.read() << 8) | Wire.read();
        inaBusVoltage = rawBus * 0.00125f;
    }

    Wire.beginTransmission(ADDR_INA226);
    Wire.write(0x01);
    if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)ADDR_INA226, (uint8_t)2) == 2) {
        int16_t rawShunt = (Wire.read() << 8) | Wire.read();
        float shuntmV = rawShunt * 0.0025f;
        inaCurrent = shuntmV / 0.1f; 
    }
    
    inaPower = inaBusVoltage * (inaCurrent / 1000.0f);
}

void updateAllPowerMeasurements() {
    analyzeACPhases();
    readTMCS1101Currents();
    readINA226Power();
}