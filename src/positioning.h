#ifndef POSITIONING_H
#define POSITIONING_H

#include <Arduino.h>

#define PIN_ENC_A 20
#define PIN_ENC_B 21

extern volatile long encoderPosition;
extern volatile unsigned long encoderTotalPulses;
extern volatile int lastDirection;

extern int32_t positionOffset;

void IRAM_ATTR handleEncoderISR();
void initPositioning();

int32_t getRawEncoderCount();
int32_t getCalculatedPosition();
void setEncoderPosition(int32_t newPos);
void setEncoderScale(float scale);
void setUpperLimit(int32_t limit);
void setLowerLimit(int32_t limit);

#endif // POSITIONING_H