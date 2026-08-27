#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>

void initSDLogger();
void logEventAsync(const char* eventStr);

#endif // SD_LOGGER_H