// ntp_time.cpp
#include "ntp_time.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <mbed_mktime.h>
#include "lvgl.h"
#include "ui.h"

WiFiUDP udp_ntp; // Renamed to avoid conflict if you use 'udp' elsewhere
byte ntpPacketBuffer[NTP_PACKET_BUFFER_SIZE];
unsigned long lastNtpSyncTimeMillis = 0; // Track millis for periodic sync
const unsigned long NTP_SYNC_INTERVAL_MS = 60 * 60 * 1000UL; // Sync every hour

unsigned long printTimeNowMillis = 0;

void initialize_ntp() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("NTP: Starting UDP client...");
        udp_ntp.begin(NTP_LOCAL_PORT); // Use the renamed UDP object
        Serial.print("NTP: Sending initial packet to "); Serial.println(NTP_SERVER);
        send_ntp_packet(NTP_SERVER);
        delay(1000); // Wait for packet
        parse_ntp_response();
        lastNtpSyncTimeMillis = millis();
    } else {
        Serial.println("NTP: WiFi not connected. Skipping initial sync.");
        if (ui_time) lv_label_set_text(ui_time, "--:--:--");
        if (ui_date) lv_label_set_text(ui_date, "No NTP Sync");
    }
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
    // Serial.println("NTP packet sent."); // Can be a bit verbose
}

void parse_ntp_response() {
    int cb = udp_ntp.parsePacket();
    if (!cb) {
        Serial.println("NTP: No packet received");
        return;
    }

    // Serial.print("NTP: Packet received, length="); Serial.println(cb); // Verbose
    udp_ntp.read(ntpPacketBuffer, NTP_PACKET_BUFFER_SIZE);

    unsigned long highWord = word(ntpPacketBuffer[40], ntpPacketBuffer[41]);
    unsigned long lowWord = word(ntpPacketBuffer[42], ntpPacketBuffer[43]);
    unsigned long secsSince1900 = highWord << 16 | lowWord;

    if (secsSince1900 == 0) {
        Serial.println("NTP: Invalid timestamp received (all zeros).");
        return;
    }
    // Serial.print("NTP: Seconds since Jan 1 1900 = "); Serial.println(secsSince1900); // Verbose

    constexpr unsigned long seventyYears = 2208988800UL;
    unsigned long utc_epoch = secsSince1900 - seventyYears;

    // Apply timezone offset to get local epoch
    long local_epoch_offset = (long)NTP_TIMEZONE * 3600L;
    unsigned long local_epoch = utc_epoch + local_epoch_offset;

    set_time(local_epoch); // Set the system RTC to LOCAL TIME

    Serial.print("NTP: System time synchronized. RTC set to local time epoch: "); Serial.println(local_epoch);

    // --- For Debugging: Print UTC and Local based on the fetched UTC epoch ---
    Serial.print("NTP: Fetched UTC Epoch = "); Serial.println(utc_epoch);
    
    // Display UTC time
    struct tm tm_utc;
    // To display the fetched UTC time, we need to convert utc_epoch to struct tm
    // We can use _rtc_localtime with the utc_epoch, as it just breaks it down.
    // The "local" aspect of _rtc_localtime only matters if the input epoch is meant to be interpreted
    // with a system-defined timezone, but here we are giving it a raw epoch.
    _rtc_localtime(utc_epoch, &tm_utc, RTC_FULL_LEAP_YEAR_SUPPORT); 
    char buffer_utc[30];
    strftime(buffer_utc, sizeof(buffer_utc), "%Y-%m-%d %H:%M:%S (Calculated UTC)", &tm_utc);
    Serial.print("NTP: "); Serial.println(buffer_utc);

    // Display local time (should match what RTC gives via time(NULL) now)
    time_t current_rtc_time = time(NULL); // Should be local_epoch
    struct tm tm_local;
    _rtc_localtime(current_rtc_time, &tm_local, RTC_FULL_LEAP_YEAR_SUPPORT);
    char buffer_local[30];
    strftime(buffer_local, sizeof(buffer_local), "%Y-%m-%d %H:%M:%S (RTC Local)", &tm_local);
    Serial.print("NTP: "); Serial.println(buffer_local);
    // --- End Debugging ---
}


// This function now directly uses time(NULL) which should be local time
void get_formatted_local_time(char* buffer, size_t buffer_size) {
    time_t now = time(NULL); // This will be local epoch if set_time was called with local_epoch
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, buffer_size, "%k:%M:%S", &timeinfo); // %k for hour (0-23), space padded
}

// This function now directly uses time(NULL) which should be local time
void get_formatted_local_date(char* buffer, size_t buffer_size) {
    time_t now = time(NULL); // This will be local epoch
    struct tm timeinfo;
    _rtc_localtime(now, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
    strftime(buffer, buffer_size, "%b %d, %Y", &timeinfo);
}

void update_ntp_time_display() {
    if (millis() - printTimeNowMillis >= NTP_PRINT_INTERVAL_MS) {
        char timeString[12];
        char dateString[20];

        get_formatted_local_time(timeString, sizeof(timeString));
        get_formatted_local_date(dateString, sizeof(dateString));

        // Serial.print("Display Time: "); Serial.print(dateString); Serial.print(" "); Serial.println(timeString);

        if (ui_time) lv_label_set_text(ui_time, timeString);
        if (ui_date) lv_label_set_text(ui_date, dateString);

        printTimeNowMillis = millis();
    }

    // Periodically re-sync NTP
    if (millis() - lastNtpSyncTimeMillis >= NTP_SYNC_INTERVAL_MS) {
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("NTP: Performing periodic sync.");
            send_ntp_packet(NTP_SERVER);
            delay(1000); // Give a sec for the packet
            parse_ntp_response();
            lastNtpSyncTimeMillis = millis();
        } else {
            Serial.println("NTP: WiFi not connected for periodic sync. Will try later.");
            // No need to reset lastNtpSyncTimeMillis here, it will keep trying
        }
    }
}
