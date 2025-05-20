// GreenhouseM7_Main_Complete.ino with DEBUG prints

// ... (all includes and global hardware objects as before) ...
#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"
#include "ui.h"
#include <RPC.h>
#include "config.h"
#include "wifi_manager.h"
#include "ntp_time.h"
#include "temperature_system.h"


Arduino_H7_Video Display(SCREEN_WIDTH, SCREEN_HEIGHT, GigaDisplayShield);
Arduino_GigaDisplayTouch TouchDetector;

// ... (M7 Global Variables for M4 Data & UI as before) ...
unsigned long lastM4DataExchangeTime = 0;
const unsigned long M4_DATA_EXCHANGE_INTERVAL_MS = 2000;
float m4_reported_temperature = NAN;
int m4_vent_stage = -1;
bool m4_heater_state = false;
bool m4_shade_state = false;
bool m4_boost_state = false;

void setup() {
    Serial.begin(115200);

    unsigned long setupStartTime = millis();
    // Make this wait very short for initial debugging. If Serial never connects, we want to proceed.
    while (!Serial && (millis() - setupStartTime < 1000)); // Shortened wait

    Serial.println("\n\n\nDEBUG: M7: Setup Phase 0 - Serial Initialized."); delay(100);

    Serial.println("DEBUG: M7: Setup Phase 1 - Initializing RPC and booting M4..."); delay(100);
    if (RPC.begin()) {
        Serial.println("DEBUG: M7: RPC.begin() successful. M4 should be running."); delay(100);
    } else {
        Serial.println("DEBUG: M7: FATAL ERROR - RPC.begin() failed! Halting M7."); delay(100);
        while(1) { delay(1000); } // Halt
    }
    Serial.println("DEBUG: M7: Delaying for M4 RPC init..."); delay(100);
    delay(2000); // Give M4 time.
    Serial.println("DEBUG: M7: M4 init delay complete."); delay(100);


    if (RPC.begin()) { // This will now attempt to start the M4 code you just flashed
        Serial.println("M7: RPC.begin() successful. M4 should be booting/running its flashed code.");
    } else {
        Serial.println("M7: FATAL ERROR - RPC.begin() failed!");
        // Potentially loop forever or indicate error clearly if this happens
        // while(1) { delay(100); }
    }

    Serial.println("M7: M4 core should be booting now.");
    delay(1000); // Give M4 some time to initialize
    
    // --- Hardware and LVGL Initialization ---
    Serial.println("DEBUG: M7: Setup Phase 2 - Display.begin()..."); delay(100);
    Display.begin();
    Serial.println("DEBUG: M7: Display.begin() DONE."); delay(100);

    Serial.println("DEBUG: M7: Setup Phase 3 - TouchDetector.begin()..."); delay(100);
    TouchDetector.begin();
    Serial.println("DEBUG: M7: TouchDetector.begin() DONE."); delay(100);

    Serial.println("DEBUG: M7: Setup Phase 5 - ui_init()..."); delay(100);
    ui_init(); 
    Serial.println("DEBUG: M7: UI Initialized (ui_init() DONE)."); delay(100);

    // --- Initialize Custom Modules ---
    Serial.println("DEBUG: M7: Setup Phase 6 - initialize_wifi()..."); delay(100);
    initialize_wifi(); // From wifi_manager.h
    Serial.println("DEBUG: M7: initialize_wifi() DONE."); delay(100);

    Serial.println("DEBUG: M7: Setup Phase 7 - initialize_ntp()..."); delay(100);
    initialize_ntp_and_rtc(); //  From ntp_time.h
    Serial.println("DEBUG: M7: initialize_ntp() DONE."); delay(100);

    Serial.println("DEBUG: M7: Setup Phase 8 - initializeTemperatureSystem()..."); delay(100);
    initializeTemperatureSystem(); // From temperature_system.h
    Serial.println("DEBUG: M7: initializeTemperatureSystem() DONE."); delay(100);

    Serial.println("DEBUG: M7: Setup complete. Entering loop()."); delay(100);
    lastM4DataExchangeTime = millis();
}

// exchangeDataWithM4AndRefreshUI() - using the simplified try-catch version
char tempLabelBufferMain[10]; // Already declared
char ventStatusBuffer[20];
char heaterStatusBuffer[15];
char shadeStatusBuffer[15];
char boostStatusBuffer[15];

void exchangeDataWithM4AndRefreshUI() {
    // 1. Send Current Time to M4 (if NTP is synced) - (No change to this part)
    if (is_time_valid()) {
        int current_hour = get_current_hour();
        int current_minute = get_current_minute();
        if (current_hour != -1) { // Valid time
            RPC.call("receiveTimeFromM7", current_hour, current_minute);
        }
    }

    // 2. Get Data from M4 and Update Individual UI Labels

    // ** GET & UPDATE M4 TEMPERATURE **
    float temp_from_m4_val = NAN;
    try {
        auto temp_handle = RPC.call("getM4Temperature");
        temp_from_m4_val = temp_handle.as<float>();
        m4_reported_temperature = temp_from_m4_val;
    } catch (const std::exception& e) {
        m4_reported_temperature = NAN;
    }
    updateCurrentTemperatureFromM4(m4_reported_temperature);
    if (ui_tempLabel) { // Simplified check
        if (!isnan(m4_reported_temperature)) {
            snprintf(tempLabelBufferMain, sizeof(tempLabelBufferMain), "%.1fC", m4_reported_temperature);
            lv_label_set_text(ui_tempLabel, tempLabelBufferMain);
        } else {
            lv_label_set_text(ui_tempLabel, "---C");
        }
    }

    // ** GET & UPDATE M4 VENT STAGE **
    int vent_stage_val = -1;
    try {
        auto vent_handle = RPC.call("getM4VentStage");
        vent_stage_val = vent_handle.as<int>();
        m4_vent_stage = vent_stage_val;
    } catch (const std::exception& e) { m4_vent_stage = -1; }

    if (ui_ventStatusLabel) { // Simplified check
        if (m4_vent_stage == 0) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: Closed");
        else if (m4_vent_stage == 1) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: 25%%");
        else if (m4_vent_stage == 2) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: 50%%");
        else if (m4_vent_stage == 3) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: 100%%");
        else snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: N/A");
        lv_label_set_text(ui_ventStatusLabel, ventStatusBuffer);
    }

    // ** GET & UPDATE M4 HEATER STATE **
    try {
        auto heater_handle = RPC.call("getM4HeaterState");
        m4_heater_state = heater_handle.as<bool>();
    } catch (const std::exception& e) { /* m4_heater_state retains previous on error */ }

    if (ui_heaterStatusLabel) { // Simplified check
        snprintf(heaterStatusBuffer, sizeof(heaterStatusBuffer), "Heater: %s", m4_heater_state ? "ON" : "OFF");
        lv_label_set_text(ui_heaterStatusLabel, heaterStatusBuffer);
    }

    // ** GET & UPDATE M4 SHADE STATE **
    try {
        auto shade_handle = RPC.call("getM4ShadeState");
        m4_shade_state = shade_handle.as<bool>();
    } catch (const std::exception& e) { /* m4_shade_state retains previous on error */ }

    if (ui_shadeStatusLabel) { // Simplified check
        snprintf(shadeStatusBuffer, sizeof(shadeStatusBuffer), "Shade: %s", m4_shade_state ? "Open" : "Closed");
        lv_label_set_text(ui_shadeStatusLabel, shadeStatusBuffer);
    }

    // ** GET & UPDATE M4 BOOST STATE **
    try {
        auto boost_handle = RPC.call("getM4BoostState");
        m4_boost_state = boost_handle.as<bool>();
    } catch (const std::exception& e) { /* m4_boost_state retains previous on error */ }

    if (ui_boostStatusLabel) { // Simplified check
        snprintf(boostStatusBuffer, sizeof(boostStatusBuffer), "Boost: %s", m4_boost_state ? "Active" : "Off");
        lv_label_set_text(ui_boostStatusLabel, boostStatusBuffer);
    }
}


unsigned long lastLoopHeartbeat = 0;

void loop() {

    unsigned long currentTimeMs = millis();
    lv_timer_handler();

    // Simple M7 heartbeat to Serial to see if loop is running
    if (currentTimeMs - lastLoopHeartbeat >= 5000) {
        Serial.println("DEBUG: M7: Loop is running..."); delay(100);
        lastLoopHeartbeat = currentTimeMs;
    }

    manage_wifi_connection();

    if (!is_time_valid()) {
        if (ui_time) lv_label_set_text(ui_time, "--:--:--");
        if (ui_date) lv_label_set_text(ui_date, "No Time");
    } else {
        update_time_management(); // Update from RTC
    }

    if (currentTimeMs - lastM4DataExchangeTime >= M4_DATA_EXCHANGE_INTERVAL_MS) {
        lastM4DataExchangeTime = currentTimeMs;
        exchangeDataWithM4AndRefreshUI();
    }

    updateTemperatureSystem(); // Updates chart

    String m4_debug_buffer = "";
    while (RPC.available()) {
        m4_debug_buffer += (char)RPC.read();
    }
    if (m4_debug_buffer.length() > 0) {
        Serial.print(m4_debug_buffer); // Print M4's messages
    }


    delay(5);
}
