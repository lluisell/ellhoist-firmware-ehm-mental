#include <Arduino.h>
#include <nvs_flash.h> // Fixed NVS error
#include "power_measurement.h"
#include "motion_control.h"
#include "web_server.h"
#include "test_routines.h"

void setup() {
    Serial.begin(115200);
    delay(2000); 

    // --- FIX 1: INITIALIZE NVS FLASH MEMORY ---
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }

    // 1. Initialize ADC & Power Sampling
    initPowerMeasurement();

    // 2. Immediate Early Startup 3-Phase Verification
    analyzeACPhases();
    
    logDiag("\r\n=========================================\r\n");
    logDiag(" INITIAL 3-PHASE POWER DIAGNOSTIC RESULT \r\n");
    logDiag("=========================================\r\n");
    logDiag("[AC STATUS] All Phases Present: " + String(allPhasesPresent ? "YES [OK]" : "NO [MISSING]") + "\r\n");
    logDiag("[AC STATUS] Sequence Direction: " + phaseSequenceStatus + "\r\n");
    logDiag("[AC STATUS] Line Frequency    : " + String(phaseFrequencyHz, 2) + " Hz\r\n");
    logDiag("[AC STATUS] V_L1L2 RMS        : " + String(vL1L2_RMS, 2) + " V\r\n");
    logDiag("[AC STATUS] V_L3L2 RMS        : " + String(vL3L2_RMS, 2) + " V\r\n");
    logDiag("=========================================\r\n\r\n");

    // 3. Start Web Server
    initWebServer("ESP32C6-TestRig");

    // --- FIX 2: INITIALIZE HP UARTS (NOT LP UART / SERIAL2) ---
    RS485_Port0.begin(115200, SERIAL_8N1, PIN_U0_RX, PIN_U0_TX);
    RS485_Port1.begin(115200, SERIAL_8N1, PIN_U1_RX, PIN_U1_TX);

    pinMode(PIN_U0_MOD, OUTPUT); pinMode(PIN_U1_MOD, OUTPUT);
    digitalWrite(PIN_U0_MOD, LOW); digitalWrite(PIN_U1_MOD, LOW); 

    pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), handleEncoderISR, CHANGE);

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    // 4. I2C Bus & Expansion Devices Initialization
    setI2CNormal();
    mcpPresent = checkMCPPresence();
    if (mcpPresent) {
        initMCP23008();
        logDiag("[INIT] MCP23008 detected and initialized.\r\n");
        //sequenceMCPOutputsOnStartup();
    } else {
        logDiag("[INIT] MCP23008 NOT detected.\r\n");
    }

    if (!rtc.begin(&Wire)) logDiag("[WARNING] DS3231 RTC not found!\r\n");
    if (!bme.begin(ADDR_BME280, &Wire)) logDiag("[WARNING] BME280 sensor not found!\r\n");

    Wire.beginTransmission(ADDR_LIS2DH12);
    Wire.write(0x20); Wire.write(0x77); 
    Wire.endTransmission();

    enable47L16AutoStore();
    printMenu();
}

void loop() {
    handleWebServer();

    if (Serial.available() > 0) {
        char cmd = Serial.read();
        handleCLICommand(cmd);
    }
}