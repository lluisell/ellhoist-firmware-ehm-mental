#include "telemetry_helper.h"
#include "power_measurement.h"
#include "persistence.h"
#include "positioning.h"
#include "sensors.h"
#include <RTClib.h>     
#include "motion_control.h"

long int lastMillis = 0;
String prevJson = "";

RTC_DS3231 rtc;

String buildTelemetryJSON() {
    if ((millis() - lastMillis >= 1000) || (prevJson == "")) {
        lastMillis = millis();

        updateAllPowerMeasurements();
        
        float temp = 0, hum = 0, pres = 0;
        readWeatherSensor(temp, hum, pres);

        int16_t ax = 0, ay = 0, az = 0;
        readAccelerometer(ax, ay, az);

        DateTime now = rtc.now();

        char rtcBuf[25];
        snprintf(rtcBuf, sizeof(rtcBuf), "%04d-%02d-%02d %02d:%02d:%02d",
                now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

        String json = "{";
        json += "\"hwId\":\"" + String(hwID) + "\",";
        json += "\"devSerial\":\"" + String(sysStats.serialNumber) + "\",";
        json += "\"rtc\":\"" + String(rtcBuf) + "\",";
        json += "\"pwrStr\":\"" + getPowerModeString() + "\",";
        json += "\"ctrlMode\":\"" + String(currentCtrlMode == CTRL_MODE_DIRECT ? "DIRECT" : "LOW_VOLTAGE") + "\",";
        json += "\"temp\":" + String(temp, 2) + ",";
        json += "\"hum\":" + String(hum, 2) + ",";
        json += "\"pres\":" + String(pres, 1) + ",";
        json += "\"ax_mms2\":" + String(accelX_mms2, 1) + ",";
        json += "\"ay_mms2\":" + String(accelY_mms2, 1) + ",";
        json += "\"az_mms2\":" + String(accelZ_mms2, 1) + ",";
        json += "\"pitch\":" + String(pitchDeg) + ",";
        json += "\"tilt\":" + String(tiltDeg) + ",";
        json += "\"curU\":" + String(motorCurrentU, 2) + ",";
        json += "\"curV\":" + String(motorCurrentV, 2) + ",";
        json += "\"curW\":" + String(motorCurrentW, 2) + ",";
        json += "\"vL1L2\":" + String(vL1L2_RMS, 2) + ",";
        json += "\"vL3L2\":" + String(vL3L2_RMS, 2) + ",";
        json += "\"vL1L3\":" + String(vL1L3_RMS, 2) + ",";
        json += "\"freq\":" + String(phaseFrequencyHz, 2) + ",";
        json += "\"motPwr\":" + String(motorPower, 2) + ",";
        json += "\"dcV\":" + String(inaBusVoltage, 2) + ",";
        json += "\"dcI\":" + String(inaCurrent, 2) + ",";
        json += "\"dcP\":" + String(inaPower, 2) + ",";
        json += "\"devRun\":" + String(sysStats.deviceRuntimeSec) + ",";
        json += "\"motRun\":" + String(sysStats.motorRuntimeSec) + ",";
        json += "\"br1C\":" + String(sysStats.br1Cycles) + ",";
        json += "\"br2C\":" + String(sysStats.br2Cycles) + ",";
        json += "\"encRaw\":" + String(getRawEncoderCount()) + ",";
        json += "\"encPos\":" + String(getCalculatedPosition()) + ",";
        json += "\"encScale\":" + String(sysStats.encoderScale, 4) + ",";
        json += "\"upperLim\":" + String(sysStats.upperLimit) + ",";
        json += "\"lowerLim\":" + String(sysStats.lowerLimit);
        json += "}";

        prevJson = json;
    }
    return prevJson;
}