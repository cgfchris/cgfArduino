// YourProjectName.ino

// Core Hardware & LVGL
#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lv_conf.h" // Should be configured by SquareLine or your project
#include "lvgl.h"
#include "ui.h" // From SquareLine Studio
#include <RPC.h>

// Custom Modules
#include "config.h"         // If you create one for SSID, etc.
#include "wifi_manager.h"
#include "ntp_time.h"
#include "temperature_system.h"


// --- Global Hardware Objects ---
Arduino_H7_Video Display(SCREEN_WIDTH, SCREEN_HEIGHT, GigaDisplayShield); // Use constants from config.h
Arduino_GigaDisplayTouch TouchDetector;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000); // Optional: wait for serial connection

    // Boot the M4 core.
    Serial.println("M7: Core setup started.");
    Serial.println("M7: Attempting to start RPC and boot M4...");

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
    Display.begin();
    TouchDetector.begin();

    // --- Initialize UI (from SquareLine Studio) ---
    ui_init(); // This creates ui_tempLabel, ui_tempChart, ui_statusLabel etc.
    Serial.println("UI Initialized.");

    // --- Initialize Custom Modules ---
    initialize_wifi(); // From wifi_manager
    Serial.println("WiFi Module Init Called.");

    initialize_ntp(); // From ntp_time
    Serial.println("NTP Module Init Called.");

    initializeTemperatureSystem(); // From temperature_system
    Serial.println("Temperature System Initialized.");

    Serial.println("Setup complete.");
}

void loop() {
    lv_timer_handler();
    
    manage_wifi_connection(); // Call this in every loop iteration
    
    // Only update NTP/other network dependent things if WiFi is connected
    if (is_wifi_connected()) { // Use the new helper function
        update_ntp_time_display(); 
    } else {
        // Optionally update UI to show NTP is waiting for WiFi
        if (ui_time) lv_label_set_text(ui_time, "--:--:--");
        if (ui_date) lv_label_set_text(ui_date, "No WiFi");
    }
    
    updateTemperatureSystem(); // Temp system can run regardless of WiFi

    delay(5);
}
