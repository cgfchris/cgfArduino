// ntp_time.h
#ifndef NTP_TIME_H
#define NTP_TIME_H

#include <stddef.h> // For size_t
#include <RTClib.h> // <<<< ADD FOR RTC LIBRARY

// ... (other constants: NTP_SERVER, NTP_LOCAL_PORT, etc. as before) ...
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
#define NTP_TIMEZONE 0
#endif
#ifndef NTP_PRINT_INTERVAL_MS
#define NTP_PRINT_INTERVAL_MS 1000
#endif
#ifndef MIN_VALID_EPOCH_TIME
#define MIN_VALID_EPOCH_TIME 1609459200UL // Jan 1, 2021 UTC
#endif
#ifndef NTP_RETRY_INTERVAL_MS
#define NTP_RETRY_INTERVAL_MS (1 * 60 * 1000UL) // Retry every 1 minute if not synced
#endif
#ifndef NTP_SYNC_INTERVAL_MS
#define NTP_SYNC_INTERVAL_MS (6 * 60 * 60 * 1000UL) // Normal NTP sync every 6 hours
#endif


// Function Declarations
void initialize_ntp_and_rtc(); // Renamed for clarity
void update_time_management();   // Renamed for clarity (handles UI update and periodic NTP sync)

int get_current_hour();
int get_current_minute();
int get_current_second();
bool is_time_valid();      // Changed from is_ntp_synced()

void get_formatted_local_time(char* buffer, size_t buffer_size);
void get_formatted_local_date(char* buffer, size_t buffer_size);

// Internal functions (usually not called directly from main .ino)
void send_ntp_packet(const char* address);
void parse_ntp_response();

#endif // NTP_TIME_H
