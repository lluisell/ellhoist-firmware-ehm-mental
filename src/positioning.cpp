#include "positioning.h"
#include "persistence.h"

volatile long encoderPosition = 0;
volatile unsigned long encoderTotalPulses = 0;
volatile int lastDirection = 0;

int32_t positionOffset = 0;

void IRAM_ATTR handleEncoderISR() {
    static uint8_t oldAB = 0;
    
    // Read current pin states (Bit 1 = A, Bit 0 = B)
    uint8_t newAB = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
    uint8_t index = (oldAB << 2) | newAB;
    
    // 4x Quadrature State Machine Lookup Table
    static const int8_t encoderLUT[] = {
        0,  1, -1,  0,
       -1,  0,  0,  1,
        1,  0,  0, -1,
        0, -1,  1,  0
    };

    int8_t step = encoderLUT[index];
    
    if (step != 0) {
        encoderPosition += step;
        lastDirection = step;
        encoderTotalPulses++;
    }

    oldAB = newAB;
}

void initPositioning() {
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), handleEncoderISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), handleEncoderISR, CHANGE);

    // 1. Zero raw counts on boot
    encoderPosition = 0;
    
    // 2. Load persistent EEPROM position into offset
    positionOffset = sysStats.savedPosition;
}

int32_t getRawEncoderCount() {
    return (int32_t)encoderPosition;
}

// Position Formula = Offset + (Raw Counts / Counts_Per_mm)
int32_t getCalculatedPosition() {
    if (sysStats.encoderScale == 0.0f) return positionOffset;
    return positionOffset + (int32_t)((float)encoderPosition / sysStats.encoderScale);
}

// Explicit Calibration/New Entry: Zero raw counts & set new offset
void setEncoderPosition(int32_t newPos) {
    noInterrupts();
    encoderPosition = 0;
    positionOffset = newPos;
    interrupts();

    saveStatsToEEPROM();
}

void setEncoderScale(float scale) {
    if (scale != 0.0f) {
        sysStats.encoderScale = scale;
        saveStatsToEEPROM();
    }
}

void setUpperLimit(int32_t limit) {
    sysStats.upperLimit = limit;
    saveStatsToEEPROM();
}

void setLowerLimit(int32_t limit) {
    sysStats.lowerLimit = limit;
    saveStatsToEEPROM();
}