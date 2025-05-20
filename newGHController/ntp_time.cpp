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
#include <Arduino.h>
#include <Wire.h>       // <<<< ADD FOR I2C
#include <RTClib.h>     // <<<< ADD FOR RTC LIBRARY

// RTC Object
RTC_DS1307 rtc; // Or RTC_PCF8523 rtc;

// NTP specific variables
WiFiUDP udp_ntp;
byte ntpPacketBuffer[NTP_PACKET_BUFFER_SIZE];
unsigned long lastNtpAttemptTimeMillis = 0;
unsigned long printTimeNowMillis = 0;

bool timeHasBeenSet = false; // True if RTC or NTP has successfully set system time

void initialize_ntp_and_rtc() {
    Serial.println("TimeKeeper: Initializing RTC...");
    if (!rtc.begin()) { // Try to initialize RTC
        Serial.println("TimeKeeper: Couldn't find RTC! Check wiring. Time will rely on NTP.");
        if (ui_date) lv_label_set_text(ui_date, "No RTC/NTP");
    } else {
        Serial.println("TimeKeeper: RTC found.");
             DateTime now_rtc = rtc.now();
            // Check if RTC time is reasonably valid (e.g., after year 2023)
            if (now_rtc.year() >= 2023) {
                // RTC time seems valid, set system time from RTC
                // RTClib's now_rtc.unixtime() gives UTC epoch if RTC was set with UTC
                // If RTC stores local time, this needs adjustment or careful handling.
                // For simplicity, let's assume RTC stores UTC.
                unsigned long rtc_utc_epoch = now_rtc.unixtime();
                unsigned long local_epoch_from_rtc = rtc_utc_epoch + ((long)NTP_TIMEZONE * 3600L);
                
                set_time(local_epoch_from_rtc); // Set system time to local time from RTC
                timeHasBeenSet = true;
                Serial.print("TimeKeeper: System time set from RTC (Local): ");
                Serial.println(local_epoch_from_rtc);
            } else {
                Serial.println("TimeKeeper: RTC time seems invalid (e.g., year < 2023). Waiting for NTP.");
            }
    }

    // Proceed with NTP initialization
    if (is_wifi_connected()) {
        Serial.println("TimeKeeper: WiFi connected. Attempting initial NTP sync...");
        if (udp_ntp.begin(NTP_LOCAL_PORT)) {
            send_ntp_packet(NTP_SERVER);
            lastNtpAttemptTimeMillis = millis();

            unsigned long ntpStartTime = millis();
            bool receivedNtpInSetup = false;
            while (millis() - ntpStartTime < 3000) { // Wait up to 3s for NTP
                if (udp_ntp.parsePacket() > 0) {
                    parse_ntp_response(); // This will set system time and update RTC
                    // timeHasBeenSet is set within parse_ntp_response
                    receivedNtpInSetup = true;
                    break;
                }
                delay(50);
            }
            if (!receivedNtpInSetup) {
                Serial.println("TimeKeeper: Initial NTP packet not received/parsed quickly.");
            }
        } else {
            Serial.println("TimeKeeper: Failed to begin UDP for NTP.");
        }
    } else {
        Serial.println("TimeKeeper: WiFi not connected at init. NTP sync deferred.");
        if (!timeHasBeenSet && ui_time) lv_label_set_text(ui_time, "--:--:--"); // Only if RTC also failed
        if (!timeHasBeenSet && ui_date) lv_label_set_text(ui_date, "No Time Source");
    }
    printTimeNowMillis = millis() - NTP_PRINT_INTERVAL_MS - 1;

}

void send_ntp_packet(const char* address) {
    // ... (same as before) ...
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

void parse_ntp_response() {
    int bytesRead = udp_ntp.read(ntpPacketBuffer, NTP_PACKET_BUFFER_SIZE);
    if (bytesRead < NTP_PACKET_BUFFER_SIZE) {
        Serial.println("NTP: Received undersized packet. Sync failed.");
        return;
    }

    unsigned long highWord = word(ntpPacketBuffer[40], ntpPacketBuffer[41]);
    unsigned long lowWord = word(ntpPacketBuffer[42], ntpPacketBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    if (secsSince1900 == 0) {

        Serial.println("NTP: Invalid timestamp (all zeros). Sync failed.");
        return;
    }
    constexpr unsigned long seventyYears = 2208988800UL;
    unsigned long utc_epoch = secsSince1900 - seventyYears;

    // Set system time (local)
    unsigned long local_epoch = utc_epoch + ((long)NTP_TIMEZONE * 3600L);
    set_time(local_epoch);
    timeHasBeenSet = true;
    Serial.print("NTP: System time synchronized. RTC set to local epoch: "); Serial.println(local_epoch);


    // Also update the hardware RTC with this new UTC time
    if (rtc.begin()) { // Ensure RTC is still accessible
        rtc.adjust(DateTime(utc_epoch)); // RTClib DateTime constructor from unixtime assumes UTC
        Serial.println("TimeKeeper: Hardware RTC updated with NTP time (UTC).");
    } else {
        Serial.println("TimeKeeper: Warning - Failed to re-access RTC to update it with NTP time.");
    }
}

void get_formatted_local_time(char* buffer, size_t buffer_size) {

    if (!is_time_valid()) { // Use the new generic time validity check
        snprintf(buffer, buffer_size, "--:--:--");
        return;
    }
    time_t now = time(NULL);
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, buffer_size, "%H:%M:%S", &timeinfo);
}

void get_formatted_local_date(char* buffer, size_t buffer_size) {
    if (!is_time_valid()) {
        snprintf(buffer, buffer_size, "No Time Source");
        return;
    }
    time_t now = time(NULL);

    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, buffer_size, "%b %d, %Y", &timeinfo);
}

void update_time_management() { // Renamed function
    // Update UI display
    if (millis() - printTimeNowMillis >= NTP_PRINT_INTERVAL_MS) {
        char timeString[12];
        char dateString[20];

        get_formatted_local_time(timeString, sizeof(timeString));
        get_formatted_local_date(dateString, sizeof(dateString));

        if (ui_time) lv_label_set_text(ui_time, timeString);
        if (ui_date) lv_label_set_text(ui_date, dateString);
        printTimeNowMillis = millis();
    }


    // Determine NTP sync interval
    unsigned long current_ntp_sync_interval;
    if (timeHasBeenSet && (time(NULL) > MIN_VALID_EPOCH_TIME) ) {
        current_ntp_sync_interval = NTP_SYNC_INTERVAL_MS; // Time is set, use normal long interval
    } else {
        current_ntp_sync_interval = NTP_RETRY_INTERVAL_MS; // Time not set, use shorter retry for NTP
    }

    // Periodically attempt to sync NTP
    if (millis() - lastNtpAttemptTimeMillis >= current_ntp_sync_interval) {
        if (is_wifi_connected()) {
            Serial.print("TimeKeeper: Attempting NTP sync with " + String(NTP_SERVER));
            if (!timeHasBeenSet) Serial.print(" (Initial/Retry Mode)");
            Serial.println();

            if (udp_ntp.begin(NTP_LOCAL_PORT)) {
                send_ntp_packet(NTP_SERVER);
                lastNtpAttemptTimeMillis = millis(); // Mark this attempt time

                unsigned long ntpResponseWaitStart = millis();
                bool ntpSuccessThisAttempt = false;
                while (millis() - ntpResponseWaitStart < 2000) {
                    if (udp_ntp.parsePacket() > 0) {
                        parse_ntp_response(); // Sets system time, RTC, and timeHasBeenSet
                        if (timeHasBeenSet && (time(NULL) > MIN_VALID_EPOCH_TIME)) {
                           ntpSuccessThisAttempt = true;
                        }
                        break;
                    }
                    delay(50);
                }
                if (!ntpSuccessThisAttempt) {
                    Serial.println("NTP: Sync attempt failed to get valid response.");
                }
            } else {
                Serial.println("NTP: Failed to begin UDP for sync attempt.");
                lastNtpAttemptTimeMillis = millis(); // So we wait before retrying UDP begin
            }
        } else {
            // If WiFi not connected, lastNtpAttemptTimeMillis is not updated here,
            // so it will try more frequently once WiFi is back, using NTP_RETRY_INTERVAL_MS
            // if timeHasBeenSet is still false.
            // Serial.println("TimeKeeper: WiFi not connected for NTP sync attempt.");

        }
    }
}

bool is_time_valid() { // Renamed
    return timeHasBeenSet && (time(NULL) > MIN_VALID_EPOCH_TIME);
}

int get_current_hour() {
    if (!is_time_valid()) return -1;
    time_t now = time(NULL);
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    return timeinfo.tm_hour;
}

int get_current_minute() {
    if (!is_time_valid()) return -1;
    time_t now = time(NULL);
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    return timeinfo.tm_min;
}

int get_current_second() {
    if (!is_time_valid()) return -1;
    time_t now = time(NULL);
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    return timeinfo.tm_sec;
}   
