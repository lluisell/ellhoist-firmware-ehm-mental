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
    uint8_t  controlMode; // 0 = LOW_VOLTAGE, 1 = DIRECT
    int32_t  upperLimit;
    int32_t  lowerLimit;
};

extern SystemStats sysStats;

// --- PERSISTENCE PROTOTYPES ---
void initPersistence();
void updatePeriodicStats();
void incrementBR1Cycles();
void incrementBR2Cycles();
void saveStatsToEEPROM();

#endif // PERSISTENCE_H