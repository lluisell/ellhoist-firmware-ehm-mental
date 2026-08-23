#include "motion_control.h"

// Reference external MCP write helper declared in the main sketch
extern void writeMCP(uint8_t reg, uint8_t value);

static MotionDirection currentMotion = MOTION_STOP;
static BrakeTestMode currentTestMode = BRAKE_TEST_NONE;

void updateMotionOutputs() {
    uint8_t outputMask = 0x00;

    switch (currentMotion) {
        case MOTION_FORWARD:
            outputMask |= (MASK_FW_CONT | MASK_BRAKE_1 | MASK_BRAKE_2);
            break;

        case MOTION_REVERSE:
            outputMask |= (MASK_REV_CONT | MASK_BRAKE_1 | MASK_BRAKE_2);
            break;

        case MOTION_STOP:
        default:
            // Contactor outputs stay off (0). 
            // Keep specific brake released if active test mode demands it.
            if (currentTestMode == BRAKE_TEST_BR1) {
                outputMask |= MASK_BRAKE_2; // Hold BR2 released
            } else if (currentTestMode == BRAKE_TEST_BR2) {
                outputMask |= MASK_BRAKE_1; // Hold BR1 released
            }
            break;
    }

    writeMCP(0x09, outputMask);
}

void setBrakeTestMode(BrakeTestMode newMode) {
    if (currentTestMode == newMode) return;

    // 500ms safety interlock when switching modes or toggling tests
    // Safely apply all brakes and turn off contactors
    currentMotion = MOTION_STOP;
    writeMCP(0x09, 0x00);
    delay(500);

    currentTestMode = newMode;
    updateMotionOutputs();
}

BrakeTestMode getBrakeTestMode() {
    return currentTestMode;
}

void setMotionState(MotionDirection dir) {
    currentMotion = dir;
    updateMotionOutputs();
}

MotionDirection getMotionState() {
    return currentMotion;
}