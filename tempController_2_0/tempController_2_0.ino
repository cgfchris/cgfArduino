
/*
  LVGLDemo

  created 17 Apr 2023
  by Leonardo Cavagnis
*/

#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbed_mktime.h>

Arduino_H7_Video          Display(800, 480, GigaDisplayShield); 
Arduino_GigaDisplayTouch  TouchDetector;

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
// LVGL objects for time display
static lv_obj_t *timeLabel;

/* Button click event callback */
static void btn_event_cb(lv_event_t * e) {
  static uint32_t cnt = 1;
  lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);
  lv_obj_t * label = lv_obj_get_child(btn, 0);
  lv_label_set_text_fmt(label, "%"LV_PRIu32, cnt);
  cnt++;
}

void setup() {
  Serial.begin(115200);

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("Connected to WiFi");
  printWifiStatus();
  setNtpTime();
  
  
  Display.begin();
  TouchDetector.begin();

  // timer for time update
  static unsigned long lastTime = 0;
  lastTime = millis();

  /* Create a container with grid 2x2 */
  static lv_coord_t col_dsc[] = {370, 370, LV_GRID_TEMPLATE_LAST};
  static lv_coord_t row_dsc[] = {215, 215, LV_GRID_TEMPLATE_LAST};
  lv_obj_t * cont = lv_obj_create(lv_scr_act());
  lv_obj_set_grid_dsc_array(cont, col_dsc, row_dsc);
  lv_obj_set_size(cont, Display.width(), Display.height());
  lv_obj_set_style_bg_color(cont, lv_color_hex(0x03989e), LV_PART_MAIN);
  lv_obj_center(cont);

  lv_obj_t * label;
  lv_obj_t * obj;

  /* [0;0] - Image */
  obj = lv_obj_create(cont);
  lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 0, 1,
                        LV_GRID_ALIGN_STRETCH, 0, 1);

  LV_IMG_DECLARE(cgfLogo);
  lv_obj_t * img1 = lv_img_create(obj);
  lv_img_set_src(img1, &cgfLogo);
  lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_size(img1, 300, 173);

  /* [1;0] - Checkboxes and button */
  obj = lv_obj_create(cont);
  lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 1, 1,
                        LV_GRID_ALIGN_STRETCH, 0, 1);
  lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_COLUMN);

  lv_obj_t * cb;
  cb = lv_checkbox_create(obj);
  lv_checkbox_set_text(cb, "Apple");

  cb = lv_checkbox_create(obj);
  lv_checkbox_set_text(cb, "Banana");
  lv_obj_add_state(cb, LV_STATE_CHECKED);

  static lv_style_t style_radio;
  static lv_style_t style_radio_chk;
  lv_style_init(&style_radio);
  lv_style_set_radius(&style_radio, LV_RADIUS_CIRCLE);
  lv_style_init(&style_radio_chk);
  #if (LVGL_VERSION_MAJOR == 9)
  lv_style_set_bg_image_src(&style_radio_chk, NULL);
  #else
  lv_style_set_bg_img_src(&style_radio_chk, NULL);
  #endif
  
  cb = lv_checkbox_create(obj);
  lv_checkbox_set_text(cb, "Lemon");
  lv_obj_add_flag(cb, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_style(cb, &style_radio, LV_PART_INDICATOR);
  lv_obj_add_style(cb, &style_radio_chk, LV_PART_INDICATOR | LV_STATE_CHECKED);
  
  cb = lv_checkbox_create(obj);
  lv_checkbox_set_text(cb, "Melon");
  lv_obj_add_flag(cb, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_add_style(cb, &style_radio, LV_PART_INDICATOR);
  lv_obj_add_style(cb, &style_radio_chk, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_add_state(cb, LV_STATE_CHECKED);

  lv_obj_t * btn = lv_btn_create(obj);
  lv_obj_set_size(btn, 100, 40);
  lv_obj_center(btn);
  lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, NULL);

  label = lv_label_create(btn);
  lv_label_set_text(label, "Click me!");
  lv_obj_center(label);

  /* [0;1] - Slider */
  obj = lv_obj_create(cont);
  lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 0, 1,
                        LV_GRID_ALIGN_STRETCH, 1, 1);
  
  lv_obj_t * slider = lv_slider_create(obj);
  lv_slider_set_value(slider, 75, LV_ANIM_OFF);
  lv_obj_center(slider);
  label = lv_label_create(obj);
  lv_label_set_text(label, "Drag me!");
  lv_obj_align_to(label, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
  
  /* [1;1] - Time */
  obj = lv_obj_create(cont);
  lv_obj_set_grid_cell(obj, LV_GRID_ALIGN_STRETCH, 1, 1,
                        LV_GRID_ALIGN_STRETCH, 1, 1);
  char timeString[32];
  getLocaltime(timeString);
  timeLabel = lv_label_create(obj);
  lv_label_set_text(timeLabel, timeString);
  
}

void loop() { 
  /* Feed LVGL engine */
  lv_timer_handler();
  char timeString[32];
  // serial print time
  if (millis() > printNow) {
    Serial.print("System Clock:          ");
    getLocaltime(timeString);
    Serial.println(timeString);
    printNow = millis() + printInterval;
    lv_label_set_text(timeLabel, timeString);
  }
  
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
  strftime(buffer, 32, "%Y-%m-%d %k:%M:%S", &t);
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
