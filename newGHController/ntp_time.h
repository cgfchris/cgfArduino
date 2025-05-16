// ntp_time.h
#ifndef NTP_TIME_H
#define NTP_TIME_H

#include <Arduino.h>

// Function to initialize NTP and set system time
void initialize_ntp();

// Function to update time display on UI
void update_ntp_time_display();

// Helper to get local time string (HH:MM:SS)
void get_formatted_local_time(char* buffer, size_t buffer_size);

// Helper to get local date string (Mon DD, YYYY)
void get_formatted_local_date(char* buffer, size_t buffer_size);

// Function to send an NTP request
void send_ntp_packet(const char* address);

// Function to parse NTP response and set time
void parse_ntp_response();


#endif // NTP_TIME_H
