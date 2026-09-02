#include "persistence.h"
#include "positioning.h"
#include "motion_control.h"
#include "melodies.h"
#include <Wire.h>
#include <Preferences.h>

#define ADDR_EERAM_DATA 0x50
#define ADDR_EERAM_CTRL 0x18
#define EEPROM_STATS_OFFSET 0x0010



SystemStats sysStats = {
    0, 0, 0, 0, 0, 1.0f, (uint8_t)CTRL_MODE_LOW_VOLTAGE, 10000, 0,
    (uint8_t)MELODY_SMOKE_ON_WATER,
    (uint8_t)MELODY_DUAL_ALERT,
    (uint8_t)MELODY_SEVEN_NATION_ARMY,
    "000000" // Default Serial Number
};
String hwID = "";

static void enableEERAMAutoStore() {
    Wire.beginTransmission(ADDR_EERAM_CTRL);
    Wire.write(0x00);
    if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)ADDR_EERAM_CTRL, (uint8_t)1) == 1) {
        uint8_t status = Wire.read();
        if (!(status & 0x02)) {
            status |= 0x02;
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

#include <Arduino.h>
#include "esp_mac.h"

String getESP32C6UniqueUUID() {
    uint64_t chipId = 0;
    // Reads full 64-bit internal chip/MAC identifier from eFuse
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    
    for (int i = 0; i < 6; i++) {
        chipId |= ((uint64_t)mac[i] << (8 * i));
    }

    // Custom non-linear mix to generate a 16-character pseudo-UUID
    uint32_t highHash = (uint32_t)(chipId >> 16) ^ 0xA5A5A5A5;
    uint32_t lowHash  = (uint32_t)(chipId & 0xFFFFFFFF) ^ 0x5A5A5A5A;

    char uuidBuf[20];
    snprintf(uuidBuf, sizeof(uuidBuf), "%08X%08X", highHash, lowHash);

    return String(uuidBuf); // Output example: "8F4A12BC99D0014E"
}

void initPersistence() {
    hwID = getESP32C6UniqueUUID();
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

    if (sysStats.startupMelody >= MELODY_COUNT) sysStats.startupMelody = (uint8_t)MELODY_SMOKE_ON_WATER, needsSave = true;
    if (sysStats.upperLimitMelody >= MELODY_COUNT) sysStats.upperLimitMelody = (uint8_t)MELODY_DUAL_ALERT, needsSave = true;
    if (sysStats.lowerLimitMelody >= MELODY_COUNT) sysStats.lowerLimitMelody = (uint8_t)MELODY_SEVEN_NATION_ARMY, needsSave = true;

    if (strlen(sysStats.serialNumber) == 0) {
        snprintf(sysStats.serialNumber, sizeof(sysStats.serialNumber), "000000");
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




static Preferences prefs;


void saveWiFiCredentials(const String& ssid, const String& password) {
    prefs.begin("wifi_config", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.end();
}

void setSSID(const String& ssid) {
    prefs.begin("wifi_config", false);
    prefs.putString("ssid", ssid);
    prefs.end();
}

void setPass(const String& password){
    prefs.begin("wifi_config", false);
    prefs.putString("pass", password);
    prefs.end();
}

String getSSID() {
    prefs.begin("wifi_config", true);
    return prefs.getString("ssid", "");
    prefs.end();
}
String getPass() {
    prefs.begin("wifi_config", true);
    return prefs.getString("pass", "");
    prefs.end();
}