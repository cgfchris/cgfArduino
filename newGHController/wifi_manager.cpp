// wifi_manager.cpp
#include "wifi_manager.h"
#include "config.h"
#include "lvgl.h"
#include "ui.h"     // For ui_statusLabel
#include <stdio.h>  // For snprintf

// WiFi credentials
const char* ssid = WIFI_SSID;
const char* pass = WIFI_PASSWORD;

// WiFi State and Timing for Reconnection
enum WiFiState {
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_CONNECTION_LOST
};
WiFiState currentWiFiState = WIFI_STATE_DISCONNECTED;

unsigned long lastWifiConnectAttemptMillis = 0;
const unsigned long WIFI_RECONNECT_INTERVAL_MS = 30000; // Try to reconnect every 30 seconds
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;    // Max time for a single connection attempt

char statusLabelBuffer[WIFI_STATUS_LABEL_BUFFER_SIZE]; // Ensure this size is adequate

// Forward declaration for internal use
static void attempt_wifi_connection();

void initialize_wifi() {
    Serial.println("WiFi: Initializing...");
    currentWiFiState = WIFI_STATE_CONNECTING; // Start in connecting state
    if (ui_statusLabel) {
        lv_label_set_text(ui_statusLabel, "WiFi Init...");
        lv_timer_handler(); // Refresh label
    }
    attempt_wifi_connection(); // Make the first connection attempt
}

static void attempt_wifi_connection() {
    Serial.println("WiFi: Attempting to connect...");
    if (ui_statusLabel) {
        lv_label_set_text(ui_statusLabel, "WiFi Connecting...");
        lv_timer_handler(); // Refresh label
    }

    WiFi.disconnect();  // Optional: ensure clean state before new attempt
    delay(100); // Short delay after disconnect
    WiFi.begin(ssid, pass);

    lastWifiConnectAttemptMillis = millis();
    currentWiFiState = WIFI_STATE_CONNECTING;

    int attemptCounter = 0;
    unsigned long connectionStartTime = millis();

    // Non-blocking check for connection with timeout
    // This part is tricky if called from setup() which is blocking.
    // For setup(), a blocking loop is fine. For manage_wifi_connection(), it needs to be non-blocking.
    // The initialize_wifi() version can be more blocking:
    if (millis() - connectionStartTime < WIFI_CONNECT_TIMEOUT_MS) { // Check if called from setup or manage
        Serial.print("WiFi: Initial connection attempt: ");
        while (WiFi.status() != WL_CONNECTED && (millis() - connectionStartTime < WIFI_CONNECT_TIMEOUT_MS)) {
            snprintf(statusLabelBuffer, sizeof(statusLabelBuffer), "Connecting... %d", attemptCounter++);
            if (ui_statusLabel) {
                lv_label_set_text(ui_statusLabel, statusLabelBuffer);
            }
            lv_timer_handler(); // Keep LVGL responsive
            delay(500);
            Serial.print(".");
        }
        Serial.println();
    }


    if (WiFi.status() == WL_CONNECTED) {
        currentWiFiState = WIFI_STATE_CONNECTED;
        Serial.println("WiFi: Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        if (ui_statusLabel) {
            lv_label_set_text(ui_statusLabel, "WiFi Connected");
            lv_timer_handler();
        }
        print_wifi_status();
    } else {
        // If still not connected after initial attempt (e.g. from setup)
        currentWiFiState = WIFI_STATE_DISCONNECTED; // Or CONNECTION_LOST if it was previously connected
        Serial.println("WiFi: Initial connection failed.");
        if (ui_statusLabel) {
            lv_label_set_text(ui_statusLabel, "WiFi Failed");
            lv_timer_handler();
        }
    }
}


void manage_wifi_connection() {
    wl_status_t status = static_cast<wl_status_t>(WiFi.status());

    switch (currentWiFiState) {
        case WIFI_STATE_CONNECTED:
            if (status != WL_CONNECTED) {
                Serial.println("WiFi: Connection Lost!");
                currentWiFiState = WIFI_STATE_CONNECTION_LOST;
                lastWifiConnectAttemptMillis = millis(); // Set for immediate retry attempt logic
                if (ui_statusLabel) {
                    lv_label_set_text(ui_statusLabel, "WiFi Lost!");
                    lv_timer_handler();
                }
                // No immediate retry here, wait for the interval in DISCONNECTED/CONNECTION_LOST
            }
            break;

        case WIFI_STATE_CONNECTING:
            if (status == WL_CONNECTED) {
                currentWiFiState = WIFI_STATE_CONNECTED;
                Serial.println("WiFi: Reconnected!");
                Serial.print("IP Address: ");
                Serial.println(WiFi.localIP());
                if (ui_statusLabel) {
                    lv_label_set_text(ui_statusLabel, "WiFi Reconnected");
                    lv_timer_handler();
                }
                print_wifi_status();
            } else if (millis() - lastWifiConnectAttemptMillis > WIFI_CONNECT_TIMEOUT_MS) {
                Serial.println("WiFi: Connection attempt timed out.");
                currentWiFiState = WIFI_STATE_DISCONNECTED; // Go to disconnected, will retry on interval
                if (ui_statusLabel) {
                    lv_label_set_text(ui_statusLabel, "WiFi Timeout");
                    lv_timer_handler();
                }
                // WiFi.disconnect();  // Optionally force disconnect
            }
            break;

        case WIFI_STATE_DISCONNECTED:
        case WIFI_STATE_CONNECTION_LOST:
            if (millis() - lastWifiConnectAttemptMillis > WIFI_RECONNECT_INTERVAL_MS) {
                Serial.println("WiFi: Attempting to reconnect...");
                if (ui_statusLabel) {
                    lv_label_set_text(ui_statusLabel, "WiFi Reconnecting...");
                    lv_timer_handler();
                }
                // Update last attempt time *before* starting the connection attempt
                lastWifiConnectAttemptMillis = millis();
                currentWiFiState = WIFI_STATE_CONNECTING; // Set state before blocking call
                
                WiFi.disconnect(); // Ensure clean state
                delay(100);
                WiFi.begin(ssid, pass); // This is a non-blocking call to start connection
                // The CONNECTING state will handle checking WiFi.status()
            }
            break;
    }
}

bool is_wifi_connected() {
    return (WiFi.status() == WL_CONNECTED && currentWiFiState == WIFI_STATE_CONNECTED);
}

void print_wifi_status() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("SSID: ");
        Serial.println(WiFi.SSID());
        IPAddress ip = WiFi.localIP();
        Serial.print("IP Address: ");
        Serial.println(ip);
        long rssi = WiFi.RSSI();
        Serial.print("Signal strength (RSSI):");
        Serial.print(rssi);
        Serial.println(" dBm");
    } else {
        Serial.println("WiFi: Not connected.");
    }
}
