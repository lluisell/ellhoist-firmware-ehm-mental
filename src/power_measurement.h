#ifndef POWER_MEASUREMENT_H
#define POWER_MEASUREMENT_H

#include <Arduino.h>

// --- SENSOR PARAMETERS ---
#define TMCS1101_SENSITIVITY_MV_A 100.0f
#define TMCS1101_ZERO_OFFSET_MV   1650.0f

#define AMC0330_REFIN_MV          1650.0f
#define AMC0330_R_HV_OHMS         996000.0f  // 4x 249k
#define AMC0330_R_IN_OHMS         1300.0f    // 1.3k
#define AMC0330_VOLTAGE_SCALE     ((AMC0330_R_HV_OHMS + AMC0330_R_IN_OHMS) / AMC0330_R_IN_OHMS / 1000.0f) // ~0.767154

// --- EARLY STARTUP DIAGNOSTIC STATUS VARIABLES ---
extern bool allPhasesPresent;         // True if L1-L2 and L3-L2 are above AC voltage threshold
extern String phaseSequenceStatus;    // "NORMAL (L1-L2-L3)", "REVERSED (L3-L2-L1)", "MISSING_PHASE", or "NO_SIGNAL"

// --- POWER & AC TELEMETRY VARIABLES ---
extern float vL1L2_RMS;        // Line Volts RMS (L1-L2)
extern float vL3L2_RMS;        // Line Volts RMS (L3-L2)
extern float vL1L3_RMS;        // Derived Line Volts RMS (L1-L3)
extern float phaseFrequencyHz; // Line Frequency in Hz

extern float motorCurrentU;    // Amperes (GPIO 5)
extern float motorCurrentV;    // Amperes (GPIO 4)
extern float motorCurrentW;    // Amperes (GPIO 6)

extern float inaBusVoltage;    // Volts DC
extern float inaCurrent;       // Milliamperes DC
extern float inaPower;         // Watts

// --- FUNCTION PROTOTYPES ---
void initPowerMeasurement();
void analyzeACPhases();
void readTMCS1101Currents();
void readINA226Power();
void updateAllPowerMeasurements();

#endif // POWER_MEASUREMENT_H