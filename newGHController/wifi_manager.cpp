// wifi_manager.cpp
#include "wifi_manager.h"
#include "config.h"
#include "lvgl.h"
#include "ui.h"     // For ui_wifiStatusLable
#include <stdio.h>  // For snprintf

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

    int attemptCounter = 0;
    unsigned long connectionStartTime = millis();

    // Non-blocking check for connection with timeout
    // This part is tricky if called from setup() which is blocking.
    // For setup(), a blocking loop is fine. For manage_wifi_connection(), it needs to be non-blocking.
    // The initialize_wifi() version can be more blocking:
    if (millis() - connectionStartTime < WIFI_CONNECT_TIMEOUT_MS) { // Check if called from setup or manage
        Serial.print("WiFi: Initial connection attempt: ");
        snprintf(statusLabelBuffer, sizeof(statusLabelBuffer), "Connecting... %d", attemptCounter++);
        if (ui_statusLabel) {
           lv_label_set_text(ui_wifiStatusLabel, statusLabelBuffer);
        }
        lv_timer_handler(); // Keep LVGL responsive
        while (WiFi.status() != WL_CONNECTED && (millis() - connectionStartTime < WIFI_CONNECT_TIMEOUT_MS)) {
            delay(500);
            Serial.print(".");
        }
        lv_timer_handler(); // Allow LVGL to process during blocking loop

        delay(500);
        wifi_status = WiFi.status(); // Re-check status
        attempt++;
    }


    if (WiFi.status() == WL_CONNECTED) {
        currentWiFiState = WIFI_STATE_CONNECTED;
        Serial.println("WiFi: Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        if (ui_wifiStatusLabel) {
            lv_label_set_text(ui_wifiStatusLable, "WiFi Connected");
            lv_timer_handler();
        }
        print_wifi_status();
    } else {
        // If still not connected after initial attempt (e.g. from setup)
        currentWiFiState = WIFI_STATE_DISCONNECTED; // Or CONNECTION_LOST if it was previously connected
        Serial.println("WiFi: Initial connection failed.");
        if (ui_wifiStatusLable) {
            lv_label_set_text(ui_wifiStatusLable, "WiFi Failed");
            lv_timer_handler();
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

    switch (currentWiFiState) {
        case WIFI_STATE_CONNECTED:
            if (status != WL_CONNECTED) {
                Serial.println("WiFi: Connection Lost!");
                currentWiFiState = WIFI_STATE_CONNECTION_LOST;
                lastWifiConnectAttemptMillis = millis(); // Set for immediate retry attempt logic
                if (ui_wifiStatusLable) {
                    lv_label_set_text(ui_wifiStatusLable, "WiFi Lost!");
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
                if (ui_wifiStatusLable) {
                    lv_label_set_text(ui_wifiStatusLable, "WiFi Reconnected");
                    lv_timer_handler();
                }
                print_wifi_status();
            } else if (millis() - lastWifiConnectAttemptMillis > WIFI_CONNECT_TIMEOUT_MS) {
                Serial.println("WiFi: Connection attempt timed out.");
                currentWiFiState = WIFI_STATE_DISCONNECTED; // Go to disconnected, will retry on interval
                if (ui_wifiStatusLable) {
                    lv_label_set_text(ui_wifiStatusLable, "WiFi Timeout");
                    lv_timer_handler();
                }
                // WiFi.disconnect();  // Optionally force disconnect
            }
            break;

        case WIFI_STATE_DISCONNECTED:
        case WIFI_STATE_CONNECTION_LOST:
            if (millis() - lastWifiConnectAttemptMillis > WIFI_RECONNECT_INTERVAL_MS) {
                Serial.println("WiFi: Attempting to reconnect...");
                if (ui_wifiStatusLable) {
                    lv_label_set_text(ui_wifiStatusLable, "WiFi Reconnecting...");
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
