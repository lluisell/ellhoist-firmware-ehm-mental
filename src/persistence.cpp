#include "persistence.h"
#include "positioning.h"
#include "motion_control.h"
#include "melodies.h"
#include <Wire.h>

#define ADDR_EERAM_DATA 0x50
#define ADDR_EERAM_CTRL 0x18
#define EEPROM_STATS_OFFSET 0x0010

SystemStats sysStats = {
    0, 0, 0, 0, 0, 1.0f, (uint8_t)CTRL_MODE_LOW_VOLTAGE, 10000, 0,
    (uint8_t)MELODY_SMOKE_ON_WATER,    // Default Startup Melody
    (uint8_t)MELODY_DUAL_ALERT,        // Default Upper Limit Melody
    (uint8_t)MELODY_SEVEN_NATION_ARMY  // Default Lower Limit Melody
};

static void enableEERAMAutoStore() {
    Wire.beginTransmission(ADDR_EERAM_CTRL);
    Wire.write(0x00);
    if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)ADDR_EERAM_CTRL, (uint8_t)1) == 1) {
        uint8_t status = Wire.read();
        if (!(status & 0x02)) {
            status |= 0x02; // Set ASE bit 1 to enable hardware Auto-Store
            Wire.beginTransmission(ADDR_EERAM_CTRL);
            Wire.write(0x00);
            Wire.write(status);
            Wire.endTransmission();
            delay(5);
        }
    }
}

void saveStatsToEEPROM() {
    sysStats.savedPosition = getCalculatedPosition();
    sysStats.controlMode = (uint8_t)currentCtrlMode;

    Wire.beginTransmission(ADDR_EERAM_DATA);
    Wire.write((uint8_t)(EEPROM_STATS_OFFSET >> 8));
    Wire.write((uint8_t)(EEPROM_STATS_OFFSET & 0xFF));

    uint8_t* ptr = (uint8_t*)&sysStats;
    for (size_t i = 0; i < sizeof(SystemStats); i++) {
        Wire.write(ptr[i]);
    }
    Wire.endTransmission();
}

void initPersistence() {
    enableEERAMAutoStore();

    Wire.beginTransmission(ADDR_EERAM_DATA);
    Wire.write((uint8_t)(EEPROM_STATS_OFFSET >> 8));
    Wire.write((uint8_t)(EEPROM_STATS_OFFSET & 0xFF));

    if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)ADDR_EERAM_DATA, (uint8_t)sizeof(SystemStats)) == sizeof(SystemStats)) {
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

    if (sysStats.startupMelody >= MELODY_COUNT) {
        sysStats.startupMelody = (uint8_t)MELODY_SMOKE_ON_WATER;
        needsSave = true;
    }
    if (sysStats.upperLimitMelody >= MELODY_COUNT) {
        sysStats.upperLimitMelody = (uint8_t)MELODY_DUAL_ALERT;
        needsSave = true;
    }
    if (sysStats.lowerLimitMelody >= MELODY_COUNT) {
        sysStats.lowerLimitMelody = (uint8_t)MELODY_SEVEN_NATION_ARMY;
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