// ntp_time.cpp
#include "ntp_time.h"
#include "config.h"
#include "wifi_manager.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <mbed_mktime.h>
#include "lvgl.h"
#include "ui.h"
#include <Arduino.h> // Ensure this is included for Serial etc.

// NTP specific variables
WiFiUDP udp_ntp;
byte ntpPacketBuffer[NTP_PACKET_BUFFER_SIZE];
unsigned long lastNtpAttemptTimeMillis = 0; // Changed name for clarity
                                           // This tracks the last *attempt* to sync.
                                           // We will decide the *next* attempt time based on sync status.

unsigned long printTimeNowMillis = 0;
bool ntpHasSyncedAtLeastOnce = false;

void initialize_ntp() {
    if (is_wifi_connected()) {
        Serial.println("NTP: WiFi connected. Starting UDP client for initial sync attempt...");
        if (udp_ntp.begin(NTP_LOCAL_PORT)) {
            send_ntp_packet(NTP_SERVER);
            lastNtpAttemptTimeMillis = millis(); // Mark time of this attempt

            unsigned long ntpStartTime = millis();
            bool receivedInSetup = false;
            while (millis() - ntpStartTime < 2000) {
                if (udp_ntp.parsePacket() > 0) {
                    parse_ntp_response(); // Sets ntpHasSyncedAtLeastOnce
                    // lastNtpAttemptTimeMillis is already set
                    receivedInSetup = true;
                    break;
                }
                delay(50);
            }
            if (!receivedInSetup) {
                Serial.println("NTP: Initial packet not immediately received/parsed. Will retry based on schedule.");
            }
        } else {
            Serial.println("NTP: Failed to begin UDP on port " + String(NTP_LOCAL_PORT) + " for initial sync.");
            lastNtpAttemptTimeMillis = millis(); // Still set to allow retry logic to kick in
        }
    } else {
        Serial.println("NTP: WiFi not connected. Skipping initial sync.");
        if (ui_time) lv_label_set_text(ui_time, "--:--:--");
        if (ui_date) lv_label_set_text(ui_date, "No NTP Sync");
        lastNtpAttemptTimeMillis = millis(); // Set to allow retry logic
    }
    printTimeNowMillis = millis() - NTP_PRINT_INTERVAL_MS - 1;
}

// send_ntp_packet - no change needed
void send_ntp_packet(const char* address) {
    memset(ntpPacketBuffer, 0, NTP_PACKET_BUFFER_SIZE);
    ntpPacketBuffer[0] = 0b11100011;
    ntpPacketBuffer[1] = 0;
    ntpPacketBuffer[2] = 6;
    ntpPacketBuffer[3] = 0xEC;
    ntpPacketBuffer[12] = 49;
    ntpPacketBuffer[13] = 0x4E;
    ntpPacketBuffer[14] = 49;
    ntpPacketBuffer[15] = 52;

    if (udp_ntp.beginPacket(address, 123)) {
        udp_ntp.write(ntpPacketBuffer, NTP_PACKET_BUFFER_SIZE);
        udp_ntp.endPacket();
    } else {
        Serial.println("NTP: Error - beginPacket failed for sending NTP request.");
    }
}


// parse_ntp_response - no change needed, already sets ntpHasSyncedAtLeastOnce
void parse_ntp_response() {
    int bytesRead = udp_ntp.read(ntpPacketBuffer, NTP_PACKET_BUFFER_SIZE);
    if (bytesRead < NTP_PACKET_BUFFER_SIZE) {
        Serial.println("NTP: Received undersized packet (" + String(bytesRead) + " bytes). Discarding.");
        return;
    }
    unsigned long highWord = word(ntpPacketBuffer[40], ntpPacketBuffer[41]);
    unsigned long lowWord = word(ntpPacketBuffer[42], ntpPacketBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    if (secsSince1900 == 0) {
        Serial.println("NTP: Invalid timestamp received (all zeros). Sync failed.");
        return;
    }
    constexpr unsigned long seventyYears = 2208988800UL;
    unsigned long utc_epoch = secsSince1900 - seventyYears;
    long local_epoch_offset = (long)NTP_TIMEZONE * 3600L;
    unsigned long local_epoch = utc_epoch + local_epoch_offset;
    set_time(local_epoch);
    ntpHasSyncedAtLeastOnce = true;
    Serial.print("NTP: System time synchronized. RTC set to local epoch: "); Serial.println(local_epoch);
}

// get_formatted_local_time - no change needed
void get_formatted_local_time(char* buffer, size_t buffer_size) {
    if (!is_ntp_synced()) {
        snprintf(buffer, buffer_size, "--:--:--");
        return;
    }
    time_t now = time(NULL);
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, buffer_size, "%H:%M:%S", &timeinfo);
}

// get_formatted_local_date - no change needed
void get_formatted_local_date(char* buffer, size_t buffer_size) {
    if (!is_ntp_synced()) {
        snprintf(buffer, buffer_size, "No Time Sync");
        return;
    }
    time_t now = time(NULL);
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, buffer_size, "%b %d, %Y", &timeinfo);
}


void update_ntp_time_display() {
    // Update UI display at a throttled rate
    if (millis() - printTimeNowMillis >= NTP_PRINT_INTERVAL_MS) {
        char timeString[12];
        char dateString[20];
        get_formatted_local_time(timeString, sizeof(timeString));
        get_formatted_local_date(dateString, sizeof(dateString));
        if (ui_time) lv_label_set_text(ui_time, timeString);
        if (ui_date) lv_label_set_text(ui_date, dateString);
        printTimeNowMillis = millis();
    }

    // Determine the correct sync interval based on current sync status
    unsigned long current_sync_interval;
    if (ntpHasSyncedAtLeastOnce && (time(NULL) > MIN_VALID_EPOCH_TIME) ) {
        current_sync_interval = NTP_SYNC_INTERVAL_MS; // Normal, longer interval
    } else {
        current_sync_interval = NTP_RETRY_INTERVAL_MS; // Not yet synced, shorter retry interval
    }

    // Periodically attempt to sync NTP
    if (millis() - lastNtpAttemptTimeMillis >= current_sync_interval) {
        if (is_wifi_connected()) {
            Serial.print("NTP: Attempting sync with " + String(NTP_SERVER));
            if (!ntpHasSyncedAtLeastOnce) {
                Serial.print(" (Retry mode, interval: " + String(current_sync_interval / 1000) + "s)");
            }
            Serial.println();

            if (udp_ntp.begin(NTP_LOCAL_PORT)) { // Ensure UDP client is ready
                send_ntp_packet(NTP_SERVER);
                lastNtpAttemptTimeMillis = millis(); // Mark the time of this attempt *before* waiting for response

                unsigned long ntpResponseWaitStart = millis();
                bool ntpSuccessThisAttempt = false;
                while (millis() - ntpResponseWaitStart < 2000) { // Wait up to 2s for response
                    if (udp_ntp.parsePacket() > 0) {
                        parse_ntp_response(); // This sets ntpHasSyncedAtLeastOnce
                        if (ntpHasSyncedAtLeastOnce && (time(NULL) > MIN_VALID_EPOCH_TIME)) {
                            ntpSuccessThisAttempt = true;
                        }
                        break;
                    }
                    delay(50);
                }

                if (!ntpSuccessThisAttempt) {
                    Serial.println("NTP: Sync attempt packet not received/parsed, or sync failed.");
                    // lastNtpAttemptTimeMillis is already updated, so it will wait for the chosen interval.
                }
                // udp_ntp.stop(); // Optional: consider if stopping is needed
            } else {
                Serial.println("NTP: Failed to begin UDP on port " + String(NTP_LOCAL_PORT) + " for sync attempt.");
                lastNtpAttemptTimeMillis = millis(); // Ensure we wait before retrying UDP begin
            }
        } else {
            // Serial.println("NTP: WiFi not connected for sync attempt."); // Can be noisy
            // Don't update lastNtpAttemptTimeMillis if WiFi is down,
            // so it attempts NTP sync more promptly once WiFi reconnects.
            // However, if it's already trying every minute, this might not be necessary.
            // Let's update it to prevent spamming begin() if WiFi is flaky.
            lastNtpAttemptTimeMillis = millis();
        }
    }
}

// is_ntp_synced - no change needed
bool is_ntp_synced() {
    return ntpHasSyncedAtLeastOnce && (time(NULL) > MIN_VALID_EPOCH_TIME);
}

// get_current_hour - no change needed
int get_current_hour() {
    if (!is_ntp_synced()) return -1;
    time_t now = time(NULL);
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    return timeinfo.tm_hour;
}

// get_current_minute - no change needed
int get_current_minute() {
    if (!is_ntp_synced()) return -1;
    time_t now = time(NULL);
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    return timeinfo.tm_min;
}

// get_current_second - no change needed
int get_current_second() {
    if (!is_ntp_synced()) return -1;
    time_t now = time(NULL);
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    return timeinfo.tm_sec;
}
