#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <Arduino.h>

extern bool deviceConnected;

void initBluetooth();

void handleBluetooth();
void notifyMinutesUpdate();

#endif // BLUETOOTH_H