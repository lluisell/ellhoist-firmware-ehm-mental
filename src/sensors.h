#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define ADDR_LIS2DH12    0x19
#define ADDR_BME280      0x77
#define ADDR_INA226      0x40

// Sensor Hardware Objects
extern Adafruit_BME280 bme;

// INA226 Telemetry
extern float inaBusVoltage;
extern float inaCurrent;
extern float inaPower;

// Environmental Telemetry
extern float bmeTemperature;
extern float bmeHumidity;
extern float bmePressure;

// Accelerometer Telemetry
extern int16_t accelX;
extern int16_t accelY;
extern int16_t accelZ;

// Calculated Acceleration Output (mm/s²)
extern float accelX_mms2;
extern float accelY_mms2;
extern float accelZ_mms2;

// Calculated Inclination Output (Degrees, Integer)
extern int pitchDeg; // Pitch around Y-axis
extern int tiltDeg;  // Tilt around Z-axis

// Initialization & Reader Functions
bool initSensors();
bool initAccelerometer();
bool initBME280();

void readAccelerometer(int16_t &x, int16_t &y, int16_t &z);
void readWeatherSensor(float &temp, float &hum, float &pres);
void readINA226Power();
void updateAllSensors();

#endif // SENSORS_H