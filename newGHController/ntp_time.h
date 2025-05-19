// ntp_time.h
#ifndef NTP_TIME_H
#define NTP_TIME_H

#include <stddef.h> // For size_t

// Constants (can also be in config.h if preferred)
#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif
#ifndef NTP_LOCAL_PORT
#define NTP_LOCAL_PORT 2390
#endif
#ifndef NTP_PACKET_BUFFER_SIZE
#define NTP_PACKET_BUFFER_SIZE 48
#endif
#ifndef NTP_TIMEZONE
#define NTP_TIMEZONE 0 // Default to UTC if not defined in config.h
#endif
#ifndef NTP_PRINT_INTERVAL_MS
#define NTP_PRINT_INTERVAL_MS 1000 // How often to update UI time display
#endif
#ifndef MIN_VALID_EPOCH_TIME
#define MIN_VALID_EPOCH_TIME 1609459200UL // Jan 1, 2021 UTC (used to check if RTC is set)
#endif
#ifndef NTP_RETRY_INTERVAL_MS
#define NTP_RETRY_INTERVAL_MS (1 * 60 * 1000UL) // Retry every 1 minute if not synced
#endif
#ifndef NTP_SYNC_INTERVAL_MS
#define NTP_SYNC_INTERVAL_MS (1 * 60 * 60 * 1000UL) // Normal sync every 1 hour
#endif


// Function Declarations
void initialize_ntp();
void send_ntp_packet(const char* address); // If needed externally, usually not
void parse_ntp_response();               // If needed externally, usually not
void update_ntp_time_display();          // Main function called from M7 loop

// New helper functions for main .ino to use for M4 communication
int get_current_hour();
int get_current_minute();
int get_current_second(); // Optional, if M4 needs it
bool is_ntp_synced();    // To check if valid time is available

// These are mainly for internal use by update_ntp_time_display but could be exposed
void get_formatted_local_time(char* buffer, size_t buffer_size);
void get_formatted_local_date(char* buffer, size_t buffer_size);


#endif // NTP_TIME_H
