#include <Arduino.h>
#include <nvs_flash.h>
#include <esp_task_wdt.h>
#include "power_measurement.h"
#include "motion_control.h"
#include "persistence.h"
#include "positioning.h"
#include "sensors.h"
#include "melodies.h"
#include "sd_logger.h"
#include "web_server.h"
#include "test_routines.h"

#define WDT_TIMEOUT_MSECONDS 500
#define PIN_BTN_SELECT 9

static bool lastBtnSelect = HIGH;
static uint8_t lastMCPInputs = 0xF0;

static bool comboUpTriggered = false;
static bool comboDownTriggered = false;

void initWatchdog() {
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        esp_task_wdt_config_t config = {
            .timeout_ms = WDT_TIMEOUT_MSECONDS,
            .idle_core_mask = (1 << 0),
            .trigger_panic = true
        };
        esp_task_wdt_reconfigure(&config);
        esp_task_wdt_add(NULL);
    #else
        esp_task_wdt_init(WDT_TIMEOUT_MSECONDS, true);
        esp_task_wdt_add(NULL);
    #endif
}

void monitorDigitalInputs() {
    bool selectPressed = (digitalRead(PIN_BTN_SELECT) == LOW);
    
    uint8_t mcpInputs = lastMCPInputs;
    if (mcpPresent) {
        uint8_t sample1 = readMCP(0x09) & 0xF0;
        if (sample1 != lastMCPInputs) {
            delay(5);
            uint8_t sample2 = readMCP(0x09) & 0xF0;
            if (sample1 == sample2) {
                mcpInputs = sample1;
            }
        } else {
            mcpInputs = sample1;
        }
    }

    if (mcpPresent) {
        uint8_t changed = mcpInputs ^ lastMCPInputs;

        bool upPressed   = !(mcpInputs & 0b00010000); // GP4
        bool downPressed = !(mcpInputs & 0b00100000); // GP5
        bool fwPressed   = !(mcpInputs & 0b01000000); // GP6
        bool rvPressed   = !(mcpInputs & 0b10000000); // GP7

        if ((changed & 0b00010000) && !selectPressed) {
            logEventAsync(upPressed ? "INPUT_UP_PRESSED" : "INPUT_UP_RELEASED");
        }

        if ((changed & 0b00100000) && !selectPressed) {
            logEventAsync(downPressed ? "INPUT_DOWN_PRESSED" : "INPUT_DOWN_RELEASED");
        }

        if (changed & 0b01000000) {
            if (fwPressed) {
                logEventAsync("INPUT_FW_PRESSED");
                setMotionState(MOTION_FORWARD);
            } else {
                logEventAsync("INPUT_FW_RELEASED");
                if (getMotionState() == MOTION_FORWARD) {
                    setMotionState(MOTION_STOP);
                }
            }
        }

        if (changed & 0b10000000) {
            if (rvPressed) {
                logEventAsync("INPUT_RV_PRESSED");
                setMotionState(MOTION_REVERSE);
            } else {
                logEventAsync("INPUT_RV_RELEASED");
                if (getMotionState() == MOTION_REVERSE) {
                    setMotionState(MOTION_STOP);
                }
            }
        }

        lastMCPInputs = mcpInputs;
    }

    bool upState   = !(mcpInputs & 0b00010000);
    bool downState = !(mcpInputs & 0b00100000);

    if (selectPressed && upState) {
        if (!comboUpTriggered) {
            comboUpTriggered = true;
            int32_t currentPos = getCalculatedPosition();
            setUpperLimit(currentPos);
            logEventAsync("UPPER_LIMIT_SET_VIA_BUTTONS");
            playUpperLimitMelodyConfigured();
        }
    } else {
        comboUpTriggered = false;
    }

    if (selectPressed && downState) {
        if (!comboDownTriggered) {
            comboDownTriggered = true;
            int32_t currentPos = getCalculatedPosition();
            setLowerLimit(currentPos);
            logEventAsync("LOWER_LIMIT_SET_VIA_BUTTONS");
            playLowerLimitMelodyConfigured();
        }
    } else {
        comboDownTriggered = false;
    }

    if (!selectPressed && lastBtnSelect == LOW) {
        logEventAsync("INPUT_BTN_SELECT_RELEASED");
    } else if (selectPressed && lastBtnSelect == HIGH && !upState && !downState) {
        logEventAsync("INPUT_BTN_SELECT_PRESSED");
    }
    lastBtnSelect = !selectPressed;
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

    initWatchdog();
    playStartupMelodyConfigured();
}

void loop() {
    esp_task_wdt_reset();

    handleWebServer();
    updatePeriodicStats();
    monitorDigitalInputs();
    
    processMotionLogic();

    if (Serial.available() > 0) {
        char cmd = Serial.read();
        handleCLICommand(cmd);
    }
}