// In GreenhouseM7_Main_Complete.ino

#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"
#include "ui.h"
#include <RPC.h>
#include "config.h"
#include "wifi_manager.h"   // Stays
#include "ntp_time.h"       // Stays (RTC aware)
#include "temperature_system.h" // Stays
#include "web_server.h"     // <<<< NEW INCLUDE
#include <Arduino.h>        // Good to have explicitly
#include "GreenhouseSettingsStruct.h" // Include the shared struct definition


Arduino_H7_Video Display(SCREEN_WIDTH, SCREEN_HEIGHT, GigaDisplayShield);
Arduino_GigaDisplayTouch TouchDetector;

unsigned long lastM4DataExchangeTime = 0;
const unsigned long M4_DATA_EXCHANGE_INTERVAL_MS = 10000;

// These globals are now 'extern' in web_server.cpp
float m4_reported_temperature = NAN;
int m4_vent_stage = -1;
bool m4_heater_state = false;
bool m4_shade_state = false;
bool m4_boost_state = false;

// Buffers for LVGL labels (main file manages these)
char tempLabelBufferMain[10];
char ventStatusBuffer[20];     // For LVGL ui_ventStatusLabel
char heaterStatusBuffer[15];   // For LVGL ui_heaterStatusLabel
char shadeStatusBuffer[15];    // For LVGL ui_shadeStatusLabel
char boostStatusBuffer[15];    // For LVGL ui_boostStatusLabel

// M7's cache of M4's configurable settings
GreenhouseSettings m4_settings_cache; // M7 will fill this from M4

void setup() {
    Serial.begin(115200);
    unsigned long setupStartTime = millis();
    while (!Serial && (millis() - setupStartTime < 1000));

    Serial.println("\nM7: Main System Setup Started.");

    if (RPC.begin()) {
        Serial.println("M7: RPC.begin() successful.");
    } else {
        Serial.println("M7: FATAL ERROR - RPC.begin() failed! Halting M7.");
        while(1) { delay(1000); }
    }
    delay(2000);

    Display.begin();
    TouchDetector.begin();
    ui_init(); // This should handle lv_init, buffer, disp/indev driver setup
    Serial.println("M7: UI Initialized.");

    initialize_wifi(); 

    if (is_wifi_connected()) {
        initialize_ntp_and_rtc(); // Uses RTC, then tries NTP
        initialize_web_server();  // <<<< Initialize web server
    } else {
        Serial.println("M7: WiFi not up. NTP and Web Server deferred.");
        if (ui_time) lv_label_set_text(ui_time, "--:--:--");
        if (ui_date) lv_label_set_text(ui_date, "No Net Svcs");
        initialize_ntp_and_rtc(); // Still init RTC part, it will try to use RTC time
    }

    initializeTemperatureSystem();
    Serial.println("M7: Setup complete.");
    lastM4DataExchangeTime = millis();
}

// exchangeDataWithM4AndRefreshUI() - now dedicated to LVGL UI updates
// (This function is mostly the same as your last working version for individual labels)
void exchangeDataWithM4AndRefreshUI_LVGL() {
    if (is_time_valid()) {
        int current_hour = get_current_hour();
        int current_minute = get_current_minute();
        if (current_hour != -1) { 
            RPC.call("receiveTimeFromM7", current_hour, current_minute);
        }
    }

    float temp_from_m4_val = NAN;
    try {
        auto temp_handle = RPC.call("getM4Temperature");
        temp_from_m4_val = temp_handle.as<float>();
        m4_reported_temperature = temp_from_m4_val;
    } catch (const std::exception& e) { m4_reported_temperature = NAN; }
    updateCurrentTemperatureFromM4(m4_reported_temperature);
    if (ui_tempLabel) { 
        if (!isnan(m4_reported_temperature)) {
            snprintf(tempLabelBufferMain, sizeof(tempLabelBufferMain), "%.1fC", m4_reported_temperature);
            lv_label_set_text(ui_tempLabel, tempLabelBufferMain);
        } else { lv_label_set_text(ui_tempLabel, "---C"); }
    }

    try { auto h = RPC.call("getM4VentStage"); m4_vent_stage = h.as<int>(); } 
    catch (const std::exception& e) { m4_vent_stage = -1; }
    if (ui_ventStatusLabel) { 
        if (m4_vent_stage == 0) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: Closed");
        else if (m4_vent_stage == 1) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: 25%%");
        else if (m4_vent_stage == 2) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: 50%%");
        else if (m4_vent_stage == 3) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: 100%%");
        else snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: N/A");
        lv_label_set_text(ui_ventStatusLabel, ventStatusBuffer);
    }

    try { auto h = RPC.call("getM4HeaterState"); m4_heater_state = h.as<bool>(); } 
    catch (const std::exception& e) { /* retain old */ }
    if (ui_heaterStatusLabel) { 
        snprintf(heaterStatusBuffer, sizeof(heaterStatusBuffer), "Heater: %s", m4_heater_state ? "ON" : "OFF");
        lv_label_set_text(ui_heaterStatusLabel, heaterStatusBuffer);
    }
    
    try { auto h = RPC.call("getM4ShadeState"); m4_shade_state = h.as<bool>(); }
    catch (const std::exception& e) { /* retain old */ }
    if (ui_shadeStatusLabel) { 
        snprintf(shadeStatusBuffer, sizeof(shadeStatusBuffer), "Shade: %s", m4_shade_state ? "Open" : "Closed");
        lv_label_set_text(ui_shadeStatusLabel, shadeStatusBuffer);
    }

    try { auto h = RPC.call("getM4BoostState"); m4_boost_state = h.as<bool>(); }
    catch (const std::exception& e) { /* retain old */ }
    if (ui_boostStatusLabel) { 
        snprintf(boostStatusBuffer, sizeof(boostStatusBuffer), "Boost: %s", m4_boost_state ? "Active" : "Off");
        lv_label_set_text(ui_boostStatusLabel, boostStatusBuffer);
    }

  // --- Fetch all configurable settings from M4 into m4_settings_cache ---
    Serial.println("M7: Fetching all settings from M4 for cache..."); // Keep this for now
    bool settingsFetchSuccess = false;
    // GreenhouseSettings temp_settings_holder; // Not needed if assigning directly to m4_settings_cache

    try {
        auto settings_handle = RPC.call("getM4AllSettings");
        // REMOVE: if (settings_handle) {
        m4_settings_cache = settings_handle.as<GreenhouseSettings>(); // Attempt conversion directly
        settingsFetchSuccess = true;
        Serial.println("M7: Successfully fetched all settings from M4.");
        // Debug print one value from the cache:
        Serial.print("M7: Cached Vent S1 Temp: "); Serial.println(m4_settings_cache.ventOpenTempStage1);

        // } else {
        //    Serial.println("M7: WARN - RPC.call for getM4AllSettings returned invalid handle.");
        // }
    } catch (const std::exception& e) {
        Serial.print("M7: WARN - Exception fetching all settings: "); Serial.println(e.what());
        // m4_settings_cache will retain its old values. Consider if you need to clear it or set a flag.
    }

}

unsigned long lastLoopHeartbeat = 0; // For M7 loop debug

void loop() {
    unsigned long currentTimeMs = millis();
    lv_timer_handler();

    if (currentTimeMs - lastLoopHeartbeat >= 60000) {
        Serial.println("M7 Loop Heartbeat...");
        lastLoopHeartbeat = currentTimeMs;
    }
    
    bool previousWifiStatus = is_wifi_connected();
    manage_wifi_connection(); 
    bool currentWifiStatus = is_wifi_connected();
    if (currentWifiStatus != previousWifiStatus) {
        notify_web_server_wifi_status(currentWifiStatus); 
        if (currentWifiStatus && !is_time_valid()){ 
            // Re-attempt NTP if WiFi just came up and we don't have valid time from RTC/NTP
            // initialize_ntp_and_rtc(); // This might be too aggressive,
                                      // update_time_management should handle retries.
            Serial.println("M7: WiFi reconnected, NTP will attempt sync via update_time_management.");
        }
    }
    
    handle_web_server_clients(); // <<<< Handle web server

    // Update time display from RTC/NTP
    update_time_management(); // This updates ui_time, ui_date and handles NTP syncs

    // Exchange data with M4 and update LVGL UI
    if (currentTimeMs - lastM4DataExchangeTime >= M4_DATA_EXCHANGE_INTERVAL_MS) {
        lastM4DataExchangeTime = currentTimeMs;
        exchangeDataWithM4AndRefreshUI_LVGL(); // Renamed for clarity
    }

    updateTemperatureSystem(); // Updates chart

    String m4_debug_buffer = "";
    while (RPC.available()) {
        m4_debug_buffer += (char)RPC.read();
    }
    if (m4_debug_buffer.length() > 0) {
        Serial.print(m4_debug_buffer);
    }

    delay(5);
}
