#include <Arduino.h>
#include <nvs_flash.h>
#include "power_measurement.h"
#include "motion_control.h"
#include "persistence.h"
#include "positioning.h"
#include "sensors.h"
#include "sd_logger.h"
#include "web_server.h"
#include "test_routines.h"

static bool lastBtnSelect = HIGH;
static uint8_t lastMCPInputs = 0xFF;

static bool comboUpTriggered = false;
static bool comboDownTriggered = false;

void monitorDigitalInputs() {
    bool selectPressed = (digitalRead(PIN_BTN_SELECT) == LOW);
    
    uint8_t mcpInputs = 0xFF;
    if (mcpPresent) {
        mcpInputs = readMCP(0x09) & 0xF0;
    }

    bool upPressed   = !(mcpInputs & 0b00010000);
    bool downPressed = !(mcpInputs & 0b00100000);

    if (selectPressed && upPressed) {
        if (!comboUpTriggered) {
            comboUpTriggered = true;
            int32_t currentPos = getCalculatedPosition();
            setUpperLimit(currentPos);
            logEventAsync("UPPER_LIMIT_SET_VIA_BUTTONS");
            playLoudAlert();
        }
    } else {
        comboUpTriggered = false;
    }

    if (selectPressed && downPressed) {
        if (!comboDownTriggered) {
            comboDownTriggered = true;
            int32_t currentPos = getCalculatedPosition();
            setLowerLimit(currentPos);
            logEventAsync("LOWER_LIMIT_SET_VIA_BUTTONS");
            playLoudAlert();
        }
    } else {
        comboDownTriggered = false;
    }

    if (!selectPressed && lastBtnSelect == LOW) {
        logEventAsync("INPUT_BTN_SELECT_RELEASED");
    } else if (selectPressed && lastBtnSelect == HIGH && !upPressed && !downPressed) {
        logEventAsync("INPUT_BTN_SELECT_PRESSED");
    }
    lastBtnSelect = !selectPressed;

    if (mcpPresent) {
        uint8_t changed = mcpInputs ^ lastMCPInputs;

        if ((changed & 0b00010000) && !selectPressed) {
            logEventAsync((mcpInputs & 0b00010000) ? "INPUT_UP_RELEASED" : "INPUT_UP_PRESSED");
        }
        if ((changed & 0b00100000) && !selectPressed) {
            logEventAsync((mcpInputs & 0b00100000) ? "INPUT_DOWN_RELEASED" : "INPUT_DOWN_PRESSED");
        }
        if (changed & 0b01000000) {
            logEventAsync((mcpInputs & 0b01000000) ? "INPUT_FW_RELEASED" : "INPUT_FW_PRESSED");
        }
        if (changed & 0b10000000) {
            logEventAsync((mcpInputs & 0b10000000) ? "INPUT_RV_RELEASED" : "INPUT_RV_PRESSED");
        }

        lastMCPInputs = mcpInputs;
    }
}

void setup() {
    initPowerMeasurement();
    setI2CNormal();
    
    mcpPresent = checkMCPPresence();
    if (mcpPresent) {
        initMCP23008();
    }

    initPersistence();
    analyzeACPhases();

    bool autoMotionTriggered = false;
    if (currentCtrlMode == CTRL_MODE_DIRECT && allPhasesPresent) {
        setMotionState(MOTION_FORWARD);
        autoMotionTriggered = true;
    }

    Serial.begin(115200);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }

    RS485_Port0.begin(115200, SERIAL_8N1, PIN_U0_RX, PIN_U0_TX);
    RS485_Port1.begin(115200, SERIAL_8N1, PIN_U1_RX, PIN_U1_TX);
    pinMode(PIN_U0_MOD, OUTPUT); pinMode(PIN_U1_MOD, OUTPUT);
    digitalWrite(PIN_U0_MOD, LOW); digitalWrite(PIN_U1_MOD, LOW);

    pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
    
    initPositioning();

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    if (!rtc.begin(&Wire)) logDiag("[WARNING] DS3231 RTC not found!\r\n");
    initSensors();

    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    if (SD.begin(PIN_SD_CS, SPI, 4000000, "/sd", 5, true)) {
        initSDLogger();
        logEventAsync("SYSTEM_POWER_ON");
        if (autoMotionTriggered) {
            logEventAsync("DIRECT_MODE_AUTO_FORWARD_ENGAGED");
        }
    }

    initWebServer("ESP32C6-TestRig");
    printMenu();

    playStartupMelody();
}

void loop() {
    handleWebServer();
    updatePeriodicStats();
    monitorDigitalInputs();
    
    // Evaluate position limits and target stopping conditions
    processMotionLogic();

    if (Serial.available() > 0) {
        char cmd = Serial.read();
        handleCLICommand(cmd);
    }
}