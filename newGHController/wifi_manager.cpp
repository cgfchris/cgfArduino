// wifi_manager.cpp
#include "wifi_manager.h"
#include "config.h"       // For WIFI_SSID, WIFI_PASSWORD, WIFI_STATUS_LABEL_BUFFER_SIZE
#include "lvgl.h"
#include "ui.h"           // For ui_wifiStatusLabel (or ui_statusLabel if you renamed it)
#include <WiFi.h>
#include <Arduino.h>      // For Serial, millis, delay
#include <stdio.h>        // For snprintf

// WiFi credentials from config.h
const char* ssid_local = WIFI_SSID;
const char* pass_local = WIFI_PASSWORD;

// WiFi State and Timing
enum WiFiState_Internal {
    WIFI_S_DISCONNECTED,
    WIFI_S_CONNECTING,
    WIFI_S_CONNECTED,
    WIFI_S_CONNECTION_FAILED,
    WIFI_S_CONNECTION_LOST
};
WiFiState_Internal currentInternalWiFiState = WIFI_S_DISCONNECTED;

unsigned long lastWifiActionMillis = 0;
unsigned long lastStatusLabelUpdateMillis = 0; // To throttle "Connecting..." messages

// Ensure these are defined in config.h or here
#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS 15000UL
#endif
#ifndef WIFI_RECONNECT_INTERVAL_MS
#define WIFI_RECONNECT_INTERVAL_MS 30000UL
#endif
#ifndef WIFI_STATUS_LABEL_BUFFER_SIZE // For status messages like "Connecting" AND IP Address
#define WIFI_STATUS_LABEL_BUFFER_SIZE 32 // Max IP "xxx.xxx.xxx.xxx" is 15 chars + null. "WiFi Connecting..." is ~20. 32 should be safe.
#endif
char wifiStatusLabelBuffer_local[WIFI_STATUS_LABEL_BUFFER_SIZE]; // Local buffer for formatting strings

// --- Function to update the ui_wifiStatusLabel ---
static void update_wifi_status_label(const char* message) {
    if (ui_wifiStatusLabel) { // Use the label dedicated to WiFi status / IP
        lv_label_set_text(ui_wifiStatusLabel, message);
        lv_timer_handler();
    }
    Serial.print("WiFiMan_UI_Update: "); Serial.println(message);
}

static void update_wifi_status_label_with_ip(IPAddress ip) {
    if (ui_wifiStatusLabel) {
        snprintf(wifiStatusLabelBuffer_local, sizeof(wifiStatusLabelBuffer_local), 
                 "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        lv_label_set_text(ui_wifiStatusLabel, wifiStatusLabelBuffer_local);
        lv_timer_handler();
    }
    Serial.print("WiFiMan_UI_Update: IP "); Serial.println(ip);
}


void initialize_wifi() {
    Serial.println("WiFiMan: Initializing...");
    currentInternalWiFiState = WIFI_S_CONNECTING;
    lastWifiActionMillis = millis();
    lastStatusLabelUpdateMillis = millis(); // Initialize this too

    WiFi.disconnect(); 
    delay(100);
    WiFi.begin(ssid_local, pass_local);
    
    Serial.println("WiFiMan: WiFi.begin() called. Initial connection process started.");
    update_wifi_status_label("WiFi Init...");
    
    // --- Print MAC Address ---
    // It's good to do this early, often possible even before WiFi.begin()
    // but safest after WiFi module has some level of initialization.
    // WiFi.macAddress() usually works once the WiFi object is available.
    byte macBuffer[6];
    WiFi.macAddress(macBuffer); // Get MAC address into a byte array
    Serial.print("WiFiMan: Device MAC Address: ");
    for (int i = 0; i < 6; i++) {
        if (macBuffer[i] < 0x10) { // Add leading zero for single-digit hex
            Serial.print("0");
        }
        Serial.print(macBuffer[i], HEX);
        if (i < 5) {
            Serial.print(":");
        }
    }
    Serial.println();
    // --- End MAC Address Print ---
}

void manage_wifi_connection() {
    // Correctly get the WiFi status with a cast
    wl_status_t current_wl_status = static_cast<wl_status_t>(WiFi.status()); 
    unsigned long current_millis = millis();

    switch (currentInternalWiFiState) {
        case WIFI_S_DISCONNECTED:
            if (current_millis - lastWifiActionMillis >= WIFI_RECONNECT_INTERVAL_MS) {
                Serial.println("WiFiMan: Reconnect interval. Attempting connection.");
                update_wifi_status_label("WiFi Reconnecting...");
                WiFi.disconnect();
                delay(100);
                WiFi.begin(ssid_local, pass_local);
                currentInternalWiFiState = WIFI_S_CONNECTING;
                lastWifiActionMillis = current_millis;
                lastStatusLabelUpdateMillis = current_millis; // Reset for "Connecting..." throttle
            }
            break;

        case WIFI_S_CONNECTING:
            if (current_wl_status == WL_CONNECTED) {
                currentInternalWiFiState = WIFI_S_CONNECTED;
                lastWifiActionMillis = current_millis;
                Serial.println("WiFiMan: Connected!");
                IPAddress currentIP = WiFi.localIP();
                Serial.print("WiFiMan: IP Address: "); Serial.println(currentIP);
                update_wifi_status_label_with_ip(currentIP); // <<<< UPDATE LABEL WITH IP
                // print_wifi_status(); 
            } else if (current_millis - lastWifiActionMillis >= WIFI_CONNECT_TIMEOUT_MS) {
                Serial.println("WiFiMan: Connection attempt timed out.");
                update_wifi_status_label("WiFi Timeout");
                WiFi.disconnect();
                currentInternalWiFiState = WIFI_S_CONNECTION_FAILED;
                lastWifiActionMillis = current_millis;
            } else {
                // Throttle "Connecting..." message updates to once per second
                if (current_millis - lastStatusLabelUpdateMillis > 1000) { 
                    update_wifi_status_label("WiFi Connecting...");
                    lastStatusLabelUpdateMillis = current_millis;
                }
            }
            break;

        case WIFI_S_CONNECTED:
            if (current_wl_status != WL_CONNECTED) {
                Serial.println("WiFiMan: Connection Lost!");
                update_wifi_status_label("WiFi Lost!");
                currentInternalWiFiState = WIFI_S_CONNECTION_LOST;
                lastWifiActionMillis = current_millis;
            }
            // No need to repeatedly update IP if it hasn't changed
            break;

        case WIFI_S_CONNECTION_FAILED:
        case WIFI_S_CONNECTION_LOST:
            if (current_millis - lastWifiActionMillis >= WIFI_RECONNECT_INTERVAL_MS) {
                Serial.println("WiFiMan: Attempting new connection after failure/loss.");
                update_wifi_status_label("WiFi Retrying...");
                WiFi.disconnect();
                delay(100);
                WiFi.begin(ssid_local, pass_local);
                currentInternalWiFiState = WIFI_S_CONNECTING;
                lastWifiActionMillis = current_millis;
                lastStatusLabelUpdateMillis = current_millis; // Reset for "Connecting..." throttle
            }
            break;
    }
}

bool is_wifi_connected() {
    return (WiFi.status() == WL_CONNECTED && currentInternalWiFiState == WIFI_S_CONNECTED);
}

void print_wifi_status() { // This remains for Serial debugging if needed
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("SSID (from print_wifi_status): "); Serial.println(WiFi.SSID());
        IPAddress ip = WiFi.localIP();
        Serial.print("IP Address (from print_wifi_status): "); Serial.println(ip);
        long rssi = WiFi.RSSI();
        Serial.print("Signal strength (RSSI from print_wifi_status):"); Serial.print(rssi); Serial.println(" dBm");
    } else {
        Serial.println("WiFi: Not connected (checked by print_wifi_status).");
    }
}
