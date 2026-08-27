#ifndef POWER_MEASUREMENT_H
#define POWER_MEASUREMENT_H

#include <Arduino.h>

#define PIN_AN_MOTOR_V   4
#define PIN_AN_MOTOR_U   5
#define PIN_AN_MOTOR_W   6

#define TMCS1101_SENSITIVITY_MV_A 100.0f
#define TMCS1101_ZERO_OFFSET_MV   1650.0f

#define AMC0330_REFIN_MV          1650.0f
#define AMC0330_R_HV_OHMS         996000.0f
#define AMC0330_R_IN_OHMS         1300.0f
#define AMC0330_VOLTAGE_SCALE     ((AMC0330_R_HV_OHMS + AMC0330_R_IN_OHMS) / AMC0330_R_IN_OHMS / 1000.0f)

enum SystemPowerMode {
    POWER_MODE_USB_5V = 0,
    POWER_MODE_24V_IDLE,
    POWER_MODE_24V_ACTIVE_FWD,
    POWER_MODE_24V_ACTIVE_REV
};

extern SystemPowerMode currentPowerMode;
extern bool allPhasesPresent;
extern String phaseSequenceStatus;

// AC Line Voltages
extern float vL1L2_RMS;
extern float vL3L2_RMS;
extern float vL1L3_RMS;
extern float phaseFrequencyHz;

// Motor Current Telemetry
extern float motorCurrentU;
extern float motorCurrentV;
extern float motorCurrentW;
extern float motorPower;

void initPowerMeasurement();
void analyzeACPhases();
void readTMCS1101Currents();
void updateAllPowerMeasurements();
String getPowerModeString();

#endif // POWER_MEASUREMENT_H