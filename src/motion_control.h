#ifndef MOTION_CONTROL_H
#define MOTION_CONTROL_H

#include <Arduino.h>

#define MASK_FW_CONT  0b00000001
#define MASK_REV_CONT 0b00000010
#define MASK_BRAKE_1  0b00000100
#define MASK_BRAKE_2  0b00001000

enum MotionDirection {
    MOTION_STOP = 0,
    MOTION_FORWARD,
    MOTION_REVERSE
};

enum BrakeTestMode {
    BRAKE_TEST_NONE = 0,
    BRAKE_TEST_BR1,
    BRAKE_TEST_BR2
};

enum OperationControlMode {
    CTRL_MODE_LOW_VOLTAGE = 0,
    CTRL_MODE_DIRECT
};

extern OperationControlMode currentCtrlMode;

void setBrakeTestMode(BrakeTestMode newMode);
BrakeTestMode getBrakeTestMode();

bool setMotionState(MotionDirection dir);
MotionDirection getMotionState();

void setOperationControlMode(OperationControlMode mode);
void updateMotionOutputs();

bool runToTargetPosition(int32_t target);
void processMotionLogic();
int32_t getTargetPosition();
bool isTargetActive();

#endif // MOTION_CONTROL_H