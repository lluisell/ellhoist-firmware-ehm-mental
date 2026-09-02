#include "bluetooth.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include "persistence.h"

// --- UUID DEFINITIONS ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define MINUTE_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SERIAL_CHAR_UUID    "d2f3bf96-9ca8-4d56-bc05-039401944883"
#define VERSION_CHAR_UUID   "e6882174-706f-4ad7-ad30-8040d7838584"
#define POWER_CHAR_UUID     "f7881234-5678-1234-5678-1234567890ab"
#define HW_ID_CHAR_UUID     "a842f1b3-6c8e-4a2b-9d1f-3e5c7a9b0d1e"
#define WIFI_SSID_CHAR_UUID "c0a80101-1234-4567-89ab-000000000001"
#define WIFI_PASS_CHAR_UUID "c0a80101-1234-4567-89ab-000000000002"

static Preferences prefs;

// --- BLE STATE & CHARACTERISTICS ---
static BLECharacteristic *pMinuteChar = nullptr;
static BLECharacteristic *pSerialChar = nullptr;
static BLECharacteristic *pPowerChar = nullptr;
static BLECharacteristic *pVersionChar = nullptr;
static BLECharacteristic *pHardwareChar = nullptr;
static BLECharacteristic *pWifiSsidChar = nullptr;
static BLECharacteristic *pWifiPassChar = nullptr;
static BLEAdvertising *pAdvertising = nullptr;

bool deviceConnected = false;
static bool shouldSyncOnConnect = false;
static unsigned long connectionTime = 0;

extern bool tryingToConnectWIFI;

// --- MAINTENANCE WRITE CALLBACKS ---
class MaintenanceCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        String uuid = pCharacteristic->getUUID().toString().c_str();
        String val = pCharacteristic->getValue().c_str();

        /*if (uuid == SERIAL_CHAR_UUID && sysStats.serialNumber != nullptr) {
            *pDeviceSerial = val;
            pSerialChar->setValue(pDeviceSerial->c_str());
            preferences.putString("serial", val);
            Serial.println("Serial updated. Reboot to see new BT name: ELLHoist_" + *pDeviceSerial);

        } else 
         if (uuid == MINUTE_CHAR_UUID && val == "0" && pTotalMinutes != nullptr) {
            *pTotalMinutes = 0;
            pMinuteChar->setValue(String(*pTotalMinutes).c_str());
            preferences.putUInt("uptime", 0);

        } else if (uuid == POWER_CHAR_UUID && val == "0" && pPowerCycles != nullptr) {
            *pPowerCycles = 0;
            pPowerChar->setValue(String(*pPowerCycles).c_str());
            preferences.putUInt("power_cycles", 0);

        } else if (uuid == WIFI_SSID_CHAR_UUID && pWifiSSID != nullptr) {
            *pWifiSSID = val;
            preferences.putString("wifi_ssid", *pWifiSSID);
            Serial.println("BLE: WiFi SSID updated -> " + *pWifiSSID);

        } else if (uuid == WIFI_PASS_CHAR_UUID && pWifiPass != nullptr) {
            *pWifiPass = val;
            preferences.putString("wifi_pass", *pWifiPass);
            Serial.println("BLE: WiFi Password updated.");
            
            WiFi.disconnect();
            if (pWifiSSID != nullptr) {
                WiFi.begin(pWifiSSID->c_str(), pWifiPass->c_str());
            }
            tryingToConnectWIFI = true;
        }

        preferences.end();*/
    }
};

// --- SERVER CONNECT/DISCONNECT CALLBACKS ---
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        deviceConnected = true;
        shouldSyncOnConnect = true; 
        connectionTime = millis();
        Serial.println(">> Bluetooth Connected");
    }
    
    void onDisconnect(BLEServer* pServer) override {
        deviceConnected = false;
        shouldSyncOnConnect = false;
        Serial.println(">> Bluetooth Disconnected");
        BLEDevice::startAdvertising(); 
    }
};

// --- PUBLIC FUNCTIONS ---
void initBluetooth() {

    if (sysStats.serialNumber == "000000") {
        Serial.println("Serial is 000000. Bluetooth initialization bypassed.");
        return;
    }

    String fullBtName = "ELLHoist_" + String(sysStats.serialNumber);
    BLEDevice::init(fullBtName.c_str());
    
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);
    auto* cb = new MaintenanceCallbacks();
    
    pMinuteChar = pService->createCharacteristic(MINUTE_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    pMinuteChar->setCallbacks(cb);
    pMinuteChar->setValue(String(sysStats.motorRuntimeSec/60).c_str());

    pSerialChar = pService->createCharacteristic(SERIAL_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pSerialChar->setCallbacks(cb);
    pSerialChar->setValue(sysStats.serialNumber);

    pPowerChar = pService->createCharacteristic(POWER_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pPowerChar->setCallbacks(cb);
    pPowerChar->setValue(String(sysStats.br1Cycles).c_str());

    pVersionChar = pService->createCharacteristic(VERSION_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pVersionChar->setValue(FIRMWARE_VERSION.c_str());

    pHardwareChar = pService->createCharacteristic(HW_ID_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pHardwareChar->setValue(hwID.c_str());

    pWifiSsidChar = pService->createCharacteristic(WIFI_SSID_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    pWifiSsidChar->setCallbacks(cb);

    pWifiPassChar = pService->createCharacteristic(WIFI_PASS_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    pWifiPassChar->setCallbacks(cb);

    pService->start();

    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("Bluetooth Started as: " + fullBtName);
}

void handleBluetooth() {
    if (deviceConnected && shouldSyncOnConnect && (millis() - connectionTime > 2500)) {
        Serial.println(">> Sending initial sync to bluetooth");
        if (pMinuteChar != nullptr) {
            pMinuteChar->setValue(String((sysStats.motorRuntimeSec/60)).c_str());
            pMinuteChar->notify();
        }
        shouldSyncOnConnect = false; 
    }
}

void notifyMinutesUpdate() {    
    if (deviceConnected && pMinuteChar != nullptr) {
        pMinuteChar->setValue(String(sysStats.motorRuntimeSec/60).c_str());
        pMinuteChar->notify();
    }
}