#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <Arduino.h>

extern bool deviceConnected;

void initBluetooth();
void handleBluetooth();
void notifyMinutesUpdate();
void notifyTelemetryUpdate(); // Sends complete JSON telemetry over BLE

#endif