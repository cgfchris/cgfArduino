#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"
#include <ui.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbed_mktime.h>

Arduino_H7_Video Display( 800, 480, GigaDisplayShield ); //( 800, 480, GigaDisplayShield );
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

// --- temp Configuration ---
const unsigned long TEMP_READ_INTERVAL = 2000;     // How often to read temp & update label (milliseconds)
//const unsigned long TEMP_SAMPLE_INTERVAL = 5 * 60 * 1000; // How often to store a sample for the graph (5 minutes)
const unsigned long TEMP_SAMPLE_INTERVAL = 10 * 1000; // Faster interval for TESTING (10 seconds)
const int MAX_TEMP_SAMPLES = 100;                  // Number of samples to store and plot

lv_chart_series_t *ui_tempChartSeries; // Pointer for the chart data series
// Temperature Data
float currentTemperature = 0.0f;
float tempSamples[MAX_TEMP_SAMPLES]; // Array to store historical temperature data
int tempSampleCount = 0;            // How many valid samples are currently stored
// Buffer for label formatting
char tempLabelBuffer[32];
// Timing
unsigned long lastTempReadTime = 0;
unsigned long lastSampleTime = 0;

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

// --- Initialization (Call this ONCE in your setup() after ui_init()) ---
void initializeTemperatureSystem() {
  // Initialize the sample array with a value indicating no data yet
  // NAN (Not a Number) is suitable if using floats. LVGL chart might ignore NAN.
  // Alternatively, use 0 or a value outside your expected range.
  for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
    tempSamples[i] = NAN; // Or 0.0f;
  }
  tempSampleCount = 0; // Reset sample count

  // Check if the chart object exists (created by ui_init)
  if (ui_tempChart != NULL) {
    // Configure the chart appearance (optional customization)
    lv_chart_set_type(ui_tempChart, LV_CHART_TYPE_LINE); // Or LV_CHART_TYPE_BAR
    lv_chart_set_update_mode(ui_tempChart, LV_CHART_UPDATE_MODE_SHIFT); // Makes adding new points easy
    lv_chart_set_point_count(ui_tempChart, MAX_TEMP_SAMPLES); // Tell chart how many points to expect
    lv_chart_set_div_line_count(ui_tempChart, 5, 5); // Example: 5 horizontal, 5 vertical division lines

    // Set the Y-axis range (Adjust min/max to your expected temperature range in C or F)
    lv_chart_set_range(ui_tempChart, LV_CHART_AXIS_PRIMARY_Y, 0, 50); // Example: 10°C to 35°C

    // Add a data series to the chart
    ui_tempChartSeries = lv_chart_add_series(ui_tempChart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    // Initialize chart points (set all initially to the bottom of the range or NAN if handled)
    // lv_chart_set_all_value(ui_tempChart, ui_tempChartSeries, 0); // Set all points to 0 initially
    // Or, if using NAN, you might need to manually set points if NAN isn't drawn nicely
    for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
         lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, -100.0); // Set initial points to min range
    }


    // Refresh the chart to apply changes
    //lv_obj_refresh_ext_draw_size(ui_tempChart); // Deprecated in newer LVGL
    lv_obj_invalidate(ui_tempChart); // Mark object for redraw

    Serial.println("Temperature Chart Initialized.");

  } else {
     Serial.println("Error: ui_tempChart object not found!");
  }

  // Initialize timing
  lastTempReadTime = millis();
  lastSampleTime = millis(); // Start timers now

   // Initial Temperature Read and Label Update
   currentTemperature = readTemperatureSensor();
   if (ui_tempLabel != NULL && !isnan(currentTemperature)) {
       snprintf(tempLabelBuffer, sizeof(tempLabelBuffer), "%.1f C", currentTemperature); // Format to 1 decimal place
       lv_label_set_text(ui_tempLabel, tempLabelBuffer);
   } else if (ui_tempLabel != NULL) {
        lv_label_set_text(ui_tempLabel, "--.- C"); // Show placeholder if initial read fails
   }
}

// --- Update Function (Call this REPEATEDLY in your main loop()) ---
void updateTemperatureSystem() {
  unsigned long currentTime = millis();

  // --- 1. Read Temperature and Update Label (Frequently) ---
  if (currentTime - lastTempReadTime >= TEMP_READ_INTERVAL) {
    lastTempReadTime = currentTime;

    currentTemperature = readTemperatureSensor(); // Get the latest reading

    // Check if the reading is valid (not NAN)
    if (!isnan(currentTemperature)) {
      // Format the temperature string (e.g., "23.5 C")
      // Use %d if your sensor gives integers, %.1f for 1 decimal, %.2f for 2 etc.
      snprintf(tempLabelBuffer, sizeof(tempLabelBuffer), "%.1f C", currentTemperature); // Change "C" to "F" if needed

      // Update the LVGL label (check if it exists)
      if (ui_tempLabel != NULL) {
        lv_label_set_text(ui_tempLabel, tempLabelBuffer);
         // Serial.print("Updated Temp Label: "); Serial.println(tempLabelBuffer); // Debug
      } else {
         // Serial.println("Warning: ui_tempLabel is NULL!"); // Debug
      }
    } else {
       // Handle failed sensor reading on the label
        if (ui_tempLabel != NULL) {
            lv_label_set_text(ui_tempLabel, "Err C");
        }
        Serial.println("Sensor read returned NAN."); // Debug
    }
  }

  // --- 2. Store Sample and Update Graph (Infrequently) ---
  if (currentTime - lastSampleTime >= TEMP_SAMPLE_INTERVAL) {
    lastSampleTime = currentTime;

    // Make sure the last read temperature is valid before sampling
    if (!isnan(currentTemperature)) {
      Serial.print("Sampling temperature: "); Serial.println(currentTemperature); // Debug

      // We use the chart's SHIFT update mode. We just need to feed it the next value.
      // LVGL will handle shifting the old data points automatically.
      if (ui_tempChart != NULL && ui_tempChartSeries != NULL) {
         // Convert float temperature to lv_coord_t (usually int16_t).
         // You might need scaling if your range is large or needs high precision,
         // but direct casting often works for typical temperature ranges.
         lv_coord_t chartValue = (lv_coord_t)round(currentTemperature); // Round to nearest integer for chart

         // Add the new value to the chart; LVGL shifts the old ones
         lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, chartValue);

         // Optional: Refresh the chart immediately if needed, but lv_timer_handler should cover it
         // lv_obj_invalidate(ui_tempChart);

         Serial.print("Added to chart: "); Serial.println(chartValue); // Debug
      } else {
         Serial.println("Warning: ui_tempChart or ui_tempChartSeries is NULL!"); // Debug
      }


      // --- Optional: Store in our own array too (if needed for other calculations) ---
      // This part is now less critical if the chart handles the history via SHIFT mode,
      // but you might want the raw float data accessible elsewhere.
      if (tempSampleCount < MAX_TEMP_SAMPLES) {
          // Still filling the array initially
          tempSamples[tempSampleCount] = currentTemperature;
          tempSampleCount++;
      } else {
          // Array is full, shift elements manually to discard the oldest
          // (More complex than letting the chart handle it with SHIFT mode)
          for (int i = 0; i < MAX_TEMP_SAMPLES - 1; i++) {
              tempSamples[i] = tempSamples[i + 1];
          }
          tempSamples[MAX_TEMP_SAMPLES - 1] = currentTemperature;
      }
      // --- End Optional Manual Array Storage ---

    } else {
       Serial.println("Skipping sample due to invalid temperature reading (NAN).");
       // Optional: Add a 'gap' or placeholder value to the chart here if desired
       // if (ui_tempChart != NULL && ui_tempChartSeries != NULL) {
       //    lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, lv_chart_get_y_range(ui_tempChart, LV_CHART_AXIS_PRIMARY_Y).min); // Add point at min value
       // }
    }
  }
}


float readTemperatureSensor() {
  // --- Example Simulation (Remove this block) ---
  // Simulates a fluctuating temperature for testing
  // Returns degrees Celsius
  float simulatedTemp = 20.0 + 5.0 * sin(millis() / 30000.0);
  return simulatedTemp;
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
