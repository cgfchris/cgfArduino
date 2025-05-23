// web_server.cpp
#include "web_server.h"
#include <Arduino.h>
#include "config.h"
#include "wifi_manager.h"
#include "ntp_time.h"
#include "GreenhouseSettingsStruct.h" // Include the shared struct definition
#include <WiFi.h>
#include <RPC.h>
#include <stdio.h>

WiFiServer M7webServer(80);
bool serverIsInitialized_ws = false;
bool wifiIsCurrentlyConnected_ws = false;

extern float m4_reported_temperature;
extern int m4_vent_stage;
extern bool m4_heater_state;
extern bool m4_shade_state;
extern bool m4_boost_state;
extern GreenhouseSettings m4_settings_cache;

// initialize_web_server() and notify_web_server_wifi_status() remain the same

void initialize_web_server() {
    if (is_wifi_connected()) { 
        M7webServer.begin();
        Serial.println("WebServer: Started on port 80.");
        Serial.print("WebServer: URL -> http://");
        Serial.println(WiFi.localIP());
        serverIsInitialized_ws = true;
        wifiIsCurrentlyConnected_ws = true;
    } else {
        Serial.println("WebServer: WiFi not connected at init. Server not started.");
        serverIsInitialized_ws = false;
        wifiIsCurrentlyConnected_ws = false;
    }
}

void notify_web_server_wifi_status(bool connected) {
    wifiIsCurrentlyConnected_ws = connected;
    if (connected && !serverIsInitialized_ws) {
        M7webServer.begin();
        Serial.println("WebServer: Re-started on port 80 after WiFi reconnect.");
        serverIsInitialized_ws = true;
    } else if (!connected && serverIsInitialized_ws) {
        Serial.println("WebServer: WiFi connection lost. Server may be unreachable.");
    }
}

String urlDecode(String str) {
    str.replace("%20", " ");
    return str;
}

// --- Helper function to call M4 setter for a float value ---
static bool sendFloatSettingToM4(const char* rpcFunctionName, float newValue, float &m7CacheValue) {
    try {
        RPC.call(rpcFunctionName, newValue);
        m7CacheValue = newValue; // Optimistically update cache if call doesn't throw
        Serial.println("WebServer-DBG: RPC " + String(rpcFunctionName) + "(" + String(newValue) + ") sent. M7 cache updated.");
        return true;
    } catch (const std::exception& e) {
        Serial.println("WebServer-DBG: Exception on RPC call for " + String(rpcFunctionName) + ": " + String(e.what()));
        return false;
    }
}

// --- Helper function to call M4 setter for a H:M time value ---
static bool sendTimeSettingToM4(const char* rpcFunctionName, uint8_t newHour, uint8_t newMinute, uint8_t &m7CacheHour, uint8_t &m7CacheMinute) {
    try {
        RPC.call(rpcFunctionName, newHour, newMinute);
        m7CacheHour = newHour;     // Optimistically update cache
        m7CacheMinute = newMinute; // Optimistically update cache
        Serial.println("WebServer-DBG: RPC " + String(rpcFunctionName) + "(" + String(newHour) + ":" + String(newMinute) + ") sent. M7 cache updated.");
        return true;
    } catch (const std::exception& e) {
        Serial.println("WebServer-DBG: Exception on RPC call for " + String(rpcFunctionName) + ": " + String(e.what()));
        return false;
    }
}

// --- Helper function to call M4 setter for a uint16_t value (like duration) ---
static bool sendUint16SettingToM4(const char* rpcFunctionName, uint16_t newValue, uint16_t &m7CacheValue) {
    try {
        RPC.call(rpcFunctionName, newValue);
        m7CacheValue = newValue; // Optimistically update cache
        Serial.println("WebServer-DBG: RPC " + String(rpcFunctionName) + "(" + String(newValue) + ") sent. M7 cache updated.");
        return true;
    } catch (const std::exception& e) {
        Serial.println("WebServer-DBG: Exception on RPC call for " + String(rpcFunctionName) + ": " + String(e.what()));
        return false;
    }
}
void handle_web_server_clients() {
    if (!serverIsInitialized_ws || !wifiIsCurrentlyConnected_ws) {
        return; 
    }

    WiFiClient client = M7webServer.available();

    if (client) {
        Serial.println("\nWebServer-DBG: Client connected. IP: " + client.remoteIP().toString());
        String currentHeaderLine = ""; 
        unsigned long requestStartTime = millis();
        bool requestHeadersFullyRead = false;
        String httpRequestMethod = "";
        String httpRequestPathAndParams = ""; 

        Serial.println("WebServer-DBG: Reading client request headers...");
        int headerLineCount = 0;
        bool firstLineProcessed = false;

        while (client.connected() && (millis() - requestStartTime < 3000)) { // Read headers timeout
            if (client.available()) {
                char c = client.read();
                // Serial.write(c); // Raw dump - enable for deep debug if needed

                if (c == '\n') {
                    headerLineCount++;
                    // Serial.print("WebServer-DBG: Header Line " + String(headerLineCount) + ": '"); Serial.print(currentHeaderLine); Serial.println("'");

                    if (!firstLineProcessed) { 
                        if (currentHeaderLine.startsWith("GET ")) {
                            httpRequestMethod = "GET";
                            httpRequestPathAndParams = currentHeaderLine.substring(4); 
                            if (httpRequestPathAndParams.indexOf(" HTTP/1.1") != -1) {
                                httpRequestPathAndParams.remove(httpRequestPathAndParams.indexOf(" HTTP/1.1"));
                            }
                            httpRequestPathAndParams.trim();
                        } else if (currentHeaderLine.startsWith("POST ")) {
                            httpRequestMethod = "POST";
                            httpRequestPathAndParams = currentHeaderLine.substring(5);
                            if (httpRequestPathAndParams.indexOf(" HTTP/1.1") != -1) {
                                httpRequestPathAndParams.remove(httpRequestPathAndParams.indexOf(" HTTP/1.1"));
                            }
                            httpRequestPathAndParams.trim();
                        } 
                        firstLineProcessed = true;
                        Serial.print("WebServer-DBG: FirstLine Parsed - Method: '" + httpRequestMethod + "', PathAndParams: '" + httpRequestPathAndParams + "'\n");
                    }

                    if (currentHeaderLine.length() == 0) { 
                        Serial.println("WebServer-DBG: End of HTTP headers detected.");
                        requestHeadersFullyRead = true;
                        break; 
                    }
                    currentHeaderLine = ""; 
                } else if (c != '\r') {
                    currentHeaderLine += c;
                }
            }
        } // End while (reading headers)

        if (!requestHeadersFullyRead) {
            Serial.println("WebServer-DBG: Request headers not fully read (timed out or client disconnected).");
            client.stop();
            Serial.println("WebServer-DBG: Client disconnected (incomplete headers).");
            return;
        }
        
        Serial.println("WebServer-DBG: Processing Request - Method: '" + httpRequestMethod + "', Path: '" + httpRequestPathAndParams + "'");
        bool sentSpecificResponse = false;

        if (httpRequestMethod == "GET") {
            if (httpRequestPathAndParams == "/favicon.ico") {
                Serial.println("WebServer-DBG: Favicon.ico request. Sending 204 No Content.");
                client.println("HTTP/1.1 204 No Content");
                client.println("Connection: close");
                client.println();
                sentSpecificResponse = true;
            } else if (httpRequestPathAndParams == "/" || httpRequestPathAndParams.startsWith("/index.html") || httpRequestPathAndParams.length() == 0) {
                // Serve a VERY simple, mostly static HTML page for this test
                Serial.println("WebServer-DBG: Serving MINIMAL STATIC test HTML page.");
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html; charset=UTF-8");
                client.println("Connection: close");
                client.println();
                client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Greenhouse Test (Minimal)</title>");
                client.println("<style>body{font-family:Helvetica,Arial,sans-serif; margin:20px;}</style></head><body>");
                client.println("<h1>Greenhouse Controller - Minimal Web Test</h1>");
                client.println("<p>This is a static test page to check web server stability.</p>");
                client.println("<p>If you see this, the basic HTTP serving is working.</p>");
                // You can add one simple dynamic piece of M7 data if you like:
                client.print("<p>M7 Uptime: "); client.print(millis()/1000); client.println(" seconds.</p>");
                client.println("</body></html>");
                sentSpecificResponse = true;
            } else {
                // Unknown GET request path
                Serial.println("WebServer-DBG: Unknown GET path: '" + httpRequestPathAndParams + "'. Sending 404.");
                client.println("HTTP/1.1 404 Not Found");
                client.println("Content-Type: text/html");
                client.println("Connection: close");
                client.println();
                client.println("<!DOCTYPE HTML><html><body><h1>404 Not Found</h1><p>The requested URL was not found on this server.</p></body></html>");
                sentSpecificResponse = true;
            }
        } else if (httpRequestMethod.length() > 0) { // If method was parsed but not GET
            Serial.println("WebServer-DBG: Unsupported HTTP method: '" + httpRequestMethod + "'. Sending 405.");
            client.println("HTTP/1.1 405 Method Not Allowed");
            client.println("Allow: GET"); 
            client.println("Content-Length: 0");
            client.println("Connection: close");
            client.println();
            sentSpecificResponse = true;
        }

        if (!sentSpecificResponse) {
            // This case should ideally not be hit if all GETs are handled or 404'd,
            // and non-GET methods are 405'd. But as a fallback:
            Serial.println("WebServer-DBG: Request fully read but not specifically handled. Path: '" + httpRequestPathAndParams + "'. Sending generic 404.");
            client.println("HTTP/1.1 404 Not Found");
            client.println("Content-Type: text/html");
            client.println("Connection: close");
            client.println();
            client.println("<!DOCTYPE HTML><html><body><h1>404 Not Found</h1><p>Resource not specifically handled.</p></body></html>");
        }
        delay(5); 
        client.stop();
        Serial.println("WebServer-DBG: Client disconnected.");
    } // end if(client)
}

void handle_web_server_clients_basic() {
    if (!serverIsInitialized_ws || !wifiIsCurrentlyConnected_ws) {
        return; 
    }

    WiFiClient client = M7webServer.available();
    if (client) {
        Serial.println("WebServer-DBG: Minimal handler - client connected, immediately closing.");
        client.println("HTTP/1.1 503 Service Unavailable"); // Or 200 OK with tiny message
        client.println("Connection: close");
        client.println();
        client.stop(); 
    }
    // No complex header reading, no HTML generation, no RPC calls from here.
}

void handle_web_server_clients_full() {
    if (!serverIsInitialized_ws || !wifiIsCurrentlyConnected_ws) {
        return;
    }

    WiFiClient client = M7webServer.available();

    if (client) {
        Serial.println("\nWebServer-DBG: Client connected. IP: " + client.remoteIP().toString());
        String currentHeaderLine = ""; // Used for accumulating each header line
        unsigned long requestStartTime = millis();
        bool requestHeadersFullyRead = false;
        String httpRequestMethod = "";
        String httpRequestPathAndParams = ""; // Will store the path and query string from the first line

        Serial.println("WebServer-DBG: Reading client request headers...");
        int headerLineCount = 0;
        bool firstLineProcessed = false;

        while (client.connected() && (millis() - requestStartTime < 3000)) { // Read headers timeout
            if (client.available()) {
                char c = client.read();
                // Serial.write(c); // Raw dump of all incoming characters (very verbose)

                if (c == '\n') {
                    headerLineCount++;
                    // Serial.print("WebServer-DBG: Header Line " + String(headerLineCount) + ": '"); Serial.print(currentHeaderLine); Serial.println("'");

                    if (!firstLineProcessed) { // This is the first line (Request-Line)
                        if (currentHeaderLine.startsWith("GET ")) {
                            httpRequestMethod = "GET";
                            httpRequestPathAndParams = currentHeaderLine.substring(4); // Remove "GET "
                            if (httpRequestPathAndParams.indexOf(" HTTP/1.1") != -1) {
                                httpRequestPathAndParams.remove(httpRequestPathAndParams.indexOf(" HTTP/1.1"));
                            }
                            httpRequestPathAndParams.trim();
                        } else if (currentHeaderLine.startsWith("POST ")) {
                            httpRequestMethod = "POST";
                            // Extract path for POST, query params might be in body
                            httpRequestPathAndParams = currentHeaderLine.substring(5);
                            if (httpRequestPathAndParams.indexOf(" HTTP/1.1") != -1) {
                                httpRequestPathAndParams.remove(httpRequestPathAndParams.indexOf(" HTTP/1.1"));
                            }
                            httpRequestPathAndParams.trim();
                        } // Add other methods if needed
                        firstLineProcessed = true;
                        Serial.print("WebServer-DBG: FirstLine Parsed - Method: '" + httpRequestMethod + "', PathAndParams: '" + httpRequestPathAndParams + "'\n");
                    }

                    if (currentHeaderLine.length() == 0) { // Blank line (EOH)
                        Serial.println("WebServer-DBG: End of HTTP headers detected.");
                        requestHeadersFullyRead = true;
                        break; 
                    }
                    currentHeaderLine = ""; // Reset for next header line
                } else if (c != '\r') {
                    currentHeaderLine += c;
                    // No arbitrary length check on currentHeaderLine here, String will grow.
                    // Be mindful of RAM if extremely long headers are possible, but unlikely for this.
                }
            }
        } // End while (reading headers)

        if (!requestHeadersFullyRead) {
            Serial.println("WebServer-DBG: Request headers not fully read (timed out or client disconnected).");
            client.stop();
            Serial.println("WebServer-DBG: Client disconnected (incomplete headers).");
            return;
        }

        // Ensure we parsed something for method and path if headers were read
        if (httpRequestMethod.length() == 0 && httpRequestPathAndParams.length() == 0 && firstLineProcessed) {
             Serial.println("WebServer-DBG: First line was processed but resulted in empty method/path. Possibly very short/malformed first line.");
        }
        Serial.println("WebServer-DBG: Processing Request - Method: '" + httpRequestMethod + "', Path: '" + httpRequestPathAndParams + "'");

        bool sentSpecificResponse = false;

        if (httpRequestMethod == "GET") {
            if (httpRequestPathAndParams == "/favicon.ico") {
                Serial.println("WebServer-DBG: Favicon.ico request. Sending 204 No Content.");
                client.println("HTTP/1.1 204 No Content");
                client.println("Connection: close");
                client.println();
                sentSpecificResponse = true;
            } else if (httpRequestPathAndParams.startsWith("/set?")) {
                RPC.println("M4: Web server received settings change request via GET.");
                String paramsStr = httpRequestPathAndParams.substring(httpRequestPathAndParams.indexOf('?') + 1);
                
                const int8_t NOT_SET_S8 = -1; 
                const int16_t NOT_SET_S16 = -1; 
                const float NOT_SET_F = NAN;  

                // Temporary holders for parsed values from the form (same as before)
                float parsed_vent1_temp = NOT_SET_F, parsed_vent2_temp = NOT_SET_F, parsed_vent3_temp = NOT_SET_F;
                float parsed_heat_day_temp = NOT_SET_F, parsed_heat_night_temp = NOT_SET_F, parsed_heat_boost_temp = NOT_SET_F;
                float parsed_hysteresis = NOT_SET_F;
                int8_t parsed_day_start_h = NOT_SET_S8, parsed_day_start_m = NOT_SET_S8;
                int8_t parsed_night_start_h = NOT_SET_S8, parsed_night_start_m = NOT_SET_S8;
                int8_t parsed_boost_start_h = NOT_SET_S8, parsed_boost_start_m = NOT_SET_S8;
                int16_t parsed_boost_dur = NOT_SET_S16;
                int8_t parsed_shade_open_h = NOT_SET_S8, parsed_shade_open_m = NOT_SET_S8;
                int8_t parsed_shade_close_h = NOT_SET_S8, parsed_shade_close_m = NOT_SET_S8;

                // Parameter Parsing Loop (same as before - fills parsed_... variables)
                // ...
                int currentParamStartIdx = 0;
                while(currentParamStartIdx < paramsStr.length()){
                    int nextAmpersandIdx = paramsStr.indexOf('&', currentParamStartIdx);
                    String singleParamPair;
                    if(nextAmpersandIdx != -1){
                        singleParamPair = paramsStr.substring(currentParamStartIdx, nextAmpersandIdx);
                        currentParamStartIdx = nextAmpersandIdx + 1;
                    } else {
                        singleParamPair = paramsStr.substring(currentParamStartIdx);
                        currentParamStartIdx = paramsStr.length(); 
                    }
                    int equalsPos = singleParamPair.indexOf('=');
                    if(equalsPos != -1){
                        String paramName = singleParamPair.substring(0, equalsPos);
                        String paramValue = urlDecode(singleParamPair.substring(equalsPos + 1));
                        // Serial.println("WebServer-DBG: Parsed Param from Web: " + paramName + " = '" + paramValue + "'");

                        if (paramName == "vent1_temp") parsed_vent1_temp = paramValue.toFloat();
                        else if (paramName == "vent2_temp") parsed_vent2_temp = paramValue.toFloat();
                        else if (paramName == "vent3_temp") parsed_vent3_temp = paramValue.toFloat();
                        else if (paramName == "heat_day_temp") parsed_heat_day_temp = paramValue.toFloat();
                        else if (paramName == "heat_night_temp") parsed_heat_night_temp = paramValue.toFloat();
                        else if (paramName == "heat_boost_temp") parsed_heat_boost_temp = paramValue.toFloat();
                        else if (paramName == "hysteresis") parsed_hysteresis = paramValue.toFloat();
                        else if (paramName == "day_start_h") parsed_day_start_h = paramValue.toInt();
                        else if (paramName == "day_start_m") parsed_day_start_m = paramValue.toInt();
                        else if (paramName == "night_start_h") parsed_night_start_h = paramValue.toInt();
                        else if (paramName == "night_start_m") parsed_night_start_m = paramValue.toInt();
                        else if (paramName == "boost_start_h") parsed_boost_start_h = paramValue.toInt();
                        else if (paramName == "boost_start_m") parsed_boost_start_m = paramValue.toInt();
                        else if (paramName == "boost_dur") parsed_boost_dur = paramValue.toInt();
                        else if (paramName == "shade_open_h") parsed_shade_open_h = paramValue.toInt();
                        else if (paramName == "shade_open_m") parsed_shade_open_m = paramValue.toInt();
                        else if (paramName == "shade_close_h") parsed_shade_close_h = paramValue.toInt();
                        else if (paramName == "shade_close_m") parsed_shade_close_m = paramValue.toInt();
                    }
                }
                // ...

                // --- Make RPC calls using helpers and Update M7 Cache Immediately ---
                if (!isnan(parsed_vent1_temp)) sendFloatSettingToM4("setVentTempS1", parsed_vent1_temp, m4_settings_cache.ventOpenTempStage1);
                if (!isnan(parsed_vent2_temp)) sendFloatSettingToM4("setVentTempS2", parsed_vent2_temp, m4_settings_cache.ventOpenTempStage2);
                if (!isnan(parsed_vent3_temp)) sendFloatSettingToM4("setVentTempS3", parsed_vent3_temp, m4_settings_cache.ventOpenTempStage3);
                if (!isnan(parsed_heat_day_temp)) sendFloatSettingToM4("setHeatTempDay", parsed_heat_day_temp, m4_settings_cache.heatSetTempDay);
                if (!isnan(parsed_heat_night_temp)) sendFloatSettingToM4("setHeatTempNight", parsed_heat_night_temp, m4_settings_cache.heatSetTempNight);
                if (!isnan(parsed_heat_boost_temp)) sendFloatSettingToM4("setHeatBoostTemp", parsed_heat_boost_temp, m4_settings_cache.heatBoostTemp);
                if (!isnan(parsed_hysteresis)) sendFloatSettingToM4("setHysteresis", parsed_hysteresis, m4_settings_cache.hysteresis);
                
                if (parsed_day_start_h != NOT_SET_S8 || parsed_day_start_m != NOT_SET_S8) {
                    sendTimeSettingToM4("setDayStartTime", 
                                        (parsed_day_start_h != NOT_SET_S8 ? (uint8_t)parsed_day_start_h : m4_settings_cache.dayStartHour), 
                                        (parsed_day_start_m != NOT_SET_S8 ? (uint8_t)parsed_day_start_m : m4_settings_cache.dayStartMinute),
                                        m4_settings_cache.dayStartHour, m4_settings_cache.dayStartMinute);
                }
                if (parsed_night_start_h != NOT_SET_S8 || parsed_night_start_m != NOT_SET_S8) {
                    sendTimeSettingToM4("setNightStartTime", 
                                        (parsed_night_start_h != NOT_SET_S8 ? (uint8_t)parsed_night_start_h : m4_settings_cache.nightStartHour), 
                                        (parsed_night_start_m != NOT_SET_S8 ? (uint8_t)parsed_night_start_m : m4_settings_cache.nightStartMinute),
                                        m4_settings_cache.nightStartHour, m4_settings_cache.nightStartMinute);
                }
                if (parsed_boost_start_h != NOT_SET_S8 || parsed_boost_start_m != NOT_SET_S8) {
                    sendTimeSettingToM4("setBoostStartTime",
                                        (parsed_boost_start_h != NOT_SET_S8 ? (uint8_t)parsed_boost_start_h : m4_settings_cache.boostStartHour),
                                        (parsed_boost_start_m != NOT_SET_S8 ? (uint8_t)parsed_boost_start_m : m4_settings_cache.boostStartMinute),
                                        m4_settings_cache.boostStartHour, m4_settings_cache.boostStartMinute);
                }
                if (parsed_boost_dur != NOT_SET_S16) {
                    sendUint16SettingToM4("setBoostDuration", (uint16_t)parsed_boost_dur, m4_settings_cache.boostDurationMinutes);
                }
                if (parsed_shade_open_h != NOT_SET_S8 || parsed_shade_open_m != NOT_SET_S8) {
                    sendTimeSettingToM4("setShadeOpenTime",
                                        (parsed_shade_open_h != NOT_SET_S8 ? (uint8_t)parsed_shade_open_h : m4_settings_cache.shadeOpenHour),
                                        (parsed_shade_open_m != NOT_SET_S8 ? (uint8_t)parsed_shade_open_m : m4_settings_cache.shadeOpenMinute),
                                        m4_settings_cache.shadeOpenHour, m4_settings_cache.shadeOpenMinute);
                }
                if (parsed_shade_close_h != NOT_SET_S8 || parsed_shade_close_m != NOT_SET_S8) {
                    sendTimeSettingToM4("setShadeCloseTime",
                                        (parsed_shade_close_h != NOT_SET_S8 ? (uint8_t)parsed_shade_close_h : m4_settings_cache.shadeCloseHour),
                                        (parsed_shade_close_m != NOT_SET_S8 ? (uint8_t)parsed_shade_close_m : m4_settings_cache.shadeCloseMinute),
                                        m4_settings_cache.shadeCloseHour, m4_settings_cache.shadeCloseMinute);
                }
                Serial.println("WebServer-DBG: Finished attempting to send settings updates to M4.");
                
                client.println("HTTP/1.1 302 Found"); client.println("Location: /"); client.println("Connection: close"); client.println();
                sentSpecificResponse = true;
            } 
            else if (httpRequestPathAndParams == "/" || httpRequestPathAndParams.startsWith("/index.html") || httpRequestPathAndParams.length() == 0) {
                Serial.println("WebServer-DBG: Serving main HTML page.");
                client.println("HTTP/1.1 200 OK"); client.println("Content-type:text/html"); client.println("Connection: close"); client.println();
                client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Greenhouse Control</title>");
                client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
                // client.println("<meta http-equiv='refresh' content='10'>"); // Keep refresh off if debug needed
                client.println("<style>body{font-family:Helvetica,Arial,sans-serif; margin:15px; background-color:#f0f2f5;}");
                client.println("h1,h2{color:#333;} table{border-collapse:collapse; width:auto; margin-bottom:25px; background-color:white; box-shadow: 0 1px 3px rgba(0,0,0,0.12), 0 1px 2px rgba(0,0,0,0.24);} ");
                client.println("th,td{border:1px solid #ddd; padding:8px 12px; text-align:left;}");
                client.println("th{background-color:#4CAF50; color:white; font-weight:bold;} .form-table td {border: none; padding: 5px;} "); // Matched padding
                client.println("input[type=number], input[type=text]{padding:6px; width:60px; border:1px solid #ccc; border-radius:3px;} input[type=submit]{background-color:#4CAF50; color:white; padding:10px 18px; border:none; cursor:pointer; border-radius:4px; font-size:1em;}");
                client.println("input[type=submit]:hover{background-color:#45a049;}</style></head><body><div class='container'>");
                client.println("<h1>Greenhouse Controller</h1>");
                char timeBuf[12], dateBuf[24];
                get_formatted_local_time(timeBuf, sizeof(timeBuf)); get_formatted_local_date(dateBuf, sizeof(dateBuf)); 
                client.print("<p>Time: <strong>"); client.print(timeBuf); client.print("</strong>   Date: <strong>"); client.print(dateBuf); client.print("</strong></p>");
                // --- ADD MANUAL REFRESH BUTTON ---
                client.println("<button class='refresh-button' onclick='location.reload();'>Refresh Status & Settings</button>");
                // --- END MANUAL REFRESH BUTTON ---
                client.println("<h2>Current Status</h2><table><tr><th>Parameter</th><th>Value</th></tr>");
                client.print("<tr><td>Temperature</td><td>"); if (!isnan(m4_reported_temperature)) { client.print(m4_reported_temperature, 1); client.print(" °C"); } else { client.print("N/A"); } client.println("</td></tr>");
                client.print("<tr><td>Vents</td><td>"); if (m4_vent_stage == 0) client.print("Closed"); else if (m4_vent_stage == 1) client.print("Stage 1 (25%)"); else if (m4_vent_stage == 2) client.print("Stage 2 (50%)"); else if (m4_vent_stage == 3) client.print("Stage 3 (100%)"); else client.print("N/A"); client.println("</td></tr>");
                client.print("<tr><td>Heater</td><td>"); client.print(m4_heater_state ? "ON" : "OFF"); if(m4_heater_state && m4_boost_state) client.print(" (Boost Active)"); client.println("</td></tr>");
                client.print("<tr><td>Shade</td><td>"); client.print(m4_shade_state ? "OPEN" : "CLOSED"); client.println("</td></tr>");
                client.println("</table>");
                client.println("<h2>Settings Control</h2>");
                client.println("<form action='/set' method='GET'><table class='form-table'>");
                client.print("<tr><td>Vent S1 Temp (°C):</td><td><input type='number' step='0.1' name='vent1_temp' value='"); client.print(m4_settings_cache.ventOpenTempStage1, 1); client.println("'></td></tr>");
                client.print("<tr><td>Vent S2 Temp (°C):</td><td><input type='number' step='0.1' name='vent2_temp' value='"); client.print(m4_settings_cache.ventOpenTempStage2, 1); client.println("'></td></tr>");
                client.print("<tr><td>Vent S3 Temp (°C):</td><td><input type='number' step='0.1' name='vent3_temp' value='"); client.print(m4_settings_cache.ventOpenTempStage3, 1); client.println("'></td></tr>");
                client.print("<tr><td>Heat Day Temp (°C):</td><td><input type='number' step='0.1' name='heat_day_temp' value='"); client.print(m4_settings_cache.heatSetTempDay, 1); client.println("'></td></tr>");
                client.print("<tr><td>Heat Night Temp (°C):</td><td><input type='number' step='0.1' name='heat_night_temp' value='"); client.print(m4_settings_cache.heatSetTempNight, 1); client.println("'></td></tr>");
                client.print("<tr><td>Heat Boost Temp (°C):</td><td><input type='number' step='0.1' name='heat_boost_temp' value='"); client.print(m4_settings_cache.heatBoostTemp, 1); client.println("'></td></tr>");
                client.print("<tr><td>Hysteresis (°C):</td><td><input type='number' step='0.1' name='hysteresis' value='"); client.print(m4_settings_cache.hysteresis, 1); client.println("'></td></tr>");
                client.print("<tr><td>Day Start (HH:MM):</td><td><input type='number' name='day_start_h' min='0' max='23' style='width:40px;' value='"); client.print(m4_settings_cache.dayStartHour); client.print("'> : <input type='number' name='day_start_m' min='0' max='59' style='width:40px;' value='"); client.print(m4_settings_cache.dayStartMinute); client.println("'></td></tr>");
                client.print("<tr><td>Night Start (HH:MM):</td><td><input type='number' name='night_start_h' min='0' max='23' style='width:40px;' value='"); client.print(m4_settings_cache.nightStartHour); client.print("'> : <input type='number' name='night_start_m' min='0' max='59' style='width:40px;' value='"); client.print(m4_settings_cache.nightStartMinute); client.println("'></td></tr>");
                client.print("<tr><td>Boost Start (HH:MM):</td><td><input type='number' name='boost_start_h' min='0' max='23' style='width:40px;' value='"); client.print(m4_settings_cache.boostStartHour); client.print("'> : <input type='number' name='boost_start_m' min='0' max='59' style='width:40px;' value='"); client.print(m4_settings_cache.boostStartMinute); client.println("'></td></tr>");
                client.print("<tr><td>Boost Duration (min):</td><td><input type='number' name='boost_dur' min='0' value='"); client.print(m4_settings_cache.boostDurationMinutes); client.println("'></td></tr>");
                client.print("<tr><td>Shade Open (HH:MM):</td><td><input type='number' name='shade_open_h' min='0' max='23' style='width:40px;' value='"); client.print(m4_settings_cache.shadeOpenHour); client.print("'> : <input type='number' name='shade_open_m' min='0' max='59' style='width:40px;' value='"); client.print(m4_settings_cache.shadeOpenMinute); client.println("'></td></tr>");
                client.print("<tr><td>Shade Close (HH:MM):</td><td><input type='number' name='shade_close_h' min='0' max='23' style='width:40px;' value='"); client.print(m4_settings_cache.shadeCloseHour); client.print("'> : <input type='number' name='shade_close_m' min='0' max='59' style='width:40px;' value='"); client.print(m4_settings_cache.shadeCloseMinute); client.println("'></td></tr>");
                client.println("<tr><td colspan='2' style='text-align:center;'><input type='submit' value='Apply Settings'></td></tr>");
                client.println("</table></form></div></body></html>");
                sentSpecificResponse = true;
            } else {
                Serial.println("WebServer-DBG: Unknown GET path: '" + httpRequestPathAndParams + "'. Sending 404.");
                client.println("HTTP/1.1 404 Not Found"); client.println("Content-Type: text/html"); client.println("Connection: close"); client.println();
                client.println("<!DOCTYPE HTML><html><body><h1>404 Not Found</h1><p>The requested URL was not found.</p></body></html>");
                sentSpecificResponse = true;
            }
        } else if (httpRequestMethod.length() > 0) { 
            Serial.println("WebServer-DBG: Unsupported HTTP method: '" + httpRequestMethod + "'. Sending 405.");
            client.println("HTTP/1.1 405 Method Not Allowed"); client.println("Allow: GET"); client.println("Content-Length: 0"); client.println("Connection: close"); client.println();
            sentSpecificResponse = true;
        }

        if (!sentSpecificResponse) {
            Serial.println("WebServer-DBG: Request fully read but not handled. Path: '" + httpRequestPathAndParams + "'. Sending 404.");
            client.println("HTTP/1.1 404 Not Found"); client.println("Content-Type: text/html"); client.println("Connection: close"); client.println();
            client.println("<!DOCTYPE HTML><html><body><h1>404 Not Found</h1><p>Resource not handled.</p></body></html>");
        }
        delay(5);
        client.stop();
        Serial.println("WebServer-DBG: Client disconnected.");
    } // end if(client)
}
