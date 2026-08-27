#include "sensors.h"
#include "test_routines.h"
#include <math.h>

Adafruit_BME280 bme;

float inaBusVoltage = 0.0f;
float inaCurrent = 0.0f;
float inaPower = 0.0f;

float bmeTemperature = 0.0f;
float bmeHumidity = 0.0f;
float bmePressure = 0.0f;

int16_t accelX = 0;
int16_t accelY = 0;
int16_t accelZ = 0;

float accelX_mms2 = 0.0f;
float accelY_mms2 = 0.0f;
float accelZ_mms2 = 0.0f;

int pitchDeg = 0;
int tiltDeg = 0;

// Dynamic 3D Static Gravity Vector (Normalized to 1g = 1000 LSB)
static float gravityX = 0.0f;
static float gravityY = 0.0f;
static float gravityZ = 0.0f;
static unsigned long lastSampleTimeMs = 0;
static bool gravityInitialized = false;

bool initAccelerometer() {
    setI2CNormal();
    
    // CTRL_REG1 (0x20): Set 100Hz ODR, Normal Power Mode, Enable X, Y, Z
    Wire.beginTransmission(ADDR_LIS2DH12);
    Wire.write(0x20);
    Wire.write(0x57);
    if (Wire.endTransmission() != 0) {
        logDiag("[WARNING] LIS2DH12 Accelerometer not detected at 0x19!\r\n");
        return false;
    }

    // CTRL_REG4 (0x23): Enable Block Data Update (BDU) & High Resolution Mode (12-bit)
    Wire.beginTransmission(ADDR_LIS2DH12);
    Wire.write(0x23);
    Wire.write(0x88);
    Wire.endTransmission();

    logDiag("[INIT] LIS2DH12 Accelerometer initialized successfully.\r\n");
    return true;
}

bool initBME280() {
    setI2CNormal();
    if (!bme.begin(ADDR_BME280, &Wire)) {
        logDiag("[WARNING] BME280 sensor not found!\r\n");
        return false;
    }
    logDiag("[INIT] BME280 Weather Sensor initialized successfully.\r\n");
    return true;
}

bool initSensors() {
    bool accelOK = initAccelerometer();
    bool bmeOK = initBME280();
    return (accelOK && bmeOK);
}

void readAccelerometer(int16_t &x, int16_t &y, int16_t &z) {
    setI2CNormal();
    
    Wire.beginTransmission(ADDR_LIS2DH12);
    Wire.write(0x28 | 0x80); // OUT_X_L with auto-increment
    if (Wire.endTransmission(false) != 0) {
        Wire.endTransmission(true);
        return;
    }
    
    if (Wire.requestFrom((uint8_t)ADDR_LIS2DH12, (uint8_t)6) == 6) {
        x = Wire.read() | (Wire.read() << 8);
        y = Wire.read() | (Wire.read() << 8);
        z = Wire.read() | (Wire.read() << 8);
        
        x >>= 4;
        y >>= 4;
        z >>= 4;
    }
    
    accelX = x;
    accelY = y;
    accelZ = z;

    float fx = (float)accelX;
    float fy = (float)accelY;
    float fz = (float)accelZ;

    // Total vector magnitude (1000 LSB = 1.0g in +/-2g High-Res mode)
    float currentMag = sqrt(fx * fx + fy * fy + fz * fz);

    unsigned long nowMs = millis();
    float dtSec = (lastSampleTimeMs == 0) ? 0.01f : (float)(nowMs - lastSampleTimeMs) / 1000.0f;
    lastSampleTimeMs = nowMs;
    if (dtSec > 1.0f) dtSec = 1.0f;

    // 1. Initialize Gravity Vector on First Read
    if (!gravityInitialized) {
        if (currentMag > 100.0f) {
            gravityX = (fx / currentMag) * 1000.0f;
            gravityY = (fy / currentMag) * 1000.0f;
            gravityZ = (fz / currentMag) * 1000.0f;
            gravityInitialized = true;
        }
    } else {
        // 2. Gate Update: Only filter baseline if total force is near static rest (0.85g to 1.15g)
        // Impacts (> 1.15g) and freefalls (< 0.85g) are BLOCKED from corrupting the baseline!
        if (currentMag >= 850.0f && currentMag <= 1150.0f) {
            // 1-second Low-Pass Filter constant (tau = 1.0s)
            float alpha = dtSec / (1.0f + dtSec);

            gravityX = gravityX * (1.0f - alpha) + fx * alpha;
            gravityY = gravityY * (1.0f - alpha) + fy * alpha;
            gravityZ = gravityZ * (1.0f - alpha) + fz * alpha;

            // Re-normalize magnitude strictly to 1.0g (1000 LSB)
            float gMag = sqrt(gravityX * gravityX + gravityY * gravityY + gravityZ * gravityZ);
            if (gMag > 100.0f) {
                gravityX = (gravityX / gMag) * 1000.0f;
                gravityY = (gravityY / gMag) * 1000.0f;
                gravityZ = (gravityZ / gMag) * 1000.0f;
            }
        }
    }

    // 3. Subtract Static 3D Gravity Vector from Raw Input
    float deltaX = fx - gravityX;
    float deltaY = fy - gravityY;
    float deltaZ = fz - gravityZ;

    // Convert dynamic delta LSB to mm/s² (1 LSB = 1 mg = 9.80665 mm/s²)
    float instX = deltaX * 9.80665f;
    float instY = deltaY * 9.80665f;
    float instZ = deltaZ * 9.80665f;

    // Squelch residual baseline sensor noise (< 50 mm/s²)
    if (fabs(instX) < 50.0f) instX = 0.0f;
    if (fabs(instY) < 50.0f) instY = 0.0f;
    if (fabs(instZ) < 50.0f) instZ = 0.0f;

    accelX_mms2 = instX;
    accelY_mms2 = instY;
    accelZ_mms2 = instZ;

    // 4. Inclination Angles (Uses raw orientation vector)
    pitchDeg = (int)round(atan2(fy, sqrt(fx * fx + fz * fz)) * 180.0f / M_PI);
    tiltDeg  = (int)round(atan2(fz, sqrt(fx * fx + fy * fy)) * 180.0f / M_PI);
}

void readWeatherSensor(float &temp, float &hum, float &pres) {
    setI2CNormal();
    temp = bme.readTemperature();
    hum = bme.readHumidity();
    pres = bme.readPressure() / 100.0F;

    bmeTemperature = temp;
    bmeHumidity = hum;
    bmePressure = pres;
}

void readINA226Power() {
    inaBusVoltage = 0.0f; 
    inaCurrent = 0.0f;

    setI2CNormal();
    Wire.beginTransmission(ADDR_INA226);
    Wire.write(0x02);
    if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)ADDR_INA226, (uint8_t)2) == 2) {
        inaBusVoltage = ((Wire.read() << 8) | Wire.read()) * 0.00125f;
    }

    Wire.beginTransmission(ADDR_INA226);
    Wire.write(0x01);
    if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)ADDR_INA226, (uint8_t)2) == 2) {
        inaCurrent = (((int16_t)(Wire.read() << 8) | Wire.read()) * 0.0025f) / 0.1f;
    }
    
    inaPower = inaBusVoltage * (inaCurrent / 1000.0f);
}

void updateAllSensors() {
    readINA226Power();
    readWeatherSensor(bmeTemperature, bmeHumidity, bmePressure);
    readAccelerometer(accelX, accelY, accelZ);
}