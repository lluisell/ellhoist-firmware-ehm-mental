#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <Arduino.h>

// --- MCP23008 BITMASKS ---
#define MASK_FW_CONT  0b00000001 // GP0
#define MASK_REV_CONT 0b00000010 // GP1
#define MASK_BRAKE_1  0b00000100 // GP2
#define MASK_BRAKE_2  0b00001000 // GP3

// --- ENUMS ---
enum MotionDirection {
    MOTION_STOP = 0,
    MOTION_FORWARD,
    MOTION_REVERSE
};

enum BrakeTestMode {
    BRAKE_TEST_NONE = 0,
    BRAKE_TEST_BR1, // Releases BR2 permanently during stops
    BRAKE_TEST_BR2  // Releases BR1 permanently during stops
};

// --- FUNCTION PROTOTYPES ---
void setBrakeTestMode(BrakeTestMode newMode);
BrakeTestMode getBrakeTestMode();

void setMotionState(MotionDirection dir);
MotionDirection getMotionState();

void updateMotionOutputs();

#endif // MOTION_CONTROL_H