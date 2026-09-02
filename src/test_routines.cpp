#include "test_routines.h"
#include "motion_control.h"
#include "web_server.h"
#include "persistence.h"
#include "power_measurement.h"
#include "sensors.h"
#include <esp_task_wdt.h>

RTC_DS3231 rtc;
HardwareSerial RS485_Port0(0);
HardwareSerial RS485_Port1(1);

bool mcpPresent = false;
bool mcpOutputsState = false;

void logDiag(const String& msg) {
    Serial.print(msg);
    appendDiagLog(msg);
}

void playStartupMelody() {
    int melody[] = { 523, 659, 784, 1047 };
    int durations[] = { 100, 100, 100, 250 };
    for (int i = 0; i < 4; i++) {
        esp_task_wdt_reset();
        tone(PIN_BUZZER, melody[i], durations[i]);
        delay((int)(durations[i] * 1.25));
        noTone(PIN_BUZZER);
    }
}

void playLoudAlert() {
    Serial.print("\r\n[BUZZER] Playing 4.0 kHz Resonant Alert...\r\n");
    for (int i = 0; i < 3; i++) {
        esp_task_wdt_reset();
        tone(PIN_BUZZER, 4000, 100);
        delay(150);
        noTone(PIN_BUZZER);
    }
}

void playMelody() {
    Serial.print("\r\n[BUZZER] Playing Melody...\r\n");
    int melody[] = { 262, 330, 392, 523, 392, 784 }; 
    int durations[] = { 150, 150, 150, 200, 150, 400 }; 
    for (int i = 0; i < 6; i++) {
        esp_task_wdt_reset();
        tone(PIN_BUZZER, melody[i], durations[i]);
        delay(durations[i] * 1.30);
        noTone(PIN_BUZZER);
    }
}

void setI2CNormal() {
    Wire.end();
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000); 
}

void setI2CSwapped() {
    Wire.end();
    Wire.begin(PIN_I2C_SCL, PIN_I2C_SDA, 100000);
}

bool checkMCPPresence() {
    setI2CSwapped();
    Wire.beginTransmission(ADDR_MCP23008);
    bool detected = (Wire.endTransmission() == 0);
    setI2CNormal();
    return detected;
}

void writeMCP(uint8_t reg, uint8_t value) {
    if (!mcpPresent) return;
    setI2CSwapped();
    Wire.beginTransmission(ADDR_MCP23008);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
    setI2CNormal();
}

uint8_t readMCP(uint8_t reg) {
    if (!mcpPresent) return 0x00;
    setI2CSwapped();
    uint8_t val = 0x00;
    Wire.beginTransmission(ADDR_MCP23008);
    Wire.write(reg);
    if (Wire.endTransmission() == 0) {
        if (Wire.requestFrom((uint8_t)ADDR_MCP23008, (uint8_t)1) == 1) {
            val = Wire.read();
        }
    }
    setI2CNormal();
    return val;
}

void initMCP23008() {
    writeMCP(0x00, 0xF0);
    writeMCP(0x06, 0xF0);
    writeMCP(0x09, 0x00);
}

void sequenceMCPOutputsOnStartup() {
    if (!mcpPresent) {
        logDiag("[STARTUP TEST] MCP23008 not found. Skipping output sequence.\r\n");
        return;
    }

    logDiag("\r\n=========================================\r\n");
    logDiag(" STARTUP SEQUENCING: MCP23008 OUTPUTS    \r\n");
    logDiag("=========================================\r\n");

    struct OutputPin {
        uint8_t bitMask;
        const char* label;
    };

    OutputPin targets[] = {
        { 0b00000100, "BRAKE_1  (GP2)" },
        { 0b00000000, "OFF      (GP2)" },
        { 0b00001000, "BRAKE_2  (GP3)" },
        { 0b00000000, "OFF      (GP2)" },
        { 0b00000100, "BRAKE_1  (GP2)" },
        { 0b00001100, "BRAKE_2  (GP3)" },
        { 0b00001101, "FW_CONT+BRAKES  (GP0)" },
        { 0b00000100, "BRAKE_1  (GP2)" },
        { 0b00000000, "OFF      (GP2)" },
        { 0b00000100, "BRAKE_1  (GP2)" },
        { 0b00001100, "BRAKE_2  (GP3)" },
        { 0b00001110, "REV_CONT+BRAKES  (GP1)" },
        { 0b00000100, "BRAKE_1  (GP2)" },
        { 0b00000000, "OFF      (GP2)" }
    };

    delay(1000);
    for (int i = 0; i < 14; i++) {
        esp_task_wdt_reset();
        writeMCP(0x09, targets[i].bitMask);
        char buf[100];
        snprintf(buf, sizeof(buf), "[TEST] %s --> ON (2 sec)\r\n", targets[i].label);
        logDiag(buf);
        delay(2000);
    }

    logDiag("=========================================\r\n");
    logDiag(" OUTPUT SEQUENCING TEST COMPLETE        \r\n");
    logDiag("=========================================\r\n\r\n");
}

// WATCHDOG-SAFE NON-BLOCKING SERIAL LINE READER
static String readSerialLine() {
    while (Serial.available()) Serial.read(); // Clear buffer
    
    String line = "";
    while (true) {
        esp_task_wdt_reset(); // Keep watchdog alive while waiting for human input
        handleWebServer();    // Keep web server responsive
        
        if (Serial.available() > 0) {
            char c = (char)Serial.read();
            if (c == '\n' || c == '\r') {
                if (line.length() > 0) {
                    break;
                }
            } else {
                line += c;
            }
        }
        delay(10);
    }
    line.trim();
    return line;
}

void liveAnalogMonitor() {
    Serial.print("\r\n--- LIVE ANALOG MONITOR ---\r\nSend 'q' to stop.\r\n");
    delay(1000);
    while(Serial.available()) Serial.read();

    while (true) {
        esp_task_wdt_reset();
        if (Serial.available() > 0 && Serial.read() == 'q') break;

        Serial.printf("V_L1L2_A: %04d | V_L1L2_B: %04d | I_MOT_U: %04d | I_MOT_V: %04d | I_MOT_W: %04d\r\n", 
                      analogRead(PIN_AN_L1_L2_A), analogRead(PIN_AN_L1_L2_B), 
                      analogRead(PIN_AN_MOTOR_U), analogRead(PIN_AN_MOTOR_V), analogRead(PIN_AN_MOTOR_W));
        delay(200); 
    }
    Serial.print("\r\nExited Analog Monitor.\r\n");
}

void testEncoder() {
    Serial.print("\r\n--- ENCODER & OPTOPAIR RAW PIN DEBUGGER ---\r\n");
    Serial.print("Press 'p' to toggle internal PULLUP / PULLDOWN mode. 'q' to exit.\r\n\r\n");
    delay(1000);
    while(Serial.available()) Serial.read();

    bool usePullup = true;

    while (true) {
        esp_task_wdt_reset();
        if (Serial.available() > 0) {
            char c = Serial.read();
            if (c == 'q' || c == 'Q') break;
            if (c == 'p' || c == 'P') {
                usePullup = !usePullup;
                pinMode(PIN_ENC_A, usePullup ? INPUT_PULLUP : INPUT_PULLDOWN);
                pinMode(PIN_ENC_B, usePullup ? INPUT_PULLUP : INPUT_PULLDOWN);
                Serial.printf("\r\n[PIN MODE CHANGED] GPIO 20/21 set to: %s\r\n\r\n", 
                              usePullup ? "INPUT_PULLUP" : "INPUT_PULLDOWN");
            }
        }

        int rawA = digitalRead(PIN_ENC_A);
        int rawB = digitalRead(PIN_ENC_B);

        delay(100);
    }
    
    pinMode(PIN_ENC_A, INPUT_PULLUP);
    pinMode(PIN_ENC_B, INPUT_PULLUP);
    Serial.print("\r\nExited Encoder Debugger.\r\n");
}

void scanI2C() {
    Serial.print("\r\n--- I2C BUS SCANNER ---\r\n");
    setI2CNormal();
    int nDevices = 0;
    for(byte address = 1; address < 127; address++) {
        esp_task_wdt_reset();
        Wire.beginTransmission(address);
        if (Wire.endTransmission() == 0) {
            Serial.printf("Found device at 0x%02X", address);
            if (address == ADDR_LIS2DH12) Serial.print(" -> (LIS2DH12)");
            if (address == ADDR_INA226)   Serial.print(" -> (INA226)");
            if (address == ADDR_EEPROM)   Serial.print(" -> (DS3231 EEPROM)");
            if (address == ADDR_DS3231)   Serial.print(" -> (DS3231MZ)");
            if (address == ADDR_BME280)   Serial.print(" -> (BME280)");
            Serial.print("\r\n");
            nDevices++;
        }    
    }

    setI2CSwapped();
    Wire.beginTransmission(ADDR_MCP23008);
    if (Wire.endTransmission() == 0) {
        Serial.print("Found device at 0x20 -> (MCP23008 via Swapped Pins)\r\n");
        nDevices++;
    }
    setI2CNormal();

    if (nDevices == 0) Serial.print("No I2C devices found!\r\n");
}

void toggleMCPOutputs() {
    if (!mcpPresent) {
        Serial.print("\r\n[ERROR] MCP23008 not present on bus!\r\n");
        return;
    }
    mcpOutputsState = !mcpOutputsState;
    uint8_t mask = mcpOutputsState ? 0b00001111 : 0b00000000; 
    writeMCP(0x09, mask);
    
    Serial.print("\r\n--- MCP23008 OUTPUTS TOGGLED ---\r\n");
    Serial.printf("FW_CONT (GP0): %s\r\n", mcpOutputsState ? "ON" : "OFF");
    Serial.printf("RV_CONT (GP1): %s\r\n", mcpOutputsState ? "ON" : "OFF");
    Serial.printf("BRAKE_1 (GP2): %s\r\n", mcpOutputsState ? "ON" : "OFF");
    Serial.printf("BRAKE_2 (GP3): %s\r\n", mcpOutputsState ? "ON" : "OFF");
}

void testRS485() {
    Serial.print("\r\n--- RS485 LOOPBACK TEST ---\r\n");
    digitalWrite(PIN_U0_MOD, HIGH); digitalWrite(PIN_U1_MOD, LOW);
    delay(10); 
    RS485_Port0.println("PING_FROM_UART0");
    RS485_Port0.flush(); 
    delay(50); 

    String rxData = "";
    while(RS485_Port1.available()) rxData += (char)RS485_Port1.read();
    
    if (rxData.indexOf("PING_FROM_UART0") >= 0) Serial.print("[SUCCESS] UART0 Transmitted -> UART1 Received.\r\n");
    else Serial.print("[FAIL] UART1 did not receive test packet.\r\n");

    digitalWrite(PIN_U1_MOD, HIGH); digitalWrite(PIN_U0_MOD, LOW);
    delay(10); 
    RS485_Port1.println("PING_FROM_UART1");
    RS485_Port1.flush();
    delay(50); 

    rxData = "";
    while(RS485_Port0.available()) rxData += (char)RS485_Port0.read();
    
    if (rxData.indexOf("PING_FROM_UART1") >= 0) Serial.print("[SUCCESS] UART1 Transmitted -> UART0 Received.\r\n");
    else Serial.print("[FAIL] UART0 did not receive test packet.\r\n");
    
    digitalWrite(PIN_U0_MOD, LOW); digitalWrite(PIN_U1_MOD, LOW);
}

void testSDCard() {
    Serial.print("\r\n--- SD CARD DIAGNOSTICS ---\r\n");
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    pinMode(PIN_SD_CS, OUTPUT);
    if (!SD.begin(PIN_SD_CS, SPI, 4000000, "/sd", 5, true)) {
        logDiag("[FAIL] SD Mount failed! Check physical insertion and SPI pins.\r\n");
        return;
    }
    char buf[100];
    snprintf(buf, sizeof(buf), "[OK] SD Card Mounted. Size: %llu MB\r\n", SD.cardSize() / (1024 * 1024));
    logDiag(buf);
}

void liveI2CDashboard() {
    Serial.print("\r\n--- LIVE SENSOR DASHBOARD ---\r\nSend 'q' to stop.\r\n\r\n");
    delay(1000);
    while(Serial.available()) Serial.read();

    while (true) {
        esp_task_wdt_reset();
        if (Serial.available() > 0 && Serial.read() == 'q') break;

        DateTime now = rtc.now();
        float temp = 0, hum = 0, pres = 0;
        readWeatherSensor(temp, hum, pres);

        int16_t ax = 0, ay = 0, az = 0;
        readAccelerometer(ax, ay, az);

        updateAllPowerMeasurements();

        bool btnUp = false, btnDn = false;
        if (mcpPresent) {
            uint8_t mcpInputs = readMCP(0x09);
            btnUp = !(mcpInputs & 0b00010000);
            btnDn = !(mcpInputs & 0b00100000);
        }

        Serial.print("--------------------------------------------------\r\n");
        Serial.printf("[RTC]   Time: %02d:%02d:%02d | Date: %02d/%02d/%04d\r\n", 
                      now.hour(), now.minute(), now.second(), now.day(), now.month(), now.year());
        Serial.printf("[ENV]   Temp: %.2f C  | Hum: %.2f %% | Pres: %.1f hPa\r\n", temp, hum, pres);
        Serial.printf("[ACCEL] X: %6d     | Y: %6d     | Z: %6d\r\n", ax, ay, az);
        Serial.printf("[AC]    V_L1L2: %.1f V | V_L3L2: %.1f V | Freq: %.1f Hz\r\n", vL1L2_RMS, vL3L2_RMS, phaseFrequencyHz);
        Serial.printf("[MOT]   I_U: %.2f A   | I_V: %.2f A   | I_W: %.2f A | Pwr: %.1f W\r\n", motorCurrentU, motorCurrentV, motorCurrentW, motorPower);
        Serial.printf("[INA]   Loop Voltage: %.2f V | Current: %.2f mA\r\n", inaBusVoltage, inaCurrent);
        
        if (mcpPresent) {
            Serial.printf("[MCP]   Inputs  -> UP:%d | DN:%d\r\n", btnUp, btnDn);
        } else {
            Serial.print("[MCP]   OFFLINE / NOT DETECTED\r\n");
        }
        
        delay(500); 
    }
    Serial.print("\r\nExited Dashboard.\r\n");
}

void enable47L16AutoStore() {
    Wire.beginTransmission(0x18);
    Wire.write(0x00);
    Wire.write(0x02);
    if (Wire.endTransmission() == 0) {
        delay(10);
        logDiag("[47L16] Auto-Store (ASE) permanently ENABLED.\r\n");
    }
}

void testEERAM() {
    Serial.print("\r\n--- MICROCHIP 47L16 EERAM DIAGNOSTIC ---\r\n");
    setI2CNormal();

    Wire.beginTransmission(ADDR_EERAM_CTRL);
    Wire.write(0x00);
    
    if (Wire.endTransmission() == 0 && Wire.requestFrom((uint8_t)ADDR_EERAM_CTRL, (uint8_t)1) == 1) {
        uint8_t statusReg = Wire.read();
        bool autoStoreEnabled = (statusReg & 0x02);
        bool arrayModified   = (statusReg & 0x01);
        
        Serial.printf("[STATUS 0x18] Raw Status: 0x%02X\r\n", statusReg);
        Serial.printf("  -> Auto-Store (VCAP) Enabled: %s\r\n", autoStoreEnabled ? "YES [OK]" : "NO [DISABLED]");
        Serial.printf("  -> SRAM Array Modified:      %s\r\n", arrayModified ? "YES" : "NO");
    } else {
        Serial.print("[FAIL] Could not read 47L16 Control Register at 0x18!\r\n");
        return;
    }

    uint16_t memAddr = 0x0000;
    const uint8_t EERAM_MAGIC[4] = {0x47, 0x4C, 0x31, 0x36};
    
    unsigned long startTime = micros();
    
    Wire.beginTransmission(ADDR_EERAM_DATA);
    Wire.write((uint8_t)(memAddr >> 8));
    Wire.write((uint8_t)(memAddr & 0xFF));
    Wire.write(EERAM_MAGIC[0]); Wire.write(EERAM_MAGIC[1]);
    Wire.write(EERAM_MAGIC[2]); Wire.write(EERAM_MAGIC[3]);
    Wire.endTransmission();
    
    unsigned long writeTime = micros() - startTime;
    Serial.printf("[SUCCESS] SRAM Burst Write completed in %lu us\r\n", writeTime);

    uint8_t readBuf[8] = {0};
    Wire.beginTransmission(ADDR_EERAM_DATA);
    Wire.write((uint8_t)(memAddr >> 8));
    Wire.write((uint8_t)(memAddr & 0xFF));
    Wire.endTransmission();
    
    if (Wire.requestFrom((uint8_t)ADDR_EERAM_DATA, (uint8_t)8) == 8) {
        for (int i = 0; i < 8; i++) readBuf[i] = Wire.read();
    }

    bool magicValid = (readBuf[0] == EERAM_MAGIC[0] && readBuf[1] == EERAM_MAGIC[1] &&
                       readBuf[2] == EERAM_MAGIC[2] && readBuf[3] == EERAM_MAGIC[3]);

    if (magicValid) {
        uint32_t powerCycleCount = ((uint32_t)readBuf[4] << 24) | ((uint32_t)readBuf[5] << 16) | 
                                   ((uint32_t)readBuf[6] << 8)  |  (uint32_t)readBuf[7];
        
        Serial.print("\r\n*** POWER-DOWN AUTO-STORE SUCCESSFUL! ***\r\n");
        Serial.printf(" -> Retained Power Cycles: %u\r\n", powerCycleCount);
        
        powerCycleCount++;
        Wire.beginTransmission(ADDR_EERAM_DATA);
        Wire.write((uint8_t)(memAddr >> 8));
        Wire.write((uint8_t)(memAddr & 0xFF));
        Wire.write(EERAM_MAGIC[0]); Wire.write(EERAM_MAGIC[1]);
        Wire.write(EERAM_MAGIC[2]); Wire.write(EERAM_MAGIC[3]);
        Wire.write((uint8_t)(powerCycleCount >> 24));
        Wire.write((uint8_t)(powerCycleCount >> 16));
        Wire.write((uint8_t)(powerCycleCount >> 8));
        Wire.write((uint8_t)(powerCycleCount & 0xFF));
        Wire.endTransmission();
    } else {
        Serial.print("\r\n[INFO] Initializing 47L16 SRAM Array...\r\n");
    }
}

void configureWiFiInteractive() {
    Serial.print("\r\n--- INTERACTIVE WI-FI NETWORK SETUP ---\r\n");
    Serial.print("Scanning 2.4GHz Wi-Fi spectrum...\r\n");
    
    int n = WiFi.scanNetworks();
    if (n == 0) {
        Serial.print("[ERROR] No Wi-Fi networks found!\r\n");
        return;
    }

    Serial.printf("\r\nFound %d networks:\r\n", n);
    for (int i = 0; i < min(n, 15); ++i) {
        Serial.printf(" [%d] %-24s (%d dBm) %s\r\n", 
                      i + 1, 
                      WiFi.SSID(i).c_str(), 
                      WiFi.RSSI(i), 
                      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "[OPEN]" : "[SECURE]");
    }

    Serial.print("\r\nEnter selection number (or 'q' to cancel): ");
    String choice = readSerialLine();
    
    if (choice.equalsIgnoreCase("q") || choice.toInt() < 1 || choice.toInt() > min(n, 15)) {
        Serial.print("\r\nWi-Fi setup canceled.\r\n");
        return;
    }

    int selectedIdx = choice.toInt() - 1;
    String selectedSSID = WiFi.SSID(selectedIdx);

    String password = "";
    if (WiFi.encryptionType(selectedIdx) != WIFI_AUTH_OPEN) {
        Serial.printf("\r\nEnter password for '%s': ", selectedSSID.c_str());
        password = readSerialLine();
    }

    Serial.printf("\r\nSaving credentials and joining network '%s' via DHCP...\r\n", selectedSSID.c_str());
    
    saveWiFiCredentials(selectedSSID, password);

    if (connectToSavedWiFi()) {
        Serial.printf("\r\n[SUCCESS] Connected to %s!\r\n", selectedSSID.c_str());
        Serial.print("DHCP IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.print("\r\n[FAIL] Could not join network. Check password/signal.\r\n");
    }
}

void testWiFiRF() {
    configureWiFiInteractive();
}

void printMenu() {
    Serial.print("\r\n=========================================\r\n");
    Serial.print("    EHM-MENTAL PCB TEST MENU (ESP32-C6)  \r\n");
    Serial.print("=========================================\r\n");
    Serial.print("[1] Live Analog Monitor (Currents & Voltages)\r\n");
    Serial.print("[2] ENCODER & OPTOCOUPLER PIN DEBUGGER\r\n");
    Serial.print("[3] I2C Device Scanner\r\n");
    Serial.print("[4] Toggle MCP23008 Outputs\r\n");
    Serial.print("[5] Run RS485 Loopback Test\r\n");
    Serial.print("[6] Test Buzzer (Play Melody)\r\n");
    Serial.print("[7] Run SD Card Diagnostics\r\n");
    Serial.print("[8] LIVE SENSOR DASHBOARD\r\n");
    Serial.print("[9] EERAM Read/Write Test (0x50)\r\n");
    Serial.print("[0] Scan, Select & Connect to Wi-Fi (DHCP)\r\n");
    Serial.print("[R] Reboot ESP32-C6\r\n");
    Serial.print("=========================================\r\n");
    Serial.print("Enter command: ");
}

void handleCLICommand(char cmd) {
    switch (cmd) {
        case '1': liveAnalogMonitor(); break;
        case '2': testEncoder(); break;
        case '3': scanI2C(); break;
        case '4': toggleMCPOutputs(); break;
        case '5': testRS485(); break;
        case '6': playLoudAlert(); break;
        case '7': testSDCard(); break;
        case '8': liveI2CDashboard(); break;
        case '9': testEERAM(); break;
        case '0': testWiFiRF(); break;
        case 'r':
        case 'R': 
            Serial.print("\r\nRebooting ESP32-C6...\r\n"); 
            delay(500); 
            ESP.restart(); 
            break;
    }

    if (cmd != '\n' && cmd != '\r') {
        delay(500);
        printMenu();
    }
}