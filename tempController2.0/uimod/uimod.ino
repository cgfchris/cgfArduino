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

void setup() {
  Serial.begin(115200);
  Display.begin();
  TouchDetector.begin();
  ui_init();
  
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
