#include "persistence.h"
#include "positioning.h"
#include "motion_control.h"
#include <Wire.h>

#define ADDR_EEPROM_DATA 0x50
#define EEPROM_STATS_OFFSET 0x0010

SystemStats sysStats = {0, 0, 0, 0, 0, 1.0f, (uint8_t)CTRL_MODE_LOW_VOLTAGE, 10000, 0};

void saveStatsToEEPROM() {
    // Pure read & write: Saves calculated position without altering raw count or offset
    sysStats.savedPosition = getCalculatedPosition();
    sysStats.controlMode = (uint8_t)currentCtrlMode;

    Wire.beginTransmission(ADDR_EEPROM_DATA);
    Wire.write((uint8_t)(EEPROM_STATS_OFFSET >> 8));
    Wire.write((uint8_t)(EEPROM_STATS_OFFSET & 0xFF));
    
    uint8_t* ptr = (uint8_t*)&sysStats;
    for (size_t i = 0; i < sizeof(SystemStats); i++) {
        Wire.write(ptr[i]);
    }
    Wire.endTransmission();
}

void initPersistence() {
    Wire.beginTransmission(ADDR_EEPROM_DATA);
    Wire.write((uint8_t)(EEPROM_STATS_OFFSET >> 8));
    Wire.write((uint8_t)(EEPROM_STATS_OFFSET & 0xFF));
    
    if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)ADDR_EEPROM_DATA, (uint8_t)sizeof(SystemStats)) == sizeof(SystemStats)) {
        uint8_t* ptr = (uint8_t*)&sysStats;
        for (size_t i = 0; i < sizeof(SystemStats); i++) {
            ptr[i] = Wire.read();
        }
    }

    bool needsSave = false;

    if (isnan(sysStats.encoderScale) || sysStats.encoderScale == 0.0f) {
        sysStats.encoderScale = 1.0f;
        needsSave = true;
    }

    if (sysStats.controlMode != (uint8_t)CTRL_MODE_DIRECT && 
        sysStats.controlMode != (uint8_t)CTRL_MODE_LOW_VOLTAGE) {
        
        sysStats.controlMode = (uint8_t)CTRL_MODE_LOW_VOLTAGE;
        needsSave = true;
    }

    if (needsSave) {
        saveStatsToEEPROM();
    }

    currentCtrlMode = (OperationControlMode)sysStats.controlMode;
}

void updatePeriodicStats() {
    static unsigned long lastSecondTick = 0;
    if (millis() - lastSecondTick >= 1000) {
        lastSecondTick = millis();
        sysStats.deviceRuntimeSec++;

        if (getMotionState() == MOTION_FORWARD || getMotionState() == MOTION_REVERSE) {
            sysStats.motorRuntimeSec++;
        }

        saveStatsToEEPROM();
    }
}

void incrementBR1Cycles() {
    sysStats.br1Cycles++;
    saveStatsToEEPROM();
}

void incrementBR2Cycles() {
    sysStats.br2Cycles++;
    saveStatsToEEPROM();
}