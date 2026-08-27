#include "motion_control.h"
#include "power_measurement.h"
#include "persistence.h"
#include "positioning.h"
#include "sd_logger.h"

extern void writeMCP(uint8_t reg, uint8_t value);

static MotionDirection currentMotion = MOTION_STOP;
static BrakeTestMode currentTestMode = BRAKE_TEST_NONE;
OperationControlMode currentCtrlMode = CTRL_MODE_LOW_VOLTAGE;

static uint8_t lastOutputMask = 0x00;

// Target Tracking Variables
static int32_t targetPos = 0;
static bool targetActive = false;

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
            if (currentTestMode == BRAKE_TEST_BR1) outputMask |= MASK_BRAKE_2;
            else if (currentTestMode == BRAKE_TEST_BR2) outputMask |= MASK_BRAKE_1;
            break;
    }

    if (!(lastOutputMask & MASK_BRAKE_1) && (outputMask & MASK_BRAKE_1)) {
        logEventAsync("BRAKE_1_OPENED");
        incrementBR1Cycles();
    } else if ((lastOutputMask & MASK_BRAKE_1) && !(outputMask & MASK_BRAKE_1)) {
        logEventAsync("BRAKE_1_CLOSED");
    }

    if (!(lastOutputMask & MASK_BRAKE_2) && (outputMask & MASK_BRAKE_2)) {
        logEventAsync("BRAKE_2_OPENED");
        incrementBR2Cycles();
    } else if ((lastOutputMask & MASK_BRAKE_2) && !(outputMask & MASK_BRAKE_2)) {
        logEventAsync("BRAKE_2_CLOSED");
    }

    if (!(lastOutputMask & MASK_FW_CONT) && (outputMask & MASK_FW_CONT)) logEventAsync("FW_CONTACTOR_CLOSED");
    else if ((lastOutputMask & MASK_FW_CONT) && !(outputMask & MASK_FW_CONT)) logEventAsync("FW_CONTACTOR_OPENED");

    if (!(lastOutputMask & MASK_REV_CONT) && (outputMask & MASK_REV_CONT)) logEventAsync("REV_CONTACTOR_CLOSED");
    else if ((lastOutputMask & MASK_REV_CONT) && !(outputMask & MASK_REV_CONT)) logEventAsync("REV_CONTACTOR_OPENED");

    lastOutputMask = outputMask;
    writeMCP(0x09, outputMask);
}

void setBrakeTestMode(BrakeTestMode newMode) {
    if (currentTestMode == newMode) return;
    currentMotion = MOTION_STOP;
    targetActive = false;
    writeMCP(0x09, 0x00);
    delay(500);
    currentTestMode = newMode;
    updateMotionOutputs();
}

BrakeTestMode getBrakeTestMode() { return currentTestMode; }

bool setMotionState(MotionDirection dir) {
    int32_t currentPos = getCalculatedPosition();

    // Direct Control Mode Interlock Check
    if (currentCtrlMode == CTRL_MODE_DIRECT && dir == MOTION_FORWARD) {
        if (currentPowerMode != POWER_MODE_24V_ACTIVE_FWD && currentPowerMode != POWER_MODE_24V_ACTIVE_REV) {
            logEventAsync("DIRECT_CTRL_BLOCKED_NO_24V_3PHASE");
            return false;
        }
    }

    // --- LOGIC B: LIMIT PROTECTION INTERLOCKS ---
    if (dir == MOTION_FORWARD && currentPos >= sysStats.upperLimit) {
        logEventAsync("MOTION_BLOCKED_UPPER_LIMIT_EXCEEDED");
        return false;
    }
    if (dir == MOTION_REVERSE && currentPos <= sysStats.lowerLimit) {
        logEventAsync("MOTION_BLOCKED_LOWER_LIMIT_EXCEEDED");
        return false;
    }

    // Manual motion command cancels active target positioning
    if (dir == MOTION_STOP || (currentMotion != dir && !targetActive)) {
        targetActive = false;
    }

    currentMotion = dir;
    updateMotionOutputs();
    return true;
}

MotionDirection getMotionState() { return currentMotion; }

void setOperationControlMode(OperationControlMode mode) {
    currentCtrlMode = mode;
    logEventAsync(mode == CTRL_MODE_DIRECT ? "CTRL_MODE_DIRECT_ENABLED" : "CTRL_MODE_LOW_VOLTAGE_ENABLED");
}

// --- LOGIC A: RUN TO POSITION TARGET FUNCTION ---
bool runToTargetPosition(int32_t target) {
    int32_t currentPos = getCalculatedPosition();
    if (target == currentPos) {
        setMotionState(MOTION_STOP);
        return true;
    }

    targetPos = target;
    targetActive = true;

    if (target > currentPos) {
        logEventAsync("RUN_TO_TARGET_STARTED_FORWARD");
        return setMotionState(MOTION_FORWARD);
    } else {
        logEventAsync("RUN_TO_TARGET_STARTED_REVERSE");
        return setMotionState(MOTION_REVERSE);
    }
}

// --- CONTINUOUS LIMIT & TARGET EVALUATION (Called in main loop) ---
void processMotionLogic() {
    if (currentMotion == MOTION_STOP) return;

    int32_t currentPos = getCalculatedPosition();

    // 1. Limit Check (Stops motion if upper/lower limit is hit or passed)
    if (currentMotion == MOTION_FORWARD && currentPos >= sysStats.upperLimit) {
        setMotionState(MOTION_STOP);
        targetActive = false;
        logEventAsync("AUTO_STOPPED_UPPER_LIMIT_REACHED");
        return;
    }
    if (currentMotion == MOTION_REVERSE && currentPos <= sysStats.lowerLimit) {
        setMotionState(MOTION_STOP);
        targetActive = false;
        logEventAsync("AUTO_STOPPED_LOWER_LIMIT_REACHED");
        return;
    }

    // 2. Target Position Check
    if (targetActive) {
        if (currentMotion == MOTION_FORWARD && currentPos >= targetPos) {
            setMotionState(MOTION_STOP);
            targetActive = false;
            logEventAsync("TARGET_POSITION_REACHED");
        } else if (currentMotion == MOTION_REVERSE && currentPos <= targetPos) {
            setMotionState(MOTION_STOP);
            targetActive = false;
            logEventAsync("TARGET_POSITION_REACHED");
        }
    }
}

int32_t getTargetPosition() { return targetPos; }
bool isTargetActive() { return targetActive; }