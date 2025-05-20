// web_server.h
#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h> // For String, etc.

// Function to initialize the web server
// Needs to be called after WiFi is connected.
void initialize_web_server();

// Function to handle incoming web client requests
// Needs to be called in the main M7 loop.
void handle_web_server_clients();

// Function to tell the web server that WiFi status has changed
// (e.g., if WiFi drops, the server might need to stop or indicate unavailability)
void notify_web_server_wifi_status(bool connected);

#endif // WEB_SERVER_H
