#include "power_measurement.h"
#include "sensors.h"
#include <Wire.h>
#include <math.h>

#define PIN_AN_L1_L2_A   0
#define PIN_AN_L1_L2_B   1
#define VOLTAGE_CALIBRATION_FACTOR 2.714f

SystemPowerMode currentPowerMode = POWER_MODE_USB_5V;
bool allPhasesPresent = false;
String phaseSequenceStatus = "UNINITIALIZED";

float vL1L2_RMS = 0.0f, vL3L2_RMS = 0.0f, vL1L3_RMS = 0.0f, phaseFrequencyHz = 0.0f;
float motorCurrentU = 0.0f, motorCurrentV = 0.0f, motorCurrentW = 0.0f, motorPower = 0.0f;

void initPowerMeasurement() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
}

void analyzeACPhases() {
    const int NUM_SAMPLES = 500;
    static float v12_mv[500], v32_mv[500];
    static uint32_t t_us[500];

    for (int i = 0; i < NUM_SAMPLES; i++) {
        t_us[i] = micros();
        v12_mv[i] = analogReadMilliVolts(PIN_AN_L1_L2_A);
        v32_mv[i] = analogReadMilliVolts(PIN_AN_L1_L2_B);
        delayMicroseconds(80);
    }

    double sum12 = 0, sum32 = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        sum12 += v12_mv[i];
        sum32 += v32_mv[i];
    }
    float dc12 = sum12 / NUM_SAMPLES;
    float dc32 = sum32 / NUM_SAMPLES;

    static float v12_volts[500], v32_volts[500];
    double sumSqL1L2 = 0, sumSqL3L2 = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        float ac12_mv = v12_mv[i] - dc12;
        float ac32_mv = v32_mv[i] - dc32;
        v12_volts[i] = ac12_mv * AMC0330_VOLTAGE_SCALE * VOLTAGE_CALIBRATION_FACTOR;
        v32_volts[i] = ac32_mv * AMC0330_VOLTAGE_SCALE * VOLTAGE_CALIBRATION_FACTOR;
        sumSqL1L2 += (v12_volts[i] * v12_volts[i]);
        sumSqL3L2 += (v32_volts[i] * v32_volts[i]);
    }

    vL1L2_RMS = sqrt(sumSqL1L2 / NUM_SAMPLES);
    vL3L2_RMS = sqrt(sumSqL3L2 / NUM_SAMPLES);
    vL1L3_RMS = sqrt((vL1L2_RMS * vL1L2_RMS) + (vL3L2_RMS * vL3L2_RMS) - (vL1L2_RMS * vL3L2_RMS));

    if (vL1L2_RMS < 50.0f || vL3L2_RMS < 50.0f) {
        allPhasesPresent = false;
        phaseSequenceStatus = "MISSING_PHASE";
        phaseFrequencyHz = 0.0f;
        return;
    }
    allPhasesPresent = true;

    int idx_v12_first = -1, idx_v12_second = -1;
    bool armed = false;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (v12_volts[i] < -20.0f) armed = true;
        else if (armed && v12_volts[i] > 20.0f) {
            armed = false;
            if (idx_v12_first == -1) idx_v12_first = i;
            else if (idx_v12_second == -1 && (t_us[i] - t_us[idx_v12_first]) > 12000) {
                idx_v12_second = i;
                break;
            }
        }
    }

    if (idx_v12_first == -1 || idx_v12_second == -1) {
        phaseSequenceStatus = "SIGNAL_UNSTABLE";
        phaseFrequencyHz = 0.0f;
        return;
    }

    uint32_t period_us = t_us[idx_v12_second] - t_us[idx_v12_first];
    phaseFrequencyHz = 1000000.0f / (float)period_us;

    int idx_v32_next = -1;
    armed = false;
    for (int i = idx_v12_first; i < NUM_SAMPLES; i++) {
        if (t_us[i] - t_us[idx_v12_first] > period_us + 4000) break;
        if (v32_volts[i] < -20.0f) armed = true;
        else if (armed && v32_volts[i] > 20.0f) {
            idx_v32_next = i;
            break;
        }
    }

    if (idx_v32_next == -1) {
        phaseSequenceStatus = "SIGNAL_UNSTABLE";
        return;
    }

    uint32_t delta_us = t_us[idx_v32_next] - t_us[idx_v12_first];
    float phaseAngleDeg = ((float)delta_us / (float)period_us) * 360.0f;

    if (phaseAngleDeg >= 180.0f && phaseAngleDeg <= 340.0f) phaseSequenceStatus = "NORMAL (L1-L2-L3)";
    else if (phaseAngleDeg >= 20.0f && phaseAngleDeg <= 160.0f) phaseSequenceStatus = "REVERSED (L3-L2-L1)";
    else phaseSequenceStatus = "PHASE_SHIFT_ERROR";
}

void readTMCS1101Currents() {
    const int SAMPLES = 200; // 20ms sampling window (1 full 50Hz AC cycle)
    static float u_mv[200], v_mv[200], w_mv[200];

    for (int i = 0; i < SAMPLES; i++) {
        u_mv[i] = analogReadMilliVolts(PIN_AN_MOTOR_U);
        v_mv[i] = analogReadMilliVolts(PIN_AN_MOTOR_V);
        w_mv[i] = analogReadMilliVolts(PIN_AN_MOTOR_W);
        delayMicroseconds(100);
    }

    double sumU = 0, sumV = 0, sumW = 0;
    for (int i = 0; i < SAMPLES; i++) {
        sumU += u_mv[i];
        sumV += v_mv[i];
        sumW += w_mv[i];
    }
    float offsetU = sumU / SAMPLES;
    float offsetV = sumV / SAMPLES;
    float offsetW = sumW / SAMPLES;

    double sumSqU = 0, sumSqV = 0, sumSqW = 0;
    for (int i = 0; i < SAMPLES; i++) {
        float iU = (u_mv[i] - offsetU) / TMCS1101_SENSITIVITY_MV_A;
        float iV = (v_mv[i] - offsetV) / TMCS1101_SENSITIVITY_MV_A;
        float iW = (w_mv[i] - offsetW) / TMCS1101_SENSITIVITY_MV_A;

        sumSqU += (iU * iU);
        sumSqV += (iV * iV);
        sumSqW += (iW * iW);
    }

    motorCurrentU = sqrt(sumSqU / SAMPLES);
    motorCurrentV = sqrt(sumSqV / SAMPLES);
    motorCurrentW = sqrt(sumSqW / SAMPLES);

    float avgLineVoltage = (vL1L2_RMS + vL3L2_RMS + vL1L3_RMS) / 3.0f;
    float avgMotorCurrent = (motorCurrentU + motorCurrentV + motorCurrentW) / 3.0f;

    if (avgMotorCurrent < 0.15f) {
        motorPower = 0.0f;
    } else {
        motorPower = 1.73205f * avgLineVoltage * avgMotorCurrent;
    }
}

void updateAllPowerMeasurements() {
    readINA226Power(); // Uses readINA226Power() from sensors.h/cpp
    analyzeACPhases();
    readTMCS1101Currents();

    if (inaBusVoltage < 20.0f) {
        currentPowerMode = POWER_MODE_USB_5V;
    } else if (!allPhasesPresent) {
        currentPowerMode = POWER_MODE_24V_IDLE;
    } else if (phaseSequenceStatus.indexOf("NORMAL") >= 0) {
        currentPowerMode = POWER_MODE_24V_ACTIVE_FWD;
    } else {
        currentPowerMode = POWER_MODE_24V_ACTIVE_REV;
    }
}

String getPowerModeString() {
    switch (currentPowerMode) {
        case POWER_MODE_USB_5V: return "USB 5V Rail Power";
        case POWER_MODE_24V_IDLE: return "24V Present (Phase Missing)";
        case POWER_MODE_24V_ACTIVE_FWD: return "Phase Normal (L1-L2-L3)";
        case POWER_MODE_24V_ACTIVE_REV: return "Phase Reversed (L3-L2-L1)";
        default: return "UNKNOWN";
    }
}