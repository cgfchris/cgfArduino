/*
 Udp NTP Client with Timezone Adjustment

 Get the time from a Network Time Protocol (NTP) time server
 Demonstrates use of UDP sendPacket and ReceivePacket
 For more on NTP time servers and the messages needed to communicate with them,
 see http://en.wikipedia.org/wiki/Network_Time_Protocol

 created 4 Sep 2010
 by Michael Margolis
 modified 9 Apr 2012
 by Tom Igoe
 modified 28 Dec 2022
 by Giampaolo Mancini
 modified 29 Jan 2024
 by Karl Söderby

This code is in the public domain.
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbed_mktime.h>

int timezone = -4; //this is GMT -1. 

int status = WL_IDLE_STATUS;

char ssid[] = "devries";        // your network SSID (name)
char pass[] = "devriesWireless";  // your network password (use for WPA, or use as key for WEP)

int keyIndex = 0;  // your network key index number (needed only for WEP)

unsigned int localPort = 2390;  // local port to listen for UDP packets

// IPAddress timeServer(162, 159, 200, 123); // pool.ntp.org NTP server

constexpr auto timeServer{ "pool.ntp.org" };

const int NTP_PACKET_SIZE = 48;  // NTP timestamp is in the first 48 bytes of the message

byte packetBuffer[NTP_PACKET_SIZE];  // buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

constexpr unsigned long printInterval{ 1000 };
unsigned long printNow{};

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

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
}

void loop() {
  if (millis() > printNow) {
    Serial.print("System Clock:          ");
    Serial.println(getLocaltime());
    printNow = millis() + printInterval;
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
  
  const unsigned long new_epoch = epoch + (3600 * timezone); //multiply the timezone with 3600 (1 hour)

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

String getLocaltime() {
  char buffer[32];
  tm t;
  _rtc_localtime(time(NULL), &t, RTC_FULL_LEAP_YEAR_SUPPORT);
  strftime(buffer, 32, "%Y-%m-%d %k:%M:%S", &t);
  return String(buffer);
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
