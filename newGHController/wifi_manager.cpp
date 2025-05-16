// wifi_manager.cpp
#include "wifi_manager.h"
#include "config.h"         // For WIFI_SSID, WIFI_PASSWORD
#include "lvgl.h"           // For LVGL types if used directly
#include "ui.h"             // For ui_statusLabel (from SquareLine)
#include <stdio.h>          // For snprintf

// WiFi credentials and status
const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASSWORD;
int wifi_status = WL_IDLE_STATUS;
char statusLabelBuffer[WIFI_STATUS_LABEL_BUFFER_SIZE];

void initialize_wifi() {
    if (ui_statusLabel) { // Check if ui_statusLabel is valid
        lv_label_set_text(ui_statusLabel, "WiFi Connecting...");
    }
    Serial.println("Attempting to connect to WiFi...");

    int attempt = 0;
    wifi_status = WiFi.begin(ssid, pass);

    while (wifi_status != WL_CONNECTED && attempt < 20) { // Timeout after ~10 seconds
        Serial.print("Attempting to connect to SSID: ");
        Serial.println(ssid);

        snprintf(statusLabelBuffer, sizeof(statusLabelBuffer), "Connecting... %d", attempt);
        if (ui_statusLabel) {
            lv_label_set_text(ui_statusLabel, statusLabelBuffer);
        }
        lv_timer_handler(); // Allow LVGL to process during blocking loop

        delay(500);
        wifi_status = WiFi.status(); // Re-check status
        attempt++;
    }

    if (wifi_status == WL_CONNECTED) {
        Serial.println("Connected to WiFi");
        if (ui_statusLabel) {
            lv_label_set_text(ui_statusLabel, "WiFi Connected");
        }
        print_wifi_status();
    } else {
        Serial.println("Failed to connect to WiFi");
        if (ui_statusLabel) {
            lv_label_set_text(ui_statusLabel, "WiFi Failed");
        }
    }
    if (ui_statusLabel) lv_timer_handler(); // Update display
}

void print_wifi_status() {
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());

    IPAddress ip = WiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    long rssi = WiFi.RSSI();
    Serial.print("Signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
}

void update_wifi_status_display() {
    // This function could be expanded to periodically check WiFi status
    // and update ui_statusLabel if it changes (e.g., disconnects).
    // For now, it's mostly initialized once.
    // If you want dynamic updates, you'd check WiFi.status() here.
    static unsigned long lastWifiCheck = 0;
    if (millis() - lastWifiCheck > 5000) { // Check every 5 seconds
        lastWifiCheck = millis();
        if (WiFi.status() != WL_CONNECTED && wifi_status == WL_CONNECTED) {
            Serial.println("WiFi Disconnected!");
            if (ui_statusLabel) lv_label_set_text(ui_statusLabel, "WiFi Lost!");
            wifi_status = WL_DISCONNECTED; // Update our tracked status
            // Optionally, try to reconnect:
            // initialize_wifi();
        } else if (WiFi.status() == WL_CONNECTED && wifi_status != WL_CONNECTED) {
            Serial.println("WiFi Reconnected!");
            if (ui_statusLabel) lv_label_set_text(ui_statusLabel, "WiFi Connected");
            wifi_status = WL_CONNECTED;
        }
    }
}
