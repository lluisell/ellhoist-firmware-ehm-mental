#include "websocket.h"
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "persistence.h"

using namespace websockets;

// --- WSS CONFIG ---
static const char* ws_url = "wss://app.ellhoist.com/?apiKey=7f29a8c1e4d3b5a698210f4c7e92d1b855a9c0e3f21a4b7d6e8c90123abcd4ef";
static WebsocketsClient client;

static bool wasConnectedWiFi = false;
static unsigned long lastReconnectAttempt = 0;

// --- EVENT CALLBACK ---
static void onEventsCallback(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        Serial.println("WSS: Connected to ELLHOIST Cloud");
        uploadWebsocketData(); // Send initial telemetry packet upon connection
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        Serial.println("WSS: Connection Closed");
    }
}

// --- PUBLIC FUNCTIONS ---
void initWebSocket() {
    client.onEvent(onEventsCallback);
    client.setInsecure();
    Serial.println("WSS: Client initialized.");
}

void handleWebSocket() {
    if (WiFi.status() == WL_CONNECTED) {
        // Detect state transition to Wi-Fi connected
        if (!wasConnectedWiFi) { 
            wasConnectedWiFi = true;
            Serial.println("WIFI Connected to: " + getSSID());
        }

        if (client.available()) {
            client.poll();
        } else {
            // Reconnect WebSocket every 10 seconds if disconnected
            if (millis() - lastReconnectAttempt > 10000) {
                lastReconnectAttempt = millis();
                Serial.println("WSS: Attempting connection to Cloud API...");
                client.connect(ws_url);
            }
        }
    } else {
        wasConnectedWiFi = false;
    }
}

void uploadWebsocketData() {
    if (WiFi.status() == WL_CONNECTED && client.available()) {
        JsonDocument doc;
        doc["action"] = "WRITE";
        doc["identifierValue"] = sysStats.serialNumber;

        JsonObject payload = doc["payload"].to<JsonObject>();
        payload["hoist_sn"] = sysStats.serialNumber;
        payload["hardware_id"] = hwID;
        payload["runtime"] = sysStats.motorRuntimeSec / 60;
        payload["version"] = FIRMWARE_VERSION;
        payload["brake_cycles"] = sysStats.br1Cycles;
        payload["brake_1_cycles"] = sysStats.br1Cycles;
        payload["brake_2_cycles"] = sysStats.br2Cycles;
        payload["motor-runtime"] = sysStats.motorRuntimeSec;
        payload["device-runtime"] = sysStats.deviceRuntimeSec;

        String jsonPayload;
        serializeJson(doc, jsonPayload);

        client.send(jsonPayload);
        Serial.println("Telemetry Sent: " + jsonPayload);
    } else {
        Serial.println("WSS: Upload skipped (WebSocket offline)");
    }
}