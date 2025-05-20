// settings_storage.cpp
#include "settings_storage.h"
#include <Arduino.h>
#include <FlashIAPBlockDevice.h>
#include "FlashIAPLimits.h"
#include <RPC.h>

// Global instances are expected to be in the main M4 .ino file
// extern GreenhouseSettings currentSettings;
// extern bool settingsDirty;

const uint32_t SETTINGS_MAGIC_NUMBER = 0xCAFEF010;
const uint16_t CURRENT_SETTINGS_VERSION = 4;

FlashIAPBlockDevice* settingsBlockDevice = nullptr;
uint32_t settings_storage_size = 0;
uint32_t program_block_size_internal = 0;
uint32_t erase_block_size_internal = 0;

const unsigned long SETTINGS_SAVE_DEBOUNCE_MS = 5000;
unsigned long lastSettingChangeTime = 0;

uint16_t calculate_checksum(const GreenhouseSettings* settings) {
    uint16_t cs = 0;
    const uint8_t* p = (const uint8_t*)settings;
    size_t data_start_offset = offsetof(GreenhouseSettings, ventOpenTempStage1);
    for (size_t i = data_start_offset; i < sizeof(GreenhouseSettings); ++i) {
        cs += p[i];
    }
    return cs;
}

void load_default_settings() {
    RPC.println("M4-FlashIAP: Loading default settings into RAM.");
    currentSettings.magicNumber = SETTINGS_MAGIC_NUMBER;
    currentSettings.settingsVersion = CURRENT_SETTINGS_VERSION;
    currentSettings.ventOpenTempStage1 = 25.0f;
    currentSettings.ventOpenTempStage2 = 27.5f;
    currentSettings.ventOpenTempStage3 = 30.0f;
    currentSettings.heatSetTempDay = 20.5f;
    currentSettings.heatSetTempNight = 18.5f;
    currentSettings.heatBoostTemp = 22.5f;
    currentSettings.hysteresis = 1.0f;
    currentSettings.dayStartHour = 7; currentSettings.dayStartMinute = 0;
    currentSettings.nightStartHour = 19; currentSettings.nightStartMinute = 0;
    currentSettings.boostStartHour = 6; currentSettings.boostStartMinute = 30;
    currentSettings.boostDurationMinutes = 60;
    currentSettings.shadeOpenHour = 8; currentSettings.shadeOpenMinute = 15;
    currentSettings.shadeCloseHour = 17; currentSettings.shadeCloseMinute = 45;
    currentSettings.checksum = calculate_checksum(&currentSettings); // CORRECTED
}

static bool actual_save_to_flashiap() {
    if (!settingsBlockDevice) {
        RPC.println("M4-FlashIAP: ERROR - Block device not initialized, cannot save!");
        return false;
    }

    RPC.println("M4-FlashIAP: Saving current settings to Flash...");
    currentSettings.checksum = calculate_checksum(&currentSettings); // CORRECTED

    const auto structSize = sizeof(GreenhouseSettings);
    const unsigned int requiredProgramBlocks = ceil(structSize / (float)program_block_size_internal);
    const auto dataSizeToProgram = requiredProgramBlocks * program_block_size_internal;

    if (dataSizeToProgram > settings_storage_size) {
        RPC.println("M4-FlashIAP: ERROR - Settings data size exceeds allocated flash space!");
        return false;
    }

    uint8_t write_buffer[dataSizeToProgram];
    memset(write_buffer, 0xFF, dataSizeToProgram);
    memcpy(write_buffer, &currentSettings, structSize); // CORRECTED

    const unsigned int requiredEraseBlocks = ceil(dataSizeToProgram / (float)erase_block_size_internal);
    bd_size_t eraseSizeNeeded = requiredEraseBlocks * erase_block_size_internal;
    
    RPC.println("M4-FlashIAP: Erasing " + String((unsigned long)eraseSizeNeeded) + " bytes..."); // CORRECTED
    int erase_result = settingsBlockDevice->erase(0, eraseSizeNeeded);
    if (erase_result != 0) {
        RPC.println("M4-FlashIAP: ERROR - Flash erase failed! Code: " + String(erase_result));
        return false;
    }
    RPC.println("M4-FlashIAP: Erase successful.");

    RPC.println("M4-FlashIAP: Programming " + String((unsigned long)dataSizeToProgram) + " bytes..."); // CORRECTED
    int program_result = settingsBlockDevice->program(write_buffer, 0, dataSizeToProgram);
    if (program_result != 0) {
        RPC.println("M4-FlashIAP: ERROR - Flash program failed! Code: " + String(program_result));
        return false;
    }

    RPC.println("M4-FlashIAP: Settings successfully written to Flash.");
    return true;
}

void initialize_settings_flashiap() {
    RPC.println("M4-FlashIAP: Initializing settings module...");

    FlashIAPLimits limits = getFlashIAPLimits();
    if (limits.available_size == 0 || limits.start_address == 0) {
        RPC.println("M4-FlashIAP: ERROR - Could not get valid FlashIAP limits!");
        load_default_settings();
        settingsDirty = false;
        return;
    }

    RPC.println("M4-FlashIAP: Total Flash: " + String(limits.flash_size / 1024.0f) + " KB"); // Use .0f for float division
    RPC.println("M4-FlashIAP: App Ends: 0x" + String(FLASHIAP_APP_ROM_END_ADDR, HEX));
    RPC.println("M4-FlashIAP: Storage Start: 0x" + String(limits.start_address, HEX));
    RPC.println("M4-FlashIAP: Storage Avail: " + String(limits.available_size / 1024.0f) + " KB"); // Use .0f

    FlashIAP tempFlash;
    if (tempFlash.init() != 0) {
        RPC.println("M4-FlashIAP: ERROR - Failed to init temp FlashIAP!");
        load_default_settings();
        return;
    }
    uint32_t flashiap_sector_size = tempFlash.get_sector_size(limits.start_address);
    tempFlash.deinit();

    if (flashiap_sector_size == 0 || flashiap_sector_size == MBED_FLASH_INVALID_SIZE) {
        RPC.println("M4-FlashIAP: ERROR - Could not get valid sector size!");
        load_default_settings();
        return;
    }
    RPC.println("M4-FlashIAP: Underlying Sector Size: " + String((unsigned long)flashiap_sector_size) + " bytes"); // CORRECTED

    const auto structSize = sizeof(GreenhouseSettings);
    const unsigned int requiredSectorsForStruct = ceil(structSize / (float)flashiap_sector_size);
    settings_storage_size = requiredSectorsForStruct * flashiap_sector_size;

    if (settings_storage_size == 0) {
        RPC.println("M4-FlashIAP: ERROR - Calculated settings_storage_size is zero!");
        load_default_settings(); return;
    }
    if (settings_storage_size > limits.available_size) {
        RPC.println("M4-FlashIAP: ERROR - Not enough flash for settings!");
        RPC.println("M4-FlashIAP: Need " + String((unsigned long)settings_storage_size) + ", Got " + String((unsigned long)limits.available_size)); // CORRECTED
        load_default_settings(); return;
    }
    RPC.println("M4-FlashIAP: Allocating " + String((unsigned long)settings_storage_size) + " bytes for BD."); // CORRECTED

    if (settingsBlockDevice) { delete settingsBlockDevice; settingsBlockDevice = nullptr; }
    settingsBlockDevice = new FlashIAPBlockDevice(limits.start_address, settings_storage_size);
    
    int init_result = settingsBlockDevice->init();
    if (init_result != 0) {
        RPC.println("M4-FlashIAP: ERROR - BD init failed! Code: " + String(init_result));
        delete settingsBlockDevice; settingsBlockDevice = nullptr;
        load_default_settings(); return;
    }
    RPC.println("M4-FlashIAP: BD initialized.");

    program_block_size_internal = settingsBlockDevice->get_program_size();
    erase_block_size_internal = settingsBlockDevice->get_erase_size();

    if (program_block_size_internal == 0 || erase_block_size_internal == 0) {
        RPC.println("M4-FlashIAP: ERROR - BD returned invalid program/erase sizes!");
        settingsBlockDevice->deinit(); delete settingsBlockDevice; settingsBlockDevice = nullptr;
        load_default_settings(); return;
    }
    RPC.println("M4-FlashIAP: BD Program Size: " + String((unsigned long)program_block_size_internal) + " bytes"); // CORRECTED
    RPC.println("M4-FlashIAP: BD Erase Size: " + String((unsigned long)erase_block_size_internal) + " bytes");   // CORRECTED

    GreenhouseSettings tempSettings;
    const unsigned int requiredProgramBlocksForRead = ceil(structSize / (float)program_block_size_internal);
    const auto dataSizeToRead = requiredProgramBlocksForRead * program_block_size_internal;
    uint8_t read_buffer[dataSizeToRead];

    int read_result = settingsBlockDevice->read(read_buffer, 0, dataSizeToRead);
    if (read_result == 0) {
        memcpy(&tempSettings, read_buffer, structSize);
        if (tempSettings.magicNumber == SETTINGS_MAGIC_NUMBER &&
            tempSettings.settingsVersion == CURRENT_SETTINGS_VERSION &&
            tempSettings.checksum == calculate_checksum(&tempSettings)) { // CORRECTED
            currentSettings = tempSettings;
            RPC.println("M4-FlashIAP: Valid settings loaded from Flash.");
        } else {
            RPC.println("M4-FlashIAP: Flash data invalid/outdated. Loading defaults and saving.");
            load_default_settings();
            if (!actual_save_to_flashiap()) {
                 RPC.println("M4-FlashIAP: ERROR - Failed to save default settings after finding invalid data!");
            }
        }
    } else {
        RPC.println("M4-FlashIAP: Failed to read from Flash (Code: " + String(read_result) + "). Loading defaults and saving.");
        load_default_settings();
        if (!actual_save_to_flashiap()) {
            RPC.println("M4-FlashIAP: ERROR - Failed to save default settings after read failure!");
        }
    }
    settingsDirty = false;
    lastSettingChangeTime = millis();
}

void mark_settings_dirty() {
    if (!settingsBlockDevice) {
        RPC.println("M4-FlashIAP: Settings marked dirty, but BD not ready.");
    }
    if (!settingsDirty) {
        RPC.println("M4-FlashIAP: Settings marked dirty. Will save after debounce.");
    }
    settingsDirty = true;
    lastSettingChangeTime = millis();
}

void save_settings_if_dirty() {
    if (settingsDirty && settingsBlockDevice && (millis() - lastSettingChangeTime >= SETTINGS_SAVE_DEBOUNCE_MS)) {
        if (actual_save_to_flashiap()) {
            settingsDirty = false;
        } else {
            RPC.println("M4-FlashIAP: Save attempt failed. Settings remain dirty.");
            lastSettingChangeTime = millis();
        }
    }
}
