#ifndef TEST_ROUTINES_H
#define TEST_ROUTINES_H

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <RTClib.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// --- HARDWARE PIN DEFINITIONS ---
#define PIN_AN_L1_L2_A   0
#define PIN_AN_L1_L2_B   1
#define PIN_U0_RX        2
#define PIN_U0_MOD       3
#define PIN_AN_MOTOR_V   4
#define PIN_AN_MOTOR_U   5
#define PIN_AN_MOTOR_W   6
#define PIN_SD_MOSI      7
#define PIN_SD_CS        8
#define PIN_BTN_SELECT   9
#define PIN_SD_SCK       10
#define PIN_SD_MISO      11
#define PIN_U1_MOD       15
#define PIN_U0_TX        16
#define PIN_U1_RX        17
#define PIN_I2C_SDA      18
#define PIN_I2C_SCL      19
#define PIN_ENC_A        20
#define PIN_ENC_B        21
#define PIN_BUZZER       22
#define PIN_U1_TX        23

// --- EXPECTED I2C ADDRESSES ---
#define ADDR_LIS2DH12    0x19
#define ADDR_MCP23008    0x20
#define ADDR_INA226      0x40
#define ADDR_EEPROM      0x50
#define ADDR_DS3231      0x68
#define ADDR_BME280      0x77
#define ADDR_EERAM_DATA  0x50 
#define ADDR_EERAM_CTRL  0x18 

// --- EXPORTED HARDWARE OBJECTS & VARIABLES ---
extern RTC_DS3231 rtc;
extern Adafruit_BME280 bme;
extern bool mcpPresent;
extern bool mcpOutputsState;

// --- EXPORT HARDWARE SERIAL INSTANCES ---
extern HardwareSerial RS485_Port0;
extern HardwareSerial RS485_Port1;

// --- ISR & BUS HELPERS ---
void IRAM_ATTR handleEncoderISR();
void setI2CNormal();
void setI2CSwapped();

// --- MCP23008 HELPERS ---
bool checkMCPPresence();
void writeMCP(uint8_t reg, uint8_t value);
uint8_t readMCP(uint8_t reg);
void initMCP23008();
void sequenceMCPOutputsOnStartup();

// --- LOGGING HELPER ---
void logDiag(const String& msg);

// --- TEST ROUTINES ---
void liveAnalogMonitor();
void testEncoder();
void scanI2C();
void toggleMCPOutputs();
void testRS485();
void playLoudAlert();
void playMelody();
void testSDCard();
void liveI2CDashboard();
void enable47L16AutoStore();
void testEERAM();
void testWiFiRF();

// --- MENU & CLI HANDLER ---
void printMenu();
void handleCLICommand(char cmd);

#endif // TEST_ROUTINES_H