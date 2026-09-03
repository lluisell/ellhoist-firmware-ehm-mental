#ifndef TELEMETRY_HELPER_H
#define TELEMETRY_HELPER_H

#include <Arduino.h>
#include <RTClib.h>

String buildTelemetryJSON();

extern RTC_DS3231 rtc;

#endif