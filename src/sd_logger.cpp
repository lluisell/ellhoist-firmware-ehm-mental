#include "sd_logger.h"
#include "web_server.h"
#include "telemetry_helper.h"
#include <SD.h>
#include <RTClib.h>

struct LogMessage {
    char timestamp[20];
    char event[80];
};

static QueueHandle_t logQueue = NULL;

static void sdWriterTask(void* pvParameters) {
    LogMessage msg;
    while (true) {
        if (xQueueReceive(logQueue, &msg, portMAX_DELAY) == pdTRUE) {
            DateTime now = rtc.now();
            char fileName[25];
            snprintf(fileName, sizeof(fileName), "/%04d-%02d-%02d.csv", now.year(), now.month(), now.day());

            File logFile = SD.open(fileName, FILE_APPEND);
            if (logFile) {
                logFile.printf("%s,%s\n", msg.timestamp, msg.event);
                logFile.close();
            }
        }
    }
}

void initSDLogger() {
    logQueue = xQueueCreate(32, sizeof(LogMessage));
    if (logQueue != NULL) {
        xTaskCreatePinnedToCore(sdWriterTask, "SDLoggerTask", 4096, NULL, 1, NULL, 0);
    }
}

void logEventAsync(const char* eventStr) {
    LogMessage msg;
    DateTime now = rtc.now();
    snprintf(msg.timestamp, sizeof(msg.timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    strncpy(msg.event, eventStr, sizeof(msg.event) - 1);
    msg.event[sizeof(msg.event) - 1] = '\0';

    // 1. Route to Live Web Console Buffer
    String consoleMsg = "[" + String(msg.timestamp) + "] " + String(eventStr) + "\r\n";
    appendDiagLog(consoleMsg);

    // 2. Route to SD Card Async Writer Task
    if (logQueue != NULL) {
        xQueueSend(logQueue, &msg, 0);
    }
}