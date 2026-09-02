#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

void initWebServer(const char* apSSID, const char* apPassword = NULL);
void handleWebServer();
void appendDiagLog(const String& logMsg);
void saveWiFiCredentials(const String& ssid, const String& password);
bool connectToSavedWiFi();

#endif // WEB_SERVER_H