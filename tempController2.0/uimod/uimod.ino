#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lv_conf.h"
#include "lvgl.h"
#include <ui.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <stdio.h>   // For snprintf
#include <math.h>    // For NAN if used
#include <time.h>    // For time_t, struct tm, strftime
#include <mbed_mktime.h>

Arduino_H7_Video Display( 480, 800, GigaDisplayShield ); //( 800, 480, GigaDisplayShield );
Arduino_GigaDisplayTouch TouchDetector;

// wifi setup
int status = WL_IDLE_STATUS;
char ssid[] = "devries";        // your network SSID (name)
char pass[] = "devriesWireless";  // your network password (use for WPA, or use as key for WEP)
unsigned int localPort = 2390;  // local port to listen for UDP packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
// time setup
int timezone = -4;
constexpr auto timeServer{ "pool.ntp.org" };
const int NTP_PACKET_SIZE = 48;  // NTP timestamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE];  // buffer to hold incoming and outgoing packets
constexpr unsigned long printInterval{ 1000 };
unsigned long printNow{};
char statusLabelBuffer[50]; 

// ai code starts here
// --- Configuration ---
const unsigned long TEMP_READ_INTERVAL = 2000;     // How often to read temp & update label (milliseconds)
//const unsigned long TEMP_SAMPLE_INTERVAL = 5 * 60 * 1000; // How often to store a sample for the graph (5 minutes)
const unsigned long TEMP_SAMPLE_INTERVAL = 10 * 1000; // Faster interval for TESTING (10 seconds)
const int MAX_TEMP_SAMPLES = 100;                  // Number of samples to store and plot

// Chart Y-axis range values
const lv_coord_t CHART_Y_MIN_VALUE = 0; // Your desired minimum Y value for the chart (e.g., 10°C)
const lv_coord_t CHART_Y_MAX_VALUE = 50; // Your desired maximum Y value for the chart (e.g., 35°C)

// Time validation constant
const time_t MIN_VALID_EPOCH_TIME = 1672531200L; // Jan 1, 2023 00:00:00 UTC - a reasonable "after this time is valid" check

// --- Structure to hold sample data ---
struct TempSampleData {
    float temperature;       // The temperature reading
    time_t timestamp;        // Unix timestamp of when the sample was taken
    bool isValidTimestamp;   // True if the timestamp is considered valid (e.g., post-NTP sync)
};

// --- Global Variables ---
// LVGL Objects (assumed to be created by ui_init() or similar)
// extern lv_obj_t *ui_tempLabel; // Should be declared in ui.h if using SquareLine Studio
// extern lv_obj_t *ui_tempChart; // Should be declared in ui.h if using SquareLine Studio
lv_chart_series_t *ui_tempChartSeries = NULL; // Pointer for the chart data series

// Temperature Data
float currentTemperature = NAN; // Initialize current temperature to Not-A-Number
TempSampleData historicalSamples[MAX_TEMP_SAMPLES]; // Array to store historical temperature data
int validSamplesCurrentlyStored = 0; // Tracks how many valid samples have been added to historicalSamples (mainly for initial filling logic if needed)

// Timing
unsigned long lastTempReadTime = 0;
unsigned long lastSampleTime = 0;

// Buffer for label formatting
char tempLabelBuffer[32];


void setup() {
  Serial.begin(115200);
  Display.begin();
  TouchDetector.begin();
  ui_init();
  initializeTemperatureSystem();
  
  // attempt to connect to WiFi network:
  lv_label_set_text(ui_statusLabel, "Connecting...");
  int i = 0;
  status = WiFi.begin(ssid, pass);
  while (status != WL_CONNECTED) {
    
    Serial.print("Attempting to connect to SSID: ");
    snprintf(statusLabelBuffer, sizeof(statusLabelBuffer), "Connecting... %d", i);
    lv_label_set_text(ui_statusLabel, statusLabelBuffer);
    lv_timer_handler();
    
    i++;
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 1 seconds for connection:
    delay(500);
  }
  Serial.println("Connected to WiFi");
  lv_label_set_text(ui_statusLabel, "WiFi Connected");
  lv_timer_handler();
  printWifiStatus();
  setNtpTime();
  

  
  
  // timer for time update
  static unsigned long lastTime = 0;
  lastTime = millis();
}

void loop()
{
  
  lv_timer_handler(); 
  char timeString[32];
  char dateString[32];
  updateTemperatureSystem();
  // serial print time
  if (millis() > printNow) {
    Serial.print("System Clock:          ");
    getLocaltime(timeString);
    getLocaldate(dateString);
    Serial.println(timeString);
    lv_label_set_text(ui_time, timeString);
    lv_label_set_text(ui_date, dateString);
    printNow = millis() + printInterval;
  }
  delay(0.5);
}

void setNtpTime() {
  udp.begin(localPort);
  sendNTPpacket(timeServer);
  delay(1000);
  parseNtpPacket();
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(const char* address) {
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  packetBuffer[0] = 0b11100011;  // LI, Version, Mode
  packetBuffer[1] = 0;           // Stratum, or type of clock
  packetBuffer[2] = 6;           // Polling Interval
  packetBuffer[3] = 0xEC;        // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  udp.beginPacket(address, 123);  // NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}
unsigned long parseNtpPacket() {
  if (!udp.parsePacket())
    return 0;

  udp.read(packetBuffer, NTP_PACKET_SIZE);
  const unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
  const unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
  const unsigned long secsSince1900 = highWord << 16 | lowWord;
  constexpr unsigned long seventyYears = 2208988800UL;
  const unsigned long epoch = secsSince1900 - seventyYears;
  
  const unsigned long new_epoch = epoch + (3600 * timezone); // multiply the timezone with 3600 (1 hour)

  set_time(new_epoch);

#if defined(VERBOSE)
  Serial.print("Seconds since Jan 1 1900 = ");
  Serial.println(secsSince1900);

  // now convert NTP time into everyday time:
  Serial.print("Unix time = ");
  // print Unix time:
  Serial.println(epoch);

  // print the hour, minute and second:
  Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
  Serial.print((epoch % 86400L) / 3600);  // print the hour (86400 equals secs per day)
  Serial.print(':');
  if (((epoch % 3600) / 60) < 10) {
    // In the first 10 minutes of each hour, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.print((epoch % 3600) / 60);  // print the minute (3600 equals secs per minute)
  Serial.print(':');
  if ((epoch % 60) < 10) {
    // In the first 10 seconds of each minute, we'll want a leading '0'
    Serial.print('0');
  }
  Serial.println(epoch % 60);  // print the second
#endif

  return epoch;
}
void getLocaltime(char* buffer) {
  tm t;
  _rtc_localtime(time(NULL), &t, RTC_FULL_LEAP_YEAR_SUPPORT);
  strftime(buffer, 32, "%k:%M:%S", &t);
}
void getLocaldate(char* buffer) {
  tm t;
  _rtc_localtime(time(NULL), &t, RTC_FULL_LEAP_YEAR_SUPPORT);
  strftime(buffer, 32, "%b %d, %Y", &t);
}
void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

// AI functions:  
/*
#include <lvgl.h>
#include <ui.h>          // Assuming this declares ui_tempLabel and ui_tempChart
#include <stdio.h>       // For snprintf
#include <math.h>        // For isnan, round
#include <time.h>        // For time_t, struct tm, strftime, time()
#include <mbed_mktime.h> // For _rtc_localtime on Portenta/Giga (or your platform's equivalent)

// --- Configuration ---
const unsigned long TEMP_READ_INTERVAL = 2000;     // How often to read temp & update label (milliseconds)
const unsigned long TEMP_SAMPLE_INTERVAL = 5 * 60 * 1000; // How often to store a sample for the graph (5 minutes)
// const unsigned long TEMP_SAMPLE_INTERVAL = 10 * 1000; // Faster interval for TESTING (10 seconds)
const int MAX_TEMP_SAMPLES = 100;                  // Number of samples to store and plot

// Chart Y-axis range values
const lv_coord_t CHART_Y_MIN_VALUE = 10; // Your desired minimum Y value for the chart (e.g., 10°C)
const lv_coord_t CHART_Y_MAX_VALUE = 35; // Your desired maximum Y value for the chart (e.g., 35°C)

// Time validation constant
const time_t MIN_VALID_EPOCH_TIME = 1672531200L; // Jan 1, 2023 00:00:00 UTC - a reasonable "after this time is valid" check

// --- Structure to hold sample data ---
struct TempSampleData {
    float temperature;       // The temperature reading
    time_t timestamp;        // Unix timestamp of when the sample was taken
    bool isValidTimestamp;   // True if the timestamp is considered valid (e.g., post-NTP sync)
};

// --- Global Variables ---
// LVGL Objects (assumed to be created by ui_init() or similar)
// extern lv_obj_t *ui_tempLabel; // Should be declared in ui.h if using SquareLine Studio
// extern lv_obj_t *ui_tempChart; // Should be declared in ui.h if using SquareLine Studio
lv_chart_series_t *ui_tempChartSeries = NULL; // Pointer for the chart data series

// Temperature Data
float currentTemperature = NAN; // Initialize current temperature to Not-A-Number
TempSampleData historicalSamples[MAX_TEMP_SAMPLES]; // Array to store historical temperature data
int validSamplesCurrentlyStored = 0; // Tracks how many valid samples have been added to historicalSamples (mainly for initial filling logic if needed)

// Timing
unsigned long lastTempReadTime = 0;
unsigned long lastSampleTime = 0;

// Buffer for label formatting
char tempLabelBuffer[32];

*/

// --- Placeholder Sensor Function ---
// !! REPLACE THIS with your actual sensor reading code !!
float readTemperatureSensor() {
    // --- Example Simulation (Remove this block) ---
    static float simulatedTempBase = 22.0; // Start at 22 degrees
    int change = rand() % 3 - 1; // -1, 0, or 1
    simulatedTempBase += (float)change * 0.5f; // Change by -0.5, 0, or +0.5
    if (simulatedTempBase < CHART_Y_MIN_VALUE + 2) simulatedTempBase = CHART_Y_MIN_VALUE + 2; // Keep within a range
    if (simulatedTempBase > CHART_Y_MAX_VALUE - 2) simulatedTempBase = CHART_Y_MAX_VALUE - 2;
    // Occasionally return NAN to test that path
    // if ((rand() % 20) == 0) return NAN;
    return simulatedTempBase + (rand() % 100) / 150.0f; // Add small noise
    // --- End Simulation ---
}


// --- Custom Draw Event for Chart X-Axis Tick Labels ---
static void chart_x_axis_draw_event_cb(lv_event_t * e) {
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);

    // Check if we are drawing a tick label on the PRIMARY X axis
    if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_X && dsc->text != NULL) {
        // dsc->value gives the index of the data point for this tick
        // (0 for the oldest point currently visible on the chart, up to point_count-1 for the newest)
        int chart_point_index = dsc->value;

        // Our historicalSamples array is always shifted, so historicalSamples[chart_point_index]
        // corresponds to the point LVGL is asking about.
        if (chart_point_index >= 0 && chart_point_index < MAX_TEMP_SAMPLES &&
            historicalSamples[chart_point_index].isValidTimestamp) {

            time_t sample_ts = historicalSamples[chart_point_index].timestamp;
            struct tm timeinfo;

            // Convert timestamp to struct tm (local time)
            _rtc_localtime(sample_ts, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT); // For Portenta/Giga
            // Or for standard C library:
            // struct tm *timeinfo_ptr = localtime(&sample_ts);
            // if (timeinfo_ptr) timeinfo = *timeinfo_ptr; else { lv_snprintf(dsc->text, dsc->text_length, "-ERR-"); return; }

            char time_str_buffer[6]; // "HH:MM\0"
            strftime(time_str_buffer, sizeof(time_str_buffer), "%H:%M", &timeinfo);

            lv_snprintf(dsc->text, dsc->text_length, "%s", time_str_buffer);
        } else {
            // If no valid timestamp for this point, make the label a placeholder
            lv_snprintf(dsc->text, dsc->text_length, "-:-");
        }
    }
}


// --- Initialization (Call this ONCE in your setup() after ui_init()) ---
void initializeTemperatureSystem() {
    // Initialize the historical sample array
    for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
        historicalSamples[i].temperature = NAN; // Or some other indicator of no data
        historicalSamples[i].timestamp = 0;
        historicalSamples[i].isValidTimestamp = false;
    }
    validSamplesCurrentlyStored = 0; // Reset counter

    // Check if the chart object exists (created by ui_init)
    if (ui_tempChart != NULL) {
        lv_chart_set_type(ui_tempChart, LV_CHART_TYPE_LINE);
        lv_chart_set_update_mode(ui_tempChart, LV_CHART_UPDATE_MODE_SHIFT);
        lv_chart_set_point_count(ui_tempChart, MAX_TEMP_SAMPLES);

        // Set the Y-axis range using the defined constants
        lv_chart_set_range(ui_tempChart, LV_CHART_AXIS_PRIMARY_Y, CHART_Y_MIN_VALUE, CHART_Y_MAX_VALUE);

        // Add a data series to the chart
        ui_tempChartSeries = lv_chart_add_series(ui_tempChart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
        if (!ui_tempChartSeries) {
            Serial.println("Error: Failed to add series to chart!");
            return; // Cannot proceed
        }

        // Initialize chart points to the minimum Y value
        Serial.print("Initializing chart points to Y_MIN: "); Serial.println(CHART_Y_MIN_VALUE);
        for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
            lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, CHART_Y_MIN_VALUE);
        }

        // Configure X-Axis Ticks
        uint8_t num_x_major_ticks = 5;    // Example: 5 labels
        lv_coord_t major_tick_len = 10;   // Length of major tick lines
        lv_coord_t minor_tick_len = 5;    // Length of minor tick lines
        uint8_t num_minor_ticks_between_major = 2; // Example
        bool draw_labels_for_major_ticks = true;
        lv_coord_t extra_draw_space_for_labels = 20; // Or 15, 25 etc. Adjust this based on font size and label length.
                                             // This is the space reserved for the labels below the x-axis.

        lv_chart_set_axis_tick(ui_tempChart, LV_CHART_AXIS_PRIMARY_X,
                       major_tick_len,
                       minor_tick_len,
                       num_x_major_ticks,
                       num_minor_ticks_between_major,
                       draw_labels_for_major_ticks,
                       extra_draw_space_for_labels); // This is an lv_coord_t
                       
        // lv_obj_set_style_text_font(ui_tempChart, &lv_font_montserrat_10, LV_PART_TICKS | LV_STATE_DEFAULT);
        // You might also want to set text color, etc. for LV_PART_TICKS
        // lv_obj_set_style_text_color(ui_tempChart, lv_color_hex(0x808080), LV_PART_TICKS | LV_STATE_DEFAULT); // Example: Grey
    
        // Add the custom draw event callback for X-axis labels
        lv_obj_add_event_cb(ui_tempChart, chart_x_axis_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
        
        // In newer LVGL, invalidation is often sufficient. refresh_ext_draw_size is for older versions or complex cases.
        // lv_obj_refresh_ext_draw_size(ui_tempChart); 
        lv_obj_invalidate(ui_tempChart); // Mark object for redraw

        Serial.println("Temperature Chart Initialized for Time-based X-Axis.");
    } else {
        Serial.println("Error: ui_tempChart object not found!");
    }

    // Initialize timing
    lastTempReadTime = millis();
    lastSampleTime = millis(); // Start timers now

    // Initial Temperature Read and Label Update
    currentTemperature = readTemperatureSensor();
    if (ui_tempLabel != NULL) { // Check if label object exists
        if (!isnan(currentTemperature)) {
            snprintf(tempLabelBuffer, sizeof(tempLabelBuffer), "%.1f C", currentTemperature);
            lv_label_set_text(ui_tempLabel, tempLabelBuffer);
        } else {
            lv_label_set_text(ui_tempLabel, "--.- C"); // Show placeholder if initial read fails
        }
    }
}


// --- Update Function (Call this REPEATEDLY in your main loop()) ---
void updateTemperatureSystem() {
    unsigned long currentTimeMs = millis();

    // --- 1. Read Temperature and Update Label (Frequently) ---
    if (currentTimeMs - lastTempReadTime >= TEMP_READ_INTERVAL) {
        lastTempReadTime = currentTimeMs;

        float newTempReading = readTemperatureSensor(); // Get the latest reading

        if (!isnan(newTempReading)) {
            currentTemperature = newTempReading; // Update global currentTemperature only if valid
            snprintf(tempLabelBuffer, sizeof(tempLabelBuffer), "%.1f C", currentTemperature);
            if (ui_tempLabel != NULL) { // Check if label object exists
                lv_label_set_text(ui_tempLabel, tempLabelBuffer);
            }
        } else {
            // Temperature reading failed (NAN)
            // Keep 'currentTemperature' as its last valid value or initial NAN
            if (ui_tempLabel != NULL) { // Check if label object exists
                lv_label_set_text(ui_tempLabel, "Err C"); // Indicate error on label
            }
            Serial.println("Sensor read returned NAN for label update.");
        }
    }

    // --- 2. Store Sample and Update Graph (Infrequently) ---
    if (currentTimeMs - lastSampleTime >= TEMP_SAMPLE_INTERVAL) {
        lastSampleTime = currentTimeMs;

        time_t currentEpochTime = time(NULL); // Get time from system clock (hopefully set by NTP)
        bool isTimeCurrentlyValid = (currentEpochTime > MIN_VALID_EPOCH_TIME);

        // Shift all existing historical samples to the left to make space for the new one at the end.
        // historicalSamples[0] is discarded, historicalSamples[MAX_TEMP_SAMPLES-1] is the new slot.
        for (int i = 0; i < MAX_TEMP_SAMPLES - 1; i++) {
            historicalSamples[i] = historicalSamples[i + 1];
        }

        // Prepare the newest sample data in the last slot of the array
        int newestSampleIndex = MAX_TEMP_SAMPLES - 1;

        if (!isnan(currentTemperature) && isTimeCurrentlyValid) {
            // Valid temperature reading AND valid system time
            Serial.print("Sampling temperature: "); Serial.print(currentTemperature);
            Serial.print(" at valid time: "); Serial.println(currentEpochTime);

            historicalSamples[newestSampleIndex].temperature = currentTemperature;
            historicalSamples[newestSampleIndex].timestamp = currentEpochTime;
            historicalSamples[newestSampleIndex].isValidTimestamp = true;

            if (ui_tempChart != NULL && ui_tempChartSeries != NULL) {
                lv_coord_t chartValue = (lv_coord_t)round(currentTemperature);
                lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, chartValue);
            }
        } else {
            // Either temperature is NAN or time is not (yet) valid
            if (isnan(currentTemperature)) Serial.println("Skipping valid sample storage: currentTemperature is NAN.");
            if (!isTimeCurrentlyValid) Serial.println("Skipping valid sample storage: currentEpochTime is not (yet) valid.");

            // Store placeholder data in historicalSamples
            historicalSamples[newestSampleIndex].temperature = isnan(currentTemperature) ? NAN : currentTemperature; // Store NAN or last good temp
            historicalSamples[newestSampleIndex].timestamp = currentEpochTime; // Store time anyway, even if invalid
            historicalSamples[newestSampleIndex].isValidTimestamp = false;     // Mark timestamp as not usable for labeling

            // Update chart with a placeholder or last known good value
            if (ui_tempChart != NULL && ui_tempChartSeries != NULL) {
                lv_coord_t y_value_for_chart;
                if (isnan(currentTemperature)) {
                    y_value_for_chart = CHART_Y_MIN_VALUE; // If temp is NAN, plot at min_y
                } else {
                    y_value_for_chart = (lv_coord_t)round(currentTemperature); // Otherwise plot the (potentially old) currentTemperature
                }
                lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, y_value_for_chart);
            }
        }

        // This counter is less critical now that historicalSamples always reflects the chart's state
        // but can be useful for debugging or knowing when the array is "full" of attempts.
        if (validSamplesCurrentlyStored < MAX_TEMP_SAMPLES) {
            validSamplesCurrentlyStored++;
        }
        
        // Important: Invalidate the chart to trigger a redraw, which will call our custom X-axis label drawer
        if(ui_tempChart != NULL) {
            lv_obj_invalidate(ui_tempChart);
        }
    }
}
