#include "sd_logger.h"
#include "web_server.h"
#include "telemetry_helper.h"
#include <SD.h>
#include <RTClib.h>

extern RTC_DS3231 rtc;

struct LogMessage {
    char timestamp[20];
    char dateStr[11]; // Stores formatted "YYYY-MM-DD"
    char event[80];
};

static QueueHandle_t logQueue = NULL;

static void sdWriterTask(void* pvParameters) {
    LogMessage msg;
    while (true) {
        if (xQueueReceive(logQueue, &msg, portMAX_DELAY) == pdTRUE) {
            // NO I2C calls here! Uses the date string passed from logEventAsync
            char fileName[30];
            snprintf(fileName, sizeof(fileName), "/%s.csv", msg.dateStr);

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

    uint16_t y = now.year();
    uint8_t m = now.month();
    uint8_t d = now.day();

    // Sanity check: If RTC read glitched or month is 0, use safe fallback
    if (m < 1 || m > 12 || d < 1 || d > 31 || y < 2024) {
        snprintf(msg.dateStr, sizeof(msg.dateStr), "UNSYNCED");
        snprintf(msg.timestamp, sizeof(msg.timestamp), "2026-01-01 %02d:%02d:%02d",
                 now.hour(), now.minute(), now.second());
    } else {
        snprintf(msg.dateStr, sizeof(msg.dateStr), "%04d-%02d-%02d", y, m, d);
        snprintf(msg.timestamp, sizeof(msg.timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
                 y, m, d, now.hour(), now.minute(), now.second());
    }

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