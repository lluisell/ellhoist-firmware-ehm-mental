#include "bluetooth.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Preferences.h>
#include <WiFi.h>
#include "persistence.h"
#include "telemetry_helper.h"

// --- UUID DEFINITIONS ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define MINUTE_CHAR_UUID    "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define SERIAL_CHAR_UUID    "d2f3bf96-9ca8-4d56-bc05-039401944883"
#define VERSION_CHAR_UUID   "e6882174-706f-4ad7-ad30-8040d7838584"
#define POWER_CHAR_UUID     "f7881234-5678-1234-5678-1234567890ab"
#define HW_ID_CHAR_UUID     "a842f1b3-6c8e-4a2b-9d1f-3e5c7a9b0d1e"
#define WIFI_SSID_CHAR_UUID "c0a80101-1234-4567-89ab-000000000001"
#define WIFI_PASS_CHAR_UUID "c0a80101-1234-4567-89ab-000000000002"

// New Characteristic UUID for Full Telemetry JSON
#define TELEMETRY_CHAR_UUID "e8721000-1234-4567-89ab-000000000001"

static Preferences prefs;

// --- BLE STATE & CHARACTERISTICS ---
static BLECharacteristic *pMinuteChar = nullptr;
static BLECharacteristic *pSerialChar = nullptr;
static BLECharacteristic *pPowerChar = nullptr;
static BLECharacteristic *pVersionChar = nullptr;
static BLECharacteristic *pHardwareChar = nullptr;
static BLECharacteristic *pWifiSsidChar = nullptr;
static BLECharacteristic *pWifiPassChar = nullptr;
static BLECharacteristic *pTelemetryChar = nullptr; // New characteristic handle
static BLEAdvertising *pAdvertising = nullptr;

bool deviceConnected = false;
static bool shouldSyncOnConnect = false;
static unsigned long connectionTime = 0;

extern bool tryingToConnectWIFI;

class MaintenanceCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        // Write logic handles settings
    }
};

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

void initBluetooth() {
    if (String(sysStats.serialNumber) == "000000") {
        Serial.println("Serial is 000000. Bluetooth initialization bypassed.");
        return;
    }

    String fullBtName = "ELLHoist_" + String(sysStats.serialNumber);
    BLEDevice::init(fullBtName.c_str());
    BLEDevice::setMTU(512); // Enables large payload transfers over BLE
    
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

    // Full Telemetry JSON Characteristic
    pTelemetryChar = pService->createCharacteristic(
        TELEMETRY_CHAR_UUID, 
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pTelemetryChar->setValue(buildTelemetryJSON().c_str());

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
            pMinuteChar->setValue(String(sysStats.motorRuntimeSec/60).c_str());
            pMinuteChar->notify();
        }
        notifyTelemetryUpdate(); // Push immediate full telemetry snapshot
        shouldSyncOnConnect = false; 
    }
}

void notifyMinutesUpdate() {    
    if (deviceConnected && pMinuteChar != nullptr) {
        pMinuteChar->setValue(String(sysStats.motorRuntimeSec/60).c_str());
        pMinuteChar->notify();
    }
}

void notifyTelemetryUpdate() {
    if (deviceConnected && pTelemetryChar != nullptr) {
        String json = buildTelemetryJSON();
        pTelemetryChar->setValue(json.c_str());
        pTelemetryChar->notify();
    }
}