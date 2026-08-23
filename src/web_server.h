#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

// --- FUNCTION PROTOTYPES ---
void initWebServer(const char* apSSID, const char* apPassword = NULL);
void handleWebServer();
void appendDiagLog(const String& logMsg);

// NVS Wi-Fi Storage Helpers
bool connectToSavedWiFi();
void saveWiFiCredentials(const String& ssid, const String& password);
void clearWiFiCredentials();

#endif // WEB_SERVER_H