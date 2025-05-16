// wifi_manager.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h> // For String, delay, etc.
#include <WiFi.h>    // For WiFi related functions

// Function to initialize WiFi connection
void initialize_wifi();

// Function to print WiFi status (optional, if you want to call it manually)
void print_wifi_status();

// Function to update any WiFi related UI elements (e.g., status label)
void update_wifi_status_display();

// Expose status if needed by other modules (less ideal, prefer functions)
// extern int wifi_status;

#endif // WIFI_MANAGER_H
