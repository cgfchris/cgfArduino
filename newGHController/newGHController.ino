// In GreenhouseM7_Main_Complete.ino

#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include <mbed.h>     // for watchdog
#include "STM32H747_System.h"  // for getting reset reason
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

// Watchdog configuration
const uint32_t REQUESTED_WATCHDOG_TIMEOUT_MS = 30000; // 30 seconds, adjust as needed

unsigned long lastM4DataExchangeTime = 0;
const unsigned long M4_DATA_EXCHANGE_INTERVAL_MS = 10000;

// These globals are now 'extern' in web_server.cpp
float m4_reported_temperature = NAN;
int m4_vent_stage = -1;
bool m4_heater_state = false;
bool m4_shade_state = false;
bool m4_boost_state = false;
bool m4_vent_opening_active = false;
bool m4_vent_closing_active = false;
bool m4_shade_opening_active = false;
bool m4_shade_closing_active = false;

// Buffers for LVGL labels (main file manages these)
char tempLabelBufferMain[10];
char ventStatusBuffer[20];     // For LVGL ui_ventStatusLabel
char heaterStatusBuffer[15];   // For LVGL ui_heaterStatusLabel
char shadeStatusBuffer[15];    // For LVGL ui_shadeStatusLabel
char statusBuffer[15];    // For LVGL any buffer

char reboot_time_String[12]; // store the last reboot time
const unsigned long LOOP_HEARTBEAT = 60000;


// M7's cache of M4's configurable settings
GreenhouseSettings m4_settings_cache; // M7 will fill this from M4

void setup() {
    Serial.begin(115200);
    unsigned long setupStartTime = millis();
    while (!Serial && (millis() - setupStartTime < 3000));
    
    Serial.println("\nM7: Main System Setup Started.");
    
    // --- Initialize Watchdog ---
    watchdog_init();
    
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
    
    initialize_ntp_and_rtc(); // Uses RTC, then tries NTP
    get_formatted_local_time(reboot_time_String, sizeof(reboot_time_String));
    get_reset_reason();

    if (is_wifi_connected()) {      
        initialize_web_server();  // <<<< Initialize web server
    } else {
        Serial.println("M7: WiFi not up. NTP and Web Server deferred.");
    }

    initializeTemperatureSystem();
    Serial.println("M7: Setup complete.");
    lastM4DataExchangeTime = millis();
}

// Function to update the color of an LVGL object (your box)
void update_indicator_box_color(lv_obj_t* box_obj, bool isActive) {
    if (box_obj) { // Check if the object exists
        if (isActive) {
            lv_obj_set_style_bg_color(box_obj, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT); // Red for active
        } else {
            lv_obj_set_style_bg_color(box_obj, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT); // White for inactive
        }
        lv_obj_set_style_bg_opa(box_obj, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT); // Ensure it's opaque
    }
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
        if (m4_vent_stage == 0) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "0 %%");
        else if (m4_vent_stage == 1) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "25 %%");
        else if (m4_vent_stage == 2) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "50 %%");
        else if (m4_vent_stage == 3) snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "100 %%");
        else snprintf(ventStatusBuffer, sizeof(ventStatusBuffer), "Vents: N/A");
        lv_label_set_text(ui_ventStatusLabel, ventStatusBuffer);
    }

    try { auto h = RPC.call("getM4HeaterState"); m4_heater_state = h.as<bool>(); } 
    catch (const std::exception& e) { /* retain old */ }
    if (ui_heaterStatusLabel) { 
        if (m4_boost_state){
          snprintf(heaterStatusBuffer, sizeof(heaterStatusBuffer), "%s", "Boost");
        }
        snprintf(heaterStatusBuffer, sizeof(heaterStatusBuffer), "%s", m4_heater_state ? "ON" : "OFF");
        lv_label_set_text(ui_heaterStatusLabel, heaterStatusBuffer);
    }
    
    try { auto h = RPC.call("getM4ShadeState"); m4_shade_state = h.as<bool>(); }
    catch (const std::exception& e) { /* retain old */ }
    if (ui_shadeStatusLabel) { 
        snprintf(shadeStatusBuffer, sizeof(shadeStatusBuffer), "%s", m4_shade_state ? "Open" : "Closed");
        lv_label_set_text(ui_shadeStatusLabel, shadeStatusBuffer);
    }

    // ---- NEW: Update temperature_system module with current operational states for charting ----
    updateCurrentVentStageForChart(m4_vent_stage);
    updateCurrentHeaterStateForChart(m4_heater_state);
    // ---- END NEW ----

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

    // print the settings to the display:
    snprintf(statusBuffer, sizeof(statusBuffer), "%.1f", m4_settings_cache.ventOpenTempStage1);
    lv_label_set_text(ui_vent25, statusBuffer);
    snprintf(statusBuffer, sizeof(statusBuffer), "%.1f", m4_settings_cache.ventOpenTempStage2);
    lv_label_set_text(ui_vent50, statusBuffer);
    snprintf(statusBuffer, sizeof(statusBuffer), "%.1f", m4_settings_cache.ventOpenTempStage3);
    lv_label_set_text(ui_vent100, statusBuffer);
    snprintf(statusBuffer, sizeof(statusBuffer), "%.1f", m4_settings_cache.heatSetTempNight);
    lv_label_set_text(ui_heatNight, statusBuffer);
    snprintf(statusBuffer, sizeof(statusBuffer), "%.1f", m4_settings_cache.heatSetTempDay);
    lv_label_set_text(ui_heatDay, statusBuffer);
    
    snprintf(statusBuffer,sizeof(statusBuffer),"%02u:%02u",m4_settings_cache.dayStartHour,m4_settings_cache.dayStartMinute);
    lv_label_set_text(ui_startDay, statusBuffer);
    snprintf(statusBuffer,sizeof(statusBuffer),"%02u:%02u",m4_settings_cache.nightStartHour,m4_settings_cache.nightStartMinute);
    lv_label_set_text(ui_startNight, statusBuffer);
    snprintf(statusBuffer,sizeof(statusBuffer),"%02u:%02u",m4_settings_cache.shadeOpenHour,m4_settings_cache.shadeOpenMinute);
    lv_label_set_text(ui_startShade, statusBuffer);
    snprintf(statusBuffer,sizeof(statusBuffer),"%02u:%02u",m4_settings_cache.shadeCloseHour,m4_settings_cache.shadeCloseMinute);
    lv_label_set_text(ui_endShade, statusBuffer);
    snprintf(statusBuffer,sizeof(statusBuffer),"%02u:%02u",m4_settings_cache.boostStartHour,m4_settings_cache.boostStartMinute);
    lv_label_set_text(ui_boostStart, statusBuffer);
    snprintf(statusBuffer,sizeof(statusBuffer),"%u",m4_settings_cache.boostDurationMinutes);
    lv_label_set_text(ui_boostDur, statusBuffer);
    snprintf(statusBuffer, sizeof(statusBuffer), "%.1f", m4_settings_cache.heatBoostTemp);
    lv_label_set_text(ui_boostTemp, statusBuffer);


    // get relay states:
    // --- Fetch Relay Activity States from M4 ---
    try { auto h = RPC.call("getVentOpeningActive"); m4_vent_opening_active = h.as<bool>(); } 
    catch (const std::exception& e) { /* Serial.println("M7 Exc: VentOpenActive"); */ }

    try { auto h = RPC.call("getVentClosingActive"); m4_vent_closing_active = h.as<bool>(); }
    catch (const std::exception& e) { /* Serial.println("M7 Exc: VentCloseActive"); */ }

    // m4_heater_state is already fetched and indicates heater relay status

    try { auto h = RPC.call("getShadeOpeningRelayActive"); m4_shade_opening_active = h.as<bool>(); }
    catch (const std::exception& e) { /* Serial.println("M7 Exc: ShadeOpenRelayActive"); */ }

    try { auto h = RPC.call("getShadeClosingRelayActive"); m4_shade_closing_active = h.as<bool>(); }
    catch (const std::exception& e) { /* Serial.println("M7 Exc: ShadeCloseRelayActive"); */ }

    // --- Update Indicator Box Colors ---
    // Replace ui_boxVentOpenIndicator etc. with your actual LVGL object names
    update_indicator_box_color(ui_boxVentOpenIndicator, m4_vent_opening_active);
    update_indicator_box_color(ui_boxVentCloseIndicator, m4_vent_closing_active);
    update_indicator_box_color(ui_boxHeaterIndicator, m4_heater_state); // Use the existing m4_heater_state
    update_indicator_box_color(ui_boxShadeOpenIndicator, m4_shade_opening_active);
    update_indicator_box_color(ui_boxShadeCloseIndicator, m4_shade_closing_active);
}


unsigned long lastLoopHeartbeat = 0; // For M7 loop debug

void loop() {
     
    // --- Kick/Pet the Watchdog at the START of the loop ---
    // Ensures that if any part of the loop hangs, the watchdog will eventually time out.
    mbed::Watchdog::get_instance().kick();
    
    unsigned long currentTimeMs = millis();
    lv_timer_handler();

    if (currentTimeMs - lastLoopHeartbeat >= LOOP_HEARTBEAT) {
        Serial.println("M7 Loop Heartbeat...");
        get_reset_reason();
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

void watchdog_init() {

    Serial.print("M7: Initializing Watchdog. Requested timeout: ");
    Serial.print(REQUESTED_WATCHDOG_TIMEOUT_MS);
    Serial.println(" ms");

    mbed::Watchdog &watchdog = mbed::Watchdog::get_instance();

    uint32_t max_hw_timeout = watchdog.get_max_timeout();
    Serial.print("M7: Watchdog Max HW Timeout reported by get_max_timeout(): ");
    Serial.print(max_hw_timeout);
    Serial.println(" ms");

    uint32_t timeout_to_use = REQUESTED_WATCHDOG_TIMEOUT_MS;

    if (max_hw_timeout == 0) {
        Serial.println("M7: WARNING - get_max_timeout() returned 0! Watchdog may be unreliable.");
        // If max is 0, trying to set any timeout might be problematic.
        // For safety, perhaps don't even try to start it, or use a very small known-to-work value if documented.
        // For now, let's still try with the requested, but this is an error indicator.
    } else if (REQUESTED_WATCHDOG_TIMEOUT_MS > max_hw_timeout) {
        Serial.println("M7: Requested timeout (" + String(REQUESTED_WATCHDOG_TIMEOUT_MS) + 
                       "ms) > max HW timeout (" + String(max_hw_timeout) + 
                       "ms). Using max HW timeout instead.");
        timeout_to_use = max_hw_timeout;
    }
    
    // Ensure timeout_to_use is not zero if we intend to start it.
    if (timeout_to_use == 0) {
        Serial.println("M7: CRITICAL - Calculated timeout_to_use is 0. Watchdog will not be started effectively.");
        return; // Don't start if timeout is 0
    }

    Serial.print("M7: Attempting to start watchdog with: ");
    Serial.print(timeout_to_use);
    Serial.println(" ms");

    if (watchdog.start(timeout_to_use)) {
        uint32_t actual_configured_timeout = watchdog.get_timeout(); // Get the actual timeout
        Serial.print("M7: Watchdog successfully started. Actual configured timeout: ");
        Serial.print(actual_configured_timeout);
        Serial.println(" ms");
        if (actual_configured_timeout < 1000 || (timeout_to_use > 1000 && actual_configured_timeout < timeout_to_use / 2) ) { 
            // If it's very short, or significantly less than requested (and request was reasonable)
            Serial.println("M7: WARNING - Actual watchdog timeout is much shorter than requested or very small! System may reset frequently.");
        }
    } else {
        Serial.print("M7: CRITICAL WARNING - Watchdog start() FAILED! Status code: ");
    }
}

void get_reset_reason(){
  reset_reason_t resetReason = STM32H747::getResetReason();

  Serial.print("Reboot time: ");
  Serial.print(reboot_time_String);
  Serial.print(" Reset reason was: ");
  switch (resetReason){
  case RESET_REASON_POWER_ON:
    Serial.println("Reset Reason Power ON");
    break;
  case RESET_REASON_PIN_RESET:
    Serial.println(  "Reset Reason PIN Reset");
    break;
  case RESET_REASON_BROWN_OUT:
    Serial.println(  "Reset Reason Brown Out");
    break;
  case RESET_REASON_SOFTWARE:
    Serial.println(  "Reset Reason Software");
    break;
  case RESET_REASON_WATCHDOG:
    Serial.println(  "Reset Reason Watchdog");
    break;
  case RESET_REASON_LOCKUP:
    Serial.println(  "Reset Reason Lockup");
    break;
  case RESET_REASON_WAKE_LOW_POWER:
    Serial.println(  "Reset Reason Wake Low Power");
    break;
  case RESET_REASON_ACCESS_ERROR:
    Serial.println(  "Reset Reason Access Error");
    break;
  case RESET_REASON_BOOT_ERROR:
    Serial.println(  "Reset Reason Boot Error");
    break;
  case RESET_REASON_MULTIPLE:
    Serial.println(  "Reset Reason Multiple");
    break;
  case RESET_REASON_PLATFORM:
    Serial.println(  "Reset Reason Platform");
    break;
  case RESET_REASON_UNKNOWN:
    Serial.println(  "Reset Reason Unknown");
    break;
  default:
    Serial.println(  "N/A");
    break;
  }
  
  
}
