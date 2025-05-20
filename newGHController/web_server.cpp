// web_server.cpp
#include "web_server.h"
#include "config.h"       // For WIFI_SSID etc. (though WiFi is managed elsewhere)
#include "wifi_manager.h" // For is_wifi_connected()
#include "ntp_time.h"     // For get_formatted_local_time/date
#include <WiFi.h>         // For WiFiServer, WiFiClient, WiFi.localIP()
#include <RPC.h>          // For calling M4 functions
#include <stdio.h>        // For snprintf

// --- Web Server Object ---
WiFiServer M7webServer(80); // Renamed to avoid conflict if "webServer" is too generic
bool serverIsInitialized = false;
bool wifiIsCurrentlyConnected = false; // Local track of WiFi status

// --- Accessing M7 global variables that hold M4 data ---
// These are defined in your main .ino file. Using 'extern' makes them accessible here.
// This is one way to share data; passing structs/params is another.
extern float m4_reported_temperature;
extern int m4_vent_stage;
extern bool m4_heater_state;
extern bool m4_shade_state;
extern bool m4_boost_state;

// If you also want to display current M4 setpoints on the web page form inputs,
// you'll need to make the m4_current_settings_copy (or similar) extern as well,
// or add more M4 RPC getter functions for each setpoint.
// For now, let's assume we'll fetch them if needed or just show input boxes.
// extern GreenhouseSettings m4_current_settings_copy; // Assuming defined in main .ino

// --- Local Buffers ---
char web_temp_buffer[10];
char web_status_buffer[60]; // For combined status line if needed

void initialize_web_server() {
    if (is_wifi_connected()) {
        M7webServer.begin();
        Serial.println("WebServer: Started on port 80.");
        Serial.print("WebServer: URL -> http://");
        Serial.println(WiFi.localIP());
        serverIsInitialized = true;
        wifiIsCurrentlyConnected = true;
    } else {
        Serial.println("WebServer: WiFi not connected. Server not started.");
        serverIsInitialized = false;
        wifiIsCurrentlyConnected = false;
    }
}

void notify_web_server_wifi_status(bool connected) {
    wifiIsCurrentlyConnected = connected;
    if (connected && !serverIsInitialized) {
        // WiFi came up after initial setup, try starting server now
        M7webServer.begin();
        Serial.println("WebServer: Re-started on port 80 after WiFi reconnect.");
        serverIsInitialized = true;
    } else if (!connected && serverIsInitialized) {
        // M7webServer.stop(); // Optional: explicitly stop server if WiFi drops
        // serverIsInitialized = false; // Then re-init when WiFi is back
        Serial.println("WebServer: WiFi connection lost. Server may be unreachable.");
    }
}


void handle_web_server_clients() {
    if (!serverIsInitialized || !wifiIsCurrentlyConnected) {
        return; // Server not running or no WiFi
    }

    WiFiClient client = M7webServer.available();

    if (client) {
        Serial.println("WebServer: New client connected.");
        String currentLine = "";
        unsigned long requestStartTime = millis();
        bool requestFullyReadAndProcessed = false; // Flag to ensure we send a response

        while (client.connected() && (millis() - requestStartTime < 2000)) { // 2-second timeout per client
            if (client.available()) {
                char c = client.read();
                if (c == '\n') {
                    if (currentLine.length() == 0) { // End of HTTP request headers
                        requestFullyReadAndProcessed = true; // Mark to send main page if no settings parsed
                        break; 
                    } else {
                        // Process this header line if needed, e.g., GET /set?...
                        if (currentLine.startsWith("GET ")) {
                            String pathAndParams = currentLine.substring(4); // Remove "GET "
                            pathAndParams.remove(pathAndParams.indexOf(" HTTP/1.1"));
                            pathAndParams.trim();
                            Serial.print("WebServer: Request Path: '"); Serial.print(pathAndParams); Serial.println("'");

                            if (pathAndParams.startsWith("/set?")) {
                                RPC.println("M4: Web server received settings change request.");
                                bool settingChanged = false;
                                // --- Parameter Parsing (Basic) ---
                                // Example: /set?vent1=26.5&day_hr=7&day_min=0
                                // This needs to be robust. A helper function for getParamValue(paramName, requestString) is better.
                                
                                String params = pathAndParams.substring(pathAndParams.indexOf('?') + 1);
                                int ampersandPos;
                                while(params.length() > 0){
                                    ampersandPos = params.indexOf('&');
                                    String singleParam;
                                    if(ampersandPos != -1){
                                        singleParam = params.substring(0, ampersandPos);
                                        params = params.substring(ampersandPos + 1);
                                    } else {
                                        singleParam = params;
                                        params = "";
                                    }

                                    int equalsPos = singleParam.indexOf('=');
                                    if(equalsPos != -1){
                                        String paramName = singleParam.substring(0, equalsPos);
                                        String paramValue = singleParam.substring(equalsPos + 1);

                                        if (paramName == "vent1_temp") {
                                            float val = paramValue.toFloat();
                                            RPC.call("setVentTempS1", val);
                                            settingChanged = true;
                                        } else if (paramName == "vent2_temp") {
                                            // RPC.call("setVentTempS2", paramValue.toFloat()); settingChanged = true;
                                        } else if (paramName == "day_start_hr") {
                                            // uint8_t hr = paramValue.toInt();
                                            // You'll need to parse minute too, then call the combined RPC
                                            // RPC.call("setDayStartTime", hr, min_val); settingChanged = true;
                                        }
                                        // ... Add parsing for ALL your settings ...
                                    }
                                }

                                if(settingChanged) Serial.println("WebServer: One or more settings sent to M4.");

                                // Redirect back to main page after processing
                                client.println("HTTP/1.1 302 Found");
                                client.println("Location: /");
                                client.println("Connection: close");
                                client.println();
                                requestFullyReadAndProcessed = true; // Processed, but response is a redirect
                                break; // Break from while(client.available)
                            } else if (pathAndParams == "/" || pathAndParams.startsWith("/index.html") || pathAndParams.length() == 0) {
                                // Request for main page, fall through to send HTML content
                                requestFullyReadAndProcessed = true; // Will send main page HTML
                                // No break here, let it fall to the outer 'break' for blank line
                            }
                        }
                        currentLine = ""; // Reset for next header line
                    }
                } else if (c != '\r') {
                    currentLine += c;
                }
            }
        } // End while(client.connected)

        if (requestFullyReadAndProcessed) { 
            if (!client.find("Location: /")) { // If it's not already a redirect response
                // Send HTTP headers
                client.println("HTTP/1.1 200 OK");
                client.println("Content-type:text/html");
                client.println("Connection: close");
                client.println(); // Blank line

                // --- Send HTML Content ---
                client.println("<!DOCTYPE html><html><head><title>Greenhouse Control</title>");
                client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
                client.println("<meta http-equiv='refresh' content='10'>");
                client.println("<style>body{font-family:Helvetica,Arial,sans-serif; margin:20px; background-color:#f4f4f4;}");
                client.println("h1{color:#2a772a;} table{border-collapse:collapse; width:auto; margin-bottom:20px; box-shadow: 0 2px 5px rgba(0,0,0,0.1);} ");
                client.println("th,td{border:1px solid #ccc; padding:10px; text-align:left;}");
                client.println("th{background-color:#e9e9e9;} .form-table td {border: none; padding: 4px;} ");
                client.println("input[type=number], input[type=text]{padding:5px; width: 70px;} input[type=submit]{background-color:#4CAF50; color:white; padding:10px 15px; border:none; cursor:pointer; border-radius:4px;}");
                client.println("input[type=submit]:hover{background-color:#45a049;}</style></head><body>");
                client.println("<h1>Greenhouse Controller Status</h1>");

                char timeBuf[12], dateBuf[24];
                get_formatted_local_time(timeBuf, sizeof(timeBuf));
                get_formatted_local_date(dateBuf, sizeof(dateBuf));
                client.print("<p>Time: <strong>"); client.print(timeBuf); client.print("</strong>   Date: <strong>"); client.print(dateBuf); client.print("</strong></p>");

                client.println("<table><tr><th>Parameter</th><th>Value</th></tr>");
                // Temperature
                client.print("<tr><td>Temperature</td><td>");
                if (!isnan(m4_reported_temperature)) { client.print(m4_reported_temperature, 1); client.print(" °C"); } 
                else { client.print("N/A"); }
                client.println("</td></tr>");
                // Vent Status
                client.print("<tr><td>Vents</td><td>");
                if (m4_vent_stage == 0) client.print("Closed");
                else if (m4_vent_stage == 1) client.print("Stage 1 (25%)");
                else if (m4_vent_stage == 2) client.print("Stage 2 (50%)");
                else if (m4_vent_stage == 3) client.print("Stage 3 (100%)");
                else client.print("N/A");
                client.println("</td></tr>");
                // Heater & Boost
                client.print("<tr><td>Heater</td><td>"); client.print(m4_heater_state ? "ON" : "OFF"); 
                if(m4_heater_state && m4_boost_state) client.print(" (Boost Active)");
                client.println("</td></tr>");
                // Shade
                client.print("<tr><td>Shade</td><td>"); client.print(m4_shade_state ? "OPEN" : "CLOSED"); client.println("</td></tr>");
                client.println("</table>");

                client.println("<h2>Settings Control</h2>");
                client.println("<form action='/set' method='GET'><table class='form-table'>");
                // Example: Vent Stage 1 Temp
                // To pre-fill these, M7 needs M4's current setpoints.
                // For now, no pre-fill, or M7 needs to fetch them.
                // float currentM4VentS1 = 0; RPC.call("getM4SetVentTempS1", ¤tM4VentS1); // Example
                client.println("<tr><td>Vent S1 Open (°C):</td><td><input type='number' step='0.1' name='vent1_temp'></td></tr>");
                // ... Add ALL other input fields for your settings struct ...
                // For hour/minute, use two input fields:
                // client.println("<tr><td>Day Start Time (HH:MM):</td><td><input type='number' name='day_start_hr' min='0' max='23'> : <input type='number' name='day_start_min' min='0' max='59'></td></tr>");
                
                client.println("<tr><td colspan='2'><input type='submit' value='Apply Settings'></td></tr>");
                client.println("</table></form>");
                client.println("</body></html>");
            }
        } else {
            Serial.println("WebServer: Client request not fully read or timed out before EOH.");
        }

        delay(5); // Allow client to process
        client.stop();
        Serial.println("WebServer: Client disconnected.");
    } // end if(client)
}
