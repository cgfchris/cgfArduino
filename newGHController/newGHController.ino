// GreenhouseM7_Main_Complete.ino with DEBUG prints

// ... (all includes and global hardware objects as before) ...
#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"
<<<<<<< Updated upstream
<<<<<<< Updated upstream
#include "ui.h" // From SquareLine Studio

// Custom Modules
#include "config.h"         // If you create one for SSID, etc.
=======
#include "ui.h"
#include <RPC.h>
#include "config.h"
>>>>>>> Stashed changes
=======
#include "ui.h"
#include <RPC.h>
#include "config.h"
>>>>>>> Stashed changes
#include "wifi_manager.h"
#include "ntp_time.h"
#include "temperature_system.h"
#include <Arduino.h> // Make sure this is here

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
char tempLabelBufferMain[10];
char statusLabelBufferMain[50];

void setup() {
    Serial.begin(115200);
<<<<<<< Updated upstream
<<<<<<< Updated upstream
    //while (!Serial && millis() < 5000); // Optional: wait for serial connection

=======
    unsigned long setupStartTime = millis();
    // Make this wait very short for initial debugging. If Serial never connects, we want to proceed.
    while (!Serial && (millis() - setupStartTime < 1000)); // Shortened wait

    Serial.println("\n\n\nDEBUG: M7: Setup Phase 0 - Serial Initialized."); delay(100);

=======
    unsigned long setupStartTime = millis();
    // Make this wait very short for initial debugging. If Serial never connects, we want to proceed.
    while (!Serial && (millis() - setupStartTime < 1000)); // Shortened wait

    Serial.println("\n\n\nDEBUG: M7: Setup Phase 0 - Serial Initialized."); delay(100);

>>>>>>> Stashed changes
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

<<<<<<< Updated upstream
>>>>>>> Stashed changes
=======
>>>>>>> Stashed changes
    // --- Hardware and LVGL Initialization ---
    Serial.println("DEBUG: M7: Setup Phase 2 - Display.begin()..."); delay(100);
    Display.begin();
    Serial.println("DEBUG: M7: Display.begin() DONE."); delay(100);

    Serial.println("DEBUG: M7: Setup Phase 3 - TouchDetector.begin()..."); delay(100);
    TouchDetector.begin();
    Serial.println("DEBUG: M7: TouchDetector.begin() DONE."); delay(100);

    // LVGL library initialization and driver registration are assumed
    // to be handled within ui_init() from SquareLine Studio.
    // If you were doing it manually, add prints around lv_init(), buffer setup, driver setup.
    // Serial.println("DEBUG: M7: Setup Phase 4 - lv_init()..."); delay(100);
    // lv_init(); // Typically called by ui_init()
    // Serial.println("DEBUG: M7: lv_init() DONE."); delay(100);
    // ... manual LVGL driver init prints ...

    Serial.println("DEBUG: M7: Setup Phase 5 - ui_init()..."); delay(100);
    ui_init(); 
    Serial.println("DEBUG: M7: UI Initialized (ui_init() DONE)."); delay(100);

    // --- Initialize Custom Modules ---
    Serial.println("DEBUG: M7: Setup Phase 6 - initialize_wifi()..."); delay(100);
    initialize_wifi(); // From wifi_manager.h
    Serial.println("DEBUG: M7: initialize_wifi() DONE."); delay(100);

    Serial.println("DEBUG: M7: Setup Phase 7 - initialize_ntp()..."); delay(100);
    initialize_ntp(); // From ntp_time.h
    Serial.println("DEBUG: M7: initialize_ntp() DONE."); delay(100);

    Serial.println("DEBUG: M7: Setup Phase 8 - initializeTemperatureSystem()..."); delay(100);
    initializeTemperatureSystem(); // From temperature_system.h
    Serial.println("DEBUG: M7: initializeTemperatureSystem() DONE."); delay(100);

    Serial.println("DEBUG: M7: Setup complete. Entering loop()."); delay(100);
    lastM4DataExchangeTime = millis();
}

// exchangeDataWithM4AndRefreshUI() - using the simplified try-catch version
void exchangeDataWithM4AndRefreshUI() {
    //Serial.println("DEBUG: M7: exchangeDataWithM4AndRefreshUI() called."); // Can be very noisy

    if (is_ntp_synced()) {
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
    } catch (const std::exception& e) {
        Serial.print("M7 [EXC_M4_TEMP]: "); Serial.println(e.what()); // More specific debug
        m4_reported_temperature = NAN;
    }
    updateCurrentTemperatureFromM4(m4_reported_temperature);
    Serial.print("DEBUG: M7: M4 Temp for UI: ");
    if (isnan(m4_reported_temperature)) Serial.println("NAN"); else Serial.println(m4_reported_temperature);
    
    if (ui_tempLabel != NULL) { // Check if UI object exists
        if (!isnan(m4_reported_temperature)) {
            snprintf(tempLabelBufferMain, sizeof(tempLabelBufferMain), "%.1fC", m4_reported_temperature);
            Serial.print("DEBUG: M7: Setting ui_tempLabel text to: "); Serial.println(tempLabelBufferMain); // Add this
            lv_label_set_text(ui_tempLabel, tempLabelBufferMain);
        } else {
            Serial.println("DEBUG: M7: Setting ui_tempLabel text to ---C"); // Add this
            lv_label_set_text(ui_tempLabel, "---C");
        }
    } else {
        Serial.println("DEBUG: M7: ERROR - ui_tempLabel is NULL!"); // Add this
    }

    int vent_stage_val = -1;
    try {
        auto vent_handle = RPC.call("getM4VentStage");
        vent_stage_val = vent_handle.as<int>();
        m4_vent_stage = vent_stage_val;
    } catch (const std::exception& e) { Serial.print("M7 [EXC_M4_VENT]: "); Serial.println(e.what()); m4_vent_stage = -1; }

    bool heater_state_val = m4_heater_state; // Default to previous
    try {
        auto heater_handle = RPC.call("getM4HeaterState");
        heater_state_val = heater_handle.as<bool>();
        m4_heater_state = heater_state_val;
    } catch (const std::exception& e) { Serial.print("M7 [EXC_M4_HEAT]: "); Serial.println(e.what()); }

    bool shade_state_val = m4_shade_state; // Default to previous
    try {
        auto shade_handle = RPC.call("getM4ShadeState");
        shade_state_val = shade_handle.as<bool>();
        m4_shade_state = shade_state_val;
    } catch (const std::exception& e) { Serial.print("M7 [EXC_M4_SHADE]: "); Serial.println(e.what()); }

    bool boost_state_val = m4_boost_state; // Default to previous
    try {
        auto boost_handle = RPC.call("getM4BoostState");
        boost_state_val = boost_handle.as<bool>();
        m4_boost_state = boost_state_val;
    } catch (const std::exception& e) { Serial.print("M7 [EXC_M4_BOOST]: "); Serial.println(e.what()); }

    if (ui_statusLabel) {
        String v_str = "V:?";
        if(m4_vent_stage != -1){
            if (m4_vent_stage == 0) v_str = "V:C";
            else if (m4_vent_stage == 1) v_str = "V:25%";
            else if (m4_vent_stage == 2) v_str = "V:50%";
            else if (m4_vent_stage == 3) v_str = "V:100%";
        }
        snprintf(statusLabelBufferMain, sizeof(statusLabelBufferMain),
                 "%s H:%s S:%s B:%s",
                 v_str.c_str(),
                 m4_heater_state ? "ON" : "OFF",
                 m4_shade_state ? "Open" : "Closed",
                 m4_boost_state ? "ON" : "OFF");
        lv_label_set_text(ui_statusLabel, statusLabelBufferMain);
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

    if (is_wifi_connected()) {
        update_ntp_time_display();
    } else {
        if (ui_time) lv_label_set_text(ui_time, "--:--:--");
        if (ui_date) lv_label_set_text(ui_date, "No WiFi");
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
