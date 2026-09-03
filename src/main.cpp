#include <Arduino.h>
#include <nvs_flash.h>
#include <esp_task_wdt.h>
#include <WiFi.h>
#include <time.h>
#include <SD.h>
#include "power_measurement.h"
#include "motion_control.h"
#include "persistence.h"
#include "positioning.h"
#include "sensors.h"
#include "melodies.h"
#include "sd_logger.h"
#include "web_server.h"
#include "test_routines.h"
#include "bluetooth.h"
#include "websocket.h"
#include "telemetry_helper.h"

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

    initWebSocket();
    initBluetooth();

    String fullBtName = "ELLHoist_" + String(sysStats.serialNumber) + "_AP";
    initWebServer(fullBtName.c_str(),"3LLH01s7");

    initWatchdog();
    playStartupMelodyConfigured();
}

bool inTestMenuMode = false;
static String serialBuffer = "";

void handleSerialAPI() {
    while (Serial.available() > 0) {
        char c = Serial.read();

        // 1. Single-character Test Menu Mode
        if (inTestMenuMode) {
            if (c == 'x' || c == 'X') {
                inTestMenuMode = false;
                Serial.println("\r\nExited Test Menu. Returning to Main API mode.");
                return;
            }
            handleCLICommand(c);
            return;
        }

        // 2. Echo character back to PuTTY terminal
        if (c != '\r' && c != '\n') {
            Serial.write(c);
            serialBuffer += c;
        }

        // 3. Process command on either '\r' or '\n'
        if (c == '\r' || c == '\n') {
            if (serialBuffer.length() > 0) {
                String input = serialBuffer;
                serialBuffer = ""; // Reset buffer
                input.trim();

                Serial.println(); // Print newline to terminal

                if (input == "RST") {
                    Serial.println(">> REBOOTING DEVICE...");
                    Serial.flush(); 
                    delay(500);     
                    ESP.restart();  
                } 
                else if (input == "TEST" || input == "MENU") {
                    inTestMenuMode = true;
                    Serial.println("\r\n--- Entering Test Routines CLI Menu ---");
                    Serial.println("Press 'X' at any prompt to return to Main API mode.");
                    printMenu();
                    return;
                }
                else if (input.startsWith("SET_SN=")) {
                    String sn = input.substring(7);
                    snprintf(sysStats.serialNumber, sizeof(sysStats.serialNumber), "%s", sn.c_str());
                    saveStatsToEEPROM();
                    Serial.println("OK:SN_SET");
                } 
                else if (input.startsWith("SET_MIN=")) {
                    sysStats.deviceRuntimeSec = input.substring(8).toInt() * 60;
                    saveStatsToEEPROM();
                    Serial.println("OK:MIN_SET");
                } 
                else if (input.startsWith("SET_PWR=")) {
                    sysStats.br1Cycles = input.substring(8).toInt();
                    saveStatsToEEPROM();
                    Serial.println("OK:PWR_SET");
                } 
                else if (input.startsWith("SET_SSID=")) {
                    setSSID(input.substring(9));
                    Serial.println("OK:SSID_SET");
                } 
                else if (input.startsWith("SET_PASS=")) {
                    setPass(input.substring(9));
                    Serial.println("OK:PASS_SET");
                } 
                else if (input.startsWith("SET_CTRL=")) {
                    String mode = input.substring(9);
                    mode.toUpperCase();
                    if (mode == "DIRECT") {
                        setOperationControlMode(CTRL_MODE_DIRECT);
                        Serial.println("OK:CTRL_DIRECT");
                    } else if (mode == "LOW_VOLTAGE") {
                        setOperationControlMode(CTRL_MODE_LOW_VOLTAGE);
                        Serial.println("OK:CTRL_LOW_VOLTAGE");
                    } else {
                        Serial.println("ERR:INVALID_CTRL_MODE");
                    }
                }
                else if (input.startsWith("DEL_LOG=") || input.startsWith("DEL_FILE=")) {
                    int idx = input.indexOf('=');
                    String fileName = input.substring(idx + 1);
                    fileName.trim();
                    if (!fileName.startsWith("/")) fileName = "/" + fileName;

                    if (SD.exists(fileName)) {
                        if (SD.remove(fileName)) {
                            Serial.println("OK:LOG_DELETED");
                        } else {
                            Serial.println("ERR:DELETE_FAILED");
                        }
                    } else {
                        Serial.println("ERR:FILE_NOT_FOUND");
                    }
                }
                else if (input == "SYNC_RTC") {
                    if (WiFi.status() == WL_CONNECTED) {
                        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
                        struct tm timeinfo;
                        if (getLocalTime(&timeinfo, 5000)) {
                            rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, 
                                                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
                            Serial.println("OK:RTC_SYNCED");
                        } else {
                            Serial.println("ERR:NTP_TIMEOUT");
                        }
                    } else {
                        Serial.println("ERR:WIFI_NOT_CONNECTED");
                    }
                }
                else if (input == "INFO") {
                    Serial.println("--- START_INFO ---");
                    Serial.println("Serial: " + String(sysStats.serialNumber));
                    Serial.println("HWID: " + hwID);
                    Serial.println("Version: " + FIRMWARE_VERSION); 
                    Serial.printf("Runtime (sec): %u\n", sysStats.deviceRuntimeSec);
                    Serial.printf("BR1 Cycles: %u\n", sysStats.br1Cycles);
                    Serial.printf("BR2 Cycles: %u\n", sysStats.br2Cycles);
                    String ssid = getSSID();
                    Serial.println("WiFi SSID: " + (ssid == "" || ssid == "null" ? "NOT SET" : ssid));
                    Serial.println("--- END_INFO ---");
                }
            }
        }

        // Buffer overflow protection
        if (serialBuffer.length() > 128) {
            serialBuffer = "";
        }
    }
}

long int lastMinuteMillis = 0;
void loop() {
    esp_task_wdt_reset();

    handleWebServer();
    updatePeriodicStats();
    monitorDigitalInputs();
    processMotionLogic();

    handleSerialAPI();

    handleWebSocket();
    handleBluetooth();

    if (millis() - lastMinuteMillis >= 15000) {
        lastMinuteMillis = millis();

        notifyMinutesUpdate();
        notifyTelemetryUpdate();
        uploadWebsocketData();
    }
}