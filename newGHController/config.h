// config.h
#ifndef CONFIG_H
#define CONFIG_H

// --- Display Configuration ---
#define SCREEN_WIDTH 480  // Or 480 if rotated
#define SCREEN_HEIGHT 800 // Or 800 if rotated
// If you set LV_DISP_DEF_ROT to 270, LVGL expects these to be the *rotated* dimensions:
// #define SCREEN_WIDTH 480
// #define SCREEN_HEIGHT 800


// --- WiFi Configuration ---
#define WIFI_SSID "devries"
#define WIFI_PASSWORD "devriesWireless"
#define WIFI_STATUS_LABEL_BUFFER_SIZE 50

// --- NTP Configuration ---
#define NTP_SERVER "pool.ntp.org"
#define NTP_LOCAL_PORT 2390
#define NTP_TIMEZONE (-4) // UTC-4
#define NTP_PACKET_BUFFER_SIZE 48
#define NTP_PRINT_INTERVAL_MS 1000

// --- Temperature System Configuration ---
#define TEMP_READ_INTERVAL_MS 2000
//#define TEMP_SAMPLE_INTERVAL_MS (10 * 1000) // 10 seconds for testing
#define TEMP_SAMPLE_INTERVAL_MS (5 * 60 * 1000) // 5 minutes for production
#define MAX_TEMP_SAMPLES 100
#define CHART_Y_MIN_VALUE 0
#define CHART_Y_MAX_VALUE 50
#define MIN_VALID_EPOCH_TIME 1672531200L // Jan 1, 2023

#endif // CONFIG_H
