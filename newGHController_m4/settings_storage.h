// settings_storage.h
#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H
#include "GreenhouseSettingsStruct.h" // Include the shared struct definition
#include <Arduino.h>

// These will be defined in the M4 main .ino file
extern GreenhouseSettings currentSettings;
extern bool settingsDirty;

// Function declarations
void initialize_settings_flashiap(); // Renamed to be specific
void save_settings_if_dirty();
void mark_settings_dirty();
void load_default_settings();

#endif // SETTINGS_STORAGE_H
