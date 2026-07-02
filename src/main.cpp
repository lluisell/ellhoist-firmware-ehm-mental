// =========================================================================
// ESP32-C6 HARDWARE VALIDATION TEST SCRIPT (WITH I2C DIAGNOSTICS)
// =========================================================================
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

// --- Pin Definitions ---
#define PIN_PHASE_L1      0   // Strapping pin - do not pull LOW at boot!
#define PIN_PHASE_L2      1
#define PIN_PHASE_L3      2

#define PIN_SD_CS         4
#define PIN_SD_MOSI       5
#define PIN_SD_SCK        6
#define PIN_SD_MISO       7

// I2C Bus Pins
#define PIN_I2C_SCL       8   // Strapping pin
#define PIN_I2C_SDA       9   // Strapping pin (Must be HIGH at boot - use pull-ups!)

#define PIN_SIG_REVERSE   10
#define PIN_SIG_FORWARD   11

#define PIN_UART0_MOD     15  
#define PIN_UART0_TX      16
#define PIN_UART0_RX      17

#define PIN_MOTOR_ON      18
#define PIN_MOTOR_DIR     19  

#define PIN_UART1_MOD     20  
#define PIN_UART1_TX      21
#define PIN_UART1_RX      22
#define PIN_ENCODER       23

// --- I2C Target Addresses ---
#define ADDR_INA226       0x40  // Address 0.0 (A1=GND, A0=GND)
#define ADDR_DS3231       0x68  // Fixed RTC Address

volatile unsigned long encoderPulseCount = 0;

void IRAM_ATTR handleEncoderPulse() {
    encoderPulseCount++;
}

// Helper function to scan a specific I2C address
bool checkI2CDevice(byte address) {
    Wire.beginTransmission(address);
    return (Wire.endTransmission() == 0);
}
void handleSerialAPI() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input == "RST") {
            Serial.println(">> REBOOTING DEVICE...");
            Serial.flush(); 
            delay(500);     
            ESP.restart();  
        }
        //preferences.begin("hoist-data", false);
        if (input.startsWith("SET_SN=")) {
            //deviceSerial = input.substring(7);
            //preferences.putString("serial", deviceSerial);
            Serial.println("OK:SN_SET");
        } else if (input.startsWith("SET_MIN=")) {
            //totalMinutes = input.substring(8).toInt();
            //preferences.putUInt("uptime", totalMinutes);
            //Console.println("OK:MIN_SET");
        } else if (input.startsWith("SET_PWR=")) {
            //powerCycles = input.substring(8).toInt();
            //preferences.putUInt("power_cycles", powerCycles);
            //Console.println("OK:PWR_SET");
        } else if (input.startsWith("SET_SSID=")) {
            //wifiSSID = input.substring(9);
            //preferences.putString("wifi_ssid", wifiSSID);
            //Console.println("OK:SSID_SET");
        } else if (input.startsWith("SET_PASS=")) {
            //wifiPass = input.substring(9);
            //preferences.putString("wifi_pass", wifiPass);
            //Console.println("OK:PASS_SET");
        } else if (input == "INFO") {
            Serial.println("--- START_INFO ---");
            //Console.println("Serial: " + deviceSerial);
            //Console.println("HWID: " + hardwareID);
            ////Console.println("Version: " + FIRMWARE_VERSION); 
            //Console.printf("Runtime: %u\n", totalMinutes);
            //Console.printf("Power Ups: %u\n", powerCycles);
            //Console.println("WiFi SSID: " + (wifiSSID == "" || wifiSSID == "null" ? "NOT SET" : wifiSSID));
            Serial.println("--- END_INFO ---");
        }
        //preferences.end();
        //updateDisplay();
    }
}

void setup() {
    // Initialize Native USB CDC Serial
    Serial.begin(115200);
    delay(2000); 
    
    Serial.println("\n==================================================");
    Serial.println("         ESP32-C6 HARDWARE TEST INITIALIZED       ");
    Serial.println("==================================================");

    // 1. Initialize Control Outputs
    pinMode(PIN_MOTOR_ON, OUTPUT);
    pinMode(PIN_MOTOR_DIR, OUTPUT);
    digitalWrite(PIN_MOTOR_ON, LOW);
    digitalWrite(PIN_MOTOR_DIR, LOW);

    pinMode(PIN_UART0_MOD, OUTPUT);
    pinMode(PIN_UART1_MOD, OUTPUT);
    digitalWrite(PIN_UART0_MOD, LOW); 
    digitalWrite(PIN_UART1_MOD, LOW); 

    // 2. Initialize Inputs
    pinMode(PIN_PHASE_L1, INPUT_PULLUP);
    pinMode(PIN_PHASE_L2, INPUT_PULLUP);
    pinMode(PIN_PHASE_L3, INPUT_PULLUP);
    pinMode(PIN_SIG_FORWARD, INPUT_PULLUP);
    pinMode(PIN_SIG_REVERSE, INPUT_PULLUP);
    
    pinMode(PIN_ENCODER, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENCODER), handleEncoderPulse, FALLING);

    // 3. Initialize I2C Bus on GPIO 8 and 9
    Serial.println("[STATUS] Initializing I2C Master Bus...");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000); // 100kHz standard mode

    
    Serial.println("[SUCCESS] GPIO allocations configured.\n");

    Serial.println("\n==================================================");
    Serial.println("            SD CARD HARDWARE DIAGNOSTIC           ");
    Serial.println("==================================================");

    // 1. Remap the default global SPI bus to your exact pins
    // Arguments: SCK, MISO, MOSI, SS
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    pinMode(PIN_SD_CS, OUTPUT);

    // 2. Pass the standard global SPI object straight into the SD library
    Serial.print("[STATUS] Initializing SD card on SPI Bus... ");
    
    if (!SD.begin(PIN_SD_CS, SPI, 4000000, "/sd", 5, true)) {
        Serial.println("\n[ERROR] SD Card mount failed!");
        Serial.println("  --> Check if card is inserted correctly.");
        Serial.println("  --> Check for loose connections on pins 4, 5, 6, 7.");
        Serial.println("  --> Ensure the card is formatted as FAT16 or FAT32.");
        return;
    }
    Serial.println("SUCCESS!");

    // 3. Print Card Metadata
    uint8_t cardType = SD.cardType();
    Serial.print(" -> Card Type: ");
    if (cardType == CARD_MMC)  Serial.println("MMC");
    else if (cardType == CARD_SD)   Serial.println("SDSC");
    else if (cardType == CARD_SDHC) Serial.println("SDHC");
    else                            Serial.println("UNKNOWN");

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf(" -> Card Size: %llu MB\n", cardSize);

    // 4. Run Read/Write Diagnostics
    Serial.println("\n[STATUS] Starting filesystem read/write test...");
    
    // Test Writing
    File testFile = SD.open("/test_log.txt", FILE_WRITE);
    if (!testFile) {
        Serial.println("[ERROR] Failed to open file for writing!");
        return;
    }
    
    Serial.println(" -> Writing data packet to '/test_log.txt'...");
    testFile.println("ESP32-C6 SD Hardware Test Log");
    testFile.println("Status: Functional");
    testFile.printf("Uptime: %lu ms\n", millis());
    testFile.close();
    Serial.println(" -> Write operation complete.");

    // Test Reading
    testFile = SD.open("/test_log.txt", FILE_READ);
    if (!testFile) {
        Serial.println("[ERROR] Failed to open file for reading!");
        return;
    }

    Serial.println("\n -> Reading file contents back:");
    Serial.println("--------------------------------------");
    while (testFile.available()) {
        Serial.write(testFile.read());
    }
    Serial.println("--------------------------------------");
    testFile.close();

    // Cleanup - removes the test file
    SD.remove("/test_log.txt");
    Serial.println("[SUCCESS] SD Card passed health checks. Test file cleaned up.");
}

void loop() {
    static unsigned long lastExecutionTime = 0;
    static bool testMotorState = false;
    static bool testDirectionState = false;

    
    handleSerialAPI();

    if (millis() - lastExecutionTime >= 10000) {
        lastExecutionTime = millis();

        // Toggle Motor Outputs
        testMotorState = !testMotorState;
        if (!testMotorState) {
            testDirectionState = !testDirectionState;
        }
        
        digitalWrite(PIN_MOTOR_ON, testMotorState ? HIGH : LOW);
        digitalWrite(PIN_MOTOR_DIR, testDirectionState ? HIGH : LOW);

        // Read Digital Inputs
        int l1_state = digitalRead(PIN_PHASE_L1);
        int l2_state = digitalRead(PIN_PHASE_L2);
        int l3_state = digitalRead(PIN_PHASE_L3);
        int fwd_state = digitalRead(PIN_SIG_FORWARD);
        int rev_state = digitalRead(PIN_SIG_REVERSE);

        // Scan I2C Devices live
        bool inaDetected = checkI2CDevice(ADDR_INA226);
        bool rtcDetected = checkI2CDevice(ADDR_DS3231);

        // Print Telemetry Report over USB Serial
        Serial.println("--------------------------------------------------");
        Serial.print("TIMESTAMP: "); Serial.print(millis() / 1000); Serial.println("s");
        
        // Output Checks
        Serial.print(" -> OUTPUTS   | MOTOR: "); 
        Serial.print(testMotorState ? "ON " : "OFF");
        Serial.print(" | DIRECTION: "); 
        Serial.println(testDirectionState ? "REVERSE" : "FORWARD");

        // Input Checks
        Serial.print(" -> PHASE DET | L1: "); Serial.print(l1_state);
        Serial.print(" | L2: "); Serial.print(l2_state);
        Serial.print(" | L3: "); Serial.println(l3_state);

        // Signal System Checks
        Serial.print(" -> SIGNALS   | FORWARD: "); 
        Serial.print(fwd_state == LOW ? "ACTIVE" : "IDLE");
        Serial.print(" | REVERSE: "); 
        Serial.println(rev_state == LOW ? "ACTIVE" : "IDLE");

        // Encoder Output
        Serial.print(" -> ENCODER   | Pulse Count: "); 
        Serial.println(encoderPulseCount);

        // I2C Bus Diagnostic Output
        Serial.print(" -> I2C BUS   | INA226 (0x40): "); 
        Serial.print(inaDetected ? "ONLINE [OK]" : "OFFLINE [ERROR]");
        Serial.print(" | DS3231MZ (0x68): "); 
        Serial.println(rtcDetected ? "ONLINE [OK]" : "OFFLINE [ERROR]");
    }
}