// ntp_time.cpp
#include "ntp_time.h"
#include "config.h"
#include "wifi_manager.h" // Needed for is_wifi_connected()
#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <mbed_mktime.h>
#include "lvgl.h"
#include "ui.h" // For ui_time, ui_date

// NTP specific variables
WiFiUDP udp_ntp; // Renamed to avoid conflict
byte ntpPacketBuffer[NTP_PACKET_BUFFER_SIZE];
unsigned long lastNtpSyncTimeMillis = 0; // Track millis for periodic sync
const unsigned long NTP_SYNC_INTERVAL_MS = 1 * 60 * 60 * 1000UL; // Sync every 1 hour
// const unsigned long NTP_SYNC_INTERVAL_MS = 60 * 1000UL; // For testing: Sync every 1 minute

unsigned long printTimeNowMillis = 0; // For throttling UI print updates

void initialize_ntp() {
    if (is_wifi_connected()) {
        Serial.println("NTP: WiFi connected. Starting UDP client for initial sync...");
        if (udp_ntp.begin(NTP_LOCAL_PORT)) { // begin() returns 1 on success
            send_ntp_packet(NTP_SERVER);
            
            unsigned long ntpStartTime = millis();
            bool received = false;
            // Wait up to ~2 seconds for a quick reply in setup
            while (millis() - ntpStartTime < 2000) {
                if (udp_ntp.parsePacket() > 0) { // Check if data is available
                    parse_ntp_response();
                    lastNtpSyncTimeMillis = millis(); // Mark time of successful sync
                    received = true;
                    break;
                }
                delay(50); // Brief pause to allow packet arrival
            }
            if (!received) {
                Serial.println("NTP: Initial packet not immediately received/parsed. Will try on schedule.");
                // Don't set lastNtpSyncTimeMillis here, let the first periodic sync happen sooner
            }
        } else {
            Serial.println("NTP: Failed to begin UDP for initial sync.");
        }
    } else {
        Serial.println("NTP: WiFi not connected at init. Skipping initial sync.");
        if (ui_time) lv_label_set_text(ui_time, "--:--:--");
        if (ui_date) lv_label_set_text(ui_date, "No NTP Sync");
    }
    // Initialize printTimeNowMillis to ensure the first UI update happens promptly in loop()
    printTimeNowMillis = millis() - NTP_PRINT_INTERVAL_MS -1; // Force immediate update
}

void send_ntp_packet(const char* address) {
    memset(ntpPacketBuffer, 0, NTP_PACKET_BUFFER_SIZE);
    ntpPacketBuffer[0] = 0b11100011;  // LI, Version, Mode
    ntpPacketBuffer[1] = 0;           // Stratum
    ntpPacketBuffer[2] = 6;           // Polling Interval
    ntpPacketBuffer[3] = 0xEC;        // Peer Clock Precision
    ntpPacketBuffer[12] = 49;
    ntpPacketBuffer[13] = 0x4E;
    ntpPacketBuffer[14] = 49;
    ntpPacketBuffer[15] = 52;

    udp_ntp.beginPacket(address, 123);
    udp_ntp.write(ntpPacketBuffer, NTP_PACKET_BUFFER_SIZE);
    udp_ntp.endPacket();
    // Serial.println("NTP packet sent."); // Can be verbose
}

void parse_ntp_response() {
    // Assumes udp_ntp.parsePacket() was called and returned > 0 before this function
    udp_ntp.read(ntpPacketBuffer, NTP_PACKET_BUFFER_SIZE);

    unsigned long highWord = word(ntpPacketBuffer[40], ntpPacketBuffer[41]);
    unsigned long lowWord = word(ntpPacketBuffer[42], ntpPacketBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    if (secsSince1900 == 0) {
        Serial.println("NTP: Invalid timestamp received (all zeros).");
        return; // Do not update time
    }

    constexpr unsigned long seventyYears = 2208988800UL;
    unsigned long utc_epoch = secsSince1900 - seventyYears;

    long local_epoch_offset = (long)NTP_TIMEZONE * 3600L;
    unsigned long local_epoch = utc_epoch + local_epoch_offset;

    set_time(local_epoch); // Set the system RTC to LOCAL TIME

    Serial.print("NTP: System time synchronized. RTC set to local time epoch: "); Serial.println(local_epoch);

    // --- For Debugging: Print UTC and Local based on the fetched UTC epoch ---
    // Serial.print("NTP: Fetched UTC Epoch = "); Serial.println(utc_epoch);
    // struct tm tm_utc;
    // _rtc_localtime(utc_epoch, &tm_utc, RTC_FULL_LEAP_YEAR_SUPPORT);
    // char buffer_utc[40];
    // strftime(buffer_utc, sizeof(buffer_utc), "%Y-%m-%d %H:%M:%S (Calc UTC)", &tm_utc);
    // Serial.print("NTP: "); Serial.println(buffer_utc);

    // time_t current_rtc_time = time(NULL);
    // struct tm tm_local;
    // _rtc_localtime(current_rtc_time, &tm_local, RTC_FULL_LEAP_YEAR_SUPPORT);
    // char buffer_local[40];
    // strftime(buffer_local, sizeof(buffer_local), "%Y-%m-%d %H:%M:%S (RTC Local)", &tm_local);
    // Serial.print("NTP: "); Serial.println(buffer_local);
    // --- End Debugging ---
}


void get_formatted_local_time(char* buffer, size_t buffer_size) {
    time_t now = time(NULL);
    if (now < MIN_VALID_EPOCH_TIME) { // Check if time is valid before formatting
        snprintf(buffer, buffer_size, "--:--:--");
        return;
    }
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, buffer_size, "%k:%M:%S", &timeinfo);
}

void get_formatted_local_date(char* buffer, size_t buffer_size) {
    time_t now = time(NULL);
    if (now < MIN_VALID_EPOCH_TIME) { // Check if time is valid
        snprintf(buffer, buffer_size, "No Time Sync");
        return;
    }
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, buffer_size, "%b %d, %Y", &timeinfo);
}

void update_ntp_time_display() {
    if (millis() - printTimeNowMillis >= NTP_PRINT_INTERVAL_MS) {
        char timeString[12];
        char dateString[20];

        // These functions now handle invalid time internally
        get_formatted_local_time(timeString, sizeof(timeString));
        get_formatted_local_date(dateString, sizeof(dateString));

        if (ui_time) lv_label_set_text(ui_time, timeString);
        if (ui_date) lv_label_set_text(ui_date, dateString);

        printTimeNowMillis = millis();
    }

    // Periodically re-sync NTP
    if (millis() - lastNtpSyncTimeMillis >= NTP_SYNC_INTERVAL_MS) {
        if (is_wifi_connected()) {
            Serial.println("NTP: Performing periodic sync.");
            if (udp_ntp.begin(NTP_LOCAL_PORT)) { // Ensure UDP is up, begin can be called multiple times
                send_ntp_packet(NTP_SERVER);
                unsigned long ntpAttemptTime = millis();
                bool ntpSuccess = false;
                // Wait up to ~2 seconds for response during periodic sync
                while (millis() - ntpAttemptTime < 2000) {
                    if (udp_ntp.parsePacket() > 0) {
                        parse_ntp_response();
                        lastNtpSyncTimeMillis = millis(); // Update time of last successful sync
                        ntpSuccess = true;
                        break;
                    }
                    delay(50); // Brief pause
                }
                if (!ntpSuccess) {
                    Serial.println("NTP: Periodic sync packet not received/parsed.");
                    // To prevent rapid retries if server is down but WiFi is up,
                    // still update lastNtpSyncTimeMillis. It will try again after the full interval.
                    lastNtpSyncTimeMillis = millis();
                }
            } else {
                Serial.println("NTP: Failed to begin UDP for periodic sync.");
                lastNtpSyncTimeMillis = millis(); // Wait full interval before retrying UDP begin
            }
        } else {
            Serial.println("NTP: WiFi not connected for periodic sync. Will try when WiFi is back.");
            // Don't update lastNtpSyncTimeMillis, so it attempts sync more promptly once WiFi reconnects.
        }
    }
}
