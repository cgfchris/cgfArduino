// wifi_manager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

// Function to initialize WiFi connection (called once in setup)
void initialize_wifi();

// Function to print WiFi status (optional for debugging)
void print_wifi_status();

// Function to be called in the main loop to manage WiFi connection and UI
void manage_wifi_connection(); // Changed from update_wifi_status_display

// Function to check if WiFi is currently connected
bool is_wifi_connected();

#endif // WIFI_MANAGER_H
