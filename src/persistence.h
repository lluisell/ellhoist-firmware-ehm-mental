#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <Arduino.h>

struct SystemStats {
    uint32_t deviceRuntimeSec;
    uint32_t motorRuntimeSec;
    uint32_t br1Cycles;
    uint32_t br2Cycles;
    int32_t  savedPosition;
    float    encoderScale;
    uint8_t  controlMode;
    int32_t  upperLimit;
    int32_t  lowerLimit;
    uint8_t  startupMelody;
    uint8_t  upperLimitMelody;
    uint8_t  lowerLimitMelody;
    char     serialNumber[16]; // Persistent Serial Number
};

extern SystemStats sysStats;
extern String hwID;

const String FIRMWARE_VERSION = "v2.0.0";

void initPersistence();
void updatePeriodicStats();
void incrementBR1Cycles();
void incrementBR2Cycles();
void saveStatsToEEPROM();

void saveWiFiCredentials(const String& ssid, const String& password);
void setSSID(const String& ssid);
void setPass(const String& password);
String getSSID();
String getPass();

#endif // PERSISTENCE_H