// settings_storage.h
#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H

#include <Arduino.h>

// GreenhouseSettings struct remains the same
typedef struct {
    uint32_t magicNumber;
    uint16_t settingsVersion;
    uint16_t checksum;
    float ventOpenTempStage1;
    float ventOpenTempStage2;
    float ventOpenTempStage3;
    float heatSetTempDay;
    float heatSetTempNight;
    float heatBoostTemp;
    float hysteresis;
    uint8_t dayStartHour;
    uint8_t dayStartMinute;
    uint8_t nightStartHour;
    uint8_t nightStartMinute;
    uint8_t boostStartHour;
    uint8_t boostStartMinute;
    uint16_t boostDurationMinutes;
    uint8_t shadeOpenHour;
    uint8_t shadeOpenMinute;
    uint8_t shadeCloseHour;
    uint8_t shadeCloseMinute;
} GreenhouseSettings;

// These will be defined in the M4 main .ino file
extern GreenhouseSettings currentSettings;
extern bool settingsDirty;

// Function declarations
void initialize_settings_flashiap(); // Renamed to be specific
void save_settings_if_dirty();
void mark_settings_dirty();
void load_default_settings();

#endif // SETTINGS_STORAGE_H
