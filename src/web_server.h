#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>

void initWebServer(const char* apSSID, const char* apPassword = NULL);
void handleWebServer();
void appendDiagLog(const String& logMsg);

bool connectToSavedWiFi();


#endif // WEB_SERVER_H