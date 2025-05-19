// temperature_system.cpp
#include <Arduino.h>             // For Serial, isnan, round, etc.
#include "temperature_system.h"  // Corresponding header
#include "ntp_time.h"
#include "config.h"              // For constants like CHART_Y_MIN_VALUE, etc.
#include "ui.h"                  // For ui_tempChart (LVGL chart object from SquareLine)
#include <stdio.h>               // For snprintf (if not covered by Arduino.h for all targets)
#include <math.h>                // For isnan, round (often part of Arduino.h via C++ headers)
#include <time.h>                // For time_t, struct tm, strftime
#include <mbed_mktime.h>         // For _rtc_localtime on Giga/Portenta

// --- Global Variables for this module ---
lv_chart_series_t *ui_tempChartSeries = NULL; // LVGL Chart series pointer
float currentTemperature_local = NAN;         // This module's copy of temperature, updated from M4 via new function
TempSampleData historicalSamples[MAX_TEMP_SAMPLES]; // Array to store historical temp data for the chart

unsigned long lastSampleTime = 0;             // To track when to take the next sample for the chart

// --- Sensor Function (NO LONGER THE PRIMARY SOURCE FOR UI DISPLAY) ---
// This function is effectively unused if M4 provides the temperature.
// Kept for structure if M7 ever needs its own independent temperature reading.
float readM7OwnTemperatureSensor_placeholder() {
    // If M7 had its own sensor for some other purpose, its reading logic would be here.
    // Returning a dummy value.
    return 21.5f;
}

// --- Custom Draw Event for Chart X-Axis Tick Labels (From your original code) ---
static void chart_x_axis_draw_event_cb(lv_event_t * e) {
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);

    // Check if it's the X-axis primary ticks and text is being drawn
    if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_X && dsc->text != NULL) {
        int chart_point_index = dsc->value; // Index of the point for which tick is drawn

        // Ensure the index is valid and we have a valid timestamp for it
        if (chart_point_index >= 0 && chart_point_index < MAX_TEMP_SAMPLES &&
            historicalSamples[chart_point_index].isValidTimestamp) {
            
            time_t sample_ts = historicalSamples[chart_point_index].timestamp;
            struct tm timeinfo;
            
            // Apply timezone for display if timestamps are stored as UTC
            // NTP_TIMEZONE should be defined in config.h or ntp_time.h
            time_t display_ts = sample_ts + (3600L * NTP_TIMEZONE);
            _rtc_localtime(display_ts, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);

            char time_str_buffer[6]; // Buffer for "HH:MM\0"
            strftime(time_str_buffer, sizeof(time_str_buffer), "%H:%M", &timeinfo);
            lv_snprintf(dsc->text, dsc->text_length, "%s", time_str_buffer);
        } else {
            // If no valid timestamp, display a placeholder
            lv_snprintf(dsc->text, dsc->text_length, "-:-");
        }
    }
}

// --- Function to update this module's temperature based on data from M4 ---
// This function is called by the main M7 .ino file after fetching temperature from M4.
void updateCurrentTemperatureFromM4(float m4_temp) {
    if (!isnan(m4_temp)) {
        currentTemperature_local = m4_temp;
    } else {
        currentTemperature_local = NAN; // Propagate NAN if M4 sends it or RPC fails
    }
    // For debugging:
    // Serial.print("M7-TempSys: Internal temperature updated to: ");
    // if(isnan(currentTemperature_local)) Serial.println("NAN"); else Serial.println(currentTemperature_local);
}

void initializeTemperatureSystem() {
    // Initialize historical samples array
    for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
        historicalSamples[i].temperature = NAN;
        historicalSamples[i].timestamp = 0;
        historicalSamples[i].isValidTimestamp = false;
    }

    // Initialize the LVGL chart (ui_tempChart should be created by ui_init())
    if (ui_tempChart != NULL) {
        lv_chart_set_type(ui_tempChart, LV_CHART_TYPE_LINE);
        lv_chart_set_update_mode(ui_tempChart, LV_CHART_UPDATE_MODE_SHIFT); // Data shifts left
        lv_chart_set_point_count(ui_tempChart, MAX_TEMP_SAMPLES);
        // CHART_Y_MIN_VALUE and CHART_Y_MAX_VALUE should be in config.h or temperature_system.h
        lv_chart_set_range(ui_tempChart, LV_CHART_AXIS_PRIMARY_Y, CHART_Y_MIN_VALUE, CHART_Y_MAX_VALUE);

        // Add a series to the chart
        ui_tempChartSeries = lv_chart_add_series(ui_tempChart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
        if (!ui_tempChartSeries) {
            Serial.println("M7-TempSys: Error! Failed to add series to chart!");
            return; // Critical error, chart won't work
        }

        // Initialize chart points to a baseline (e.g., min value)
        for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
            lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, CHART_Y_MIN_VALUE);
        }

        // Configure X-axis ticks and labels (same as your original)
        uint8_t num_x_major_ticks = 5; // Example: 5 major ticks
        lv_coord_t major_tick_len = 7;
        lv_coord_t minor_tick_len = 4;
        // Auto calculate minor ticks, ensure denominator isn't zero
        uint8_t num_minor_ticks_between_major = (MAX_TEMP_SAMPLES / num_x_major_ticks) > 1 ? (MAX_TEMP_SAMPLES / (num_x_major_ticks +1) ) / 2 -1 : 0;
        num_minor_ticks_between_major = (num_minor_ticks_between_major > 0) ? num_minor_ticks_between_major : 0; // ensure non-negative

        bool draw_labels_for_major_ticks = true;
        lv_coord_t extra_draw_space_for_labels = 25; // Space below X-axis for labels

        lv_chart_set_axis_tick(ui_tempChart, LV_CHART_AXIS_PRIMARY_X,
                       major_tick_len, minor_tick_len,
                       num_x_major_ticks, num_minor_ticks_between_major,
                       draw_labels_for_major_ticks, extra_draw_space_for_labels);
        
        // Add event callback for drawing custom X-axis tick labels
        lv_obj_add_event_cb(ui_tempChart, chart_x_axis_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
        lv_obj_invalidate(ui_tempChart); // Force redraw after setup
        Serial.println("M7-TempSys: Temperature Chart Initialized.");
    } else {
        Serial.println("M7-TempSys: Error! ui_tempChart object (from ui.h) not found!");
    }

    lastSampleTime = millis(); // Initialize timer for chart sampling

    // currentTemperature_local will be updated by data from M4.
    // The main ui_tempLabel (current temperature display) will be updated in the main .ino loop
    // using data fetched directly from M4. This module (temperature_system) primarily manages the chart.
}

void updateTemperatureSystem() { // This function now primarily updates the chart
    unsigned long currentTimeMs = millis();

    // The main current temperature label (ui_tempLabel) is updated in the main .ino file.
    // This function focuses on adding the current M4-sourced temperature to the chart history.

    // TEMP_SAMPLE_INTERVAL_MS should be defined in config.h or temperature_system.h
    if (currentTimeMs - lastSampleTime >= TEMP_SAMPLE_INTERVAL_MS) {
        lastSampleTime = currentTimeMs;
        // MIN_VALID_EPOCH_TIME should be defined in config.h or ntp_time.h
        time_t currentEpochTimeUTC = time(NULL); // Get current system time (should be UTC if NTP sets UTC, or local if NTP sets local)
                                                 // Our NTP module sets local time, so this is local.
                                                 // For chart labels, if we want UTC based labels, we'd need to convert.
                                                 // But chart_x_axis_draw_event_cb already assumes sample_ts is UTC and adds NTP_TIMEZONE for display.
                                                 // So, historicalSamples[].timestamp should store UTC.
                                                 // If time(NULL) returns local time here:
        time_t currentEpochLocal = time(NULL);
        currentEpochTimeUTC = currentEpochLocal - ( (long)NTP_TIMEZONE * 3600L ); // Convert local back to UTC for storage

        bool isTimeCurrentlyValid = (currentEpochTimeUTC > MIN_VALID_EPOCH_TIME) && is_ntp_synced(); // also check ntp_time.h's sync status

        // Shift historical samples to make space for the new one
        for (int i = 0; i < MAX_TEMP_SAMPLES - 1; i++) {
            historicalSamples[i] = historicalSamples[i + 1];
        }
        int newestSampleIndex = MAX_TEMP_SAMPLES - 1;

        if (!isnan(currentTemperature_local)) { // Use the M4-sourced temperature
            historicalSamples[newestSampleIndex].temperature = currentTemperature_local;
            historicalSamples[newestSampleIndex].timestamp = currentEpochTimeUTC; // Store UTC timestamp
            historicalSamples[newestSampleIndex].isValidTimestamp = isTimeCurrentlyValid;

            if (ui_tempChart != NULL && ui_tempChartSeries != NULL) {
                lv_coord_t chartValue = (lv_coord_t)round(currentTemperature_local);
                // Ensure chartValue is within Y range
                if (chartValue < CHART_Y_MIN_VALUE) chartValue = CHART_Y_MIN_VALUE;
                if (chartValue > CHART_Y_MAX_VALUE) chartValue = CHART_Y_MAX_VALUE;
                lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, chartValue);
            }
            // Serial.print("M7-TempSys: Chart Sampled: "); Serial.print(currentTemperature_local); Serial.println(" C");
        } else { // currentTemperature_local from M4 is NAN
            historicalSamples[newestSampleIndex].temperature = NAN; // Store NAN
            historicalSamples[newestSampleIndex].timestamp = currentEpochTimeUTC;
            historicalSamples[newestSampleIndex].isValidTimestamp = false; // Mark as invalid sample point

            if (ui_tempChart != NULL && ui_tempChartSeries != NULL) {
                // Plot NAN as min value on chart, or some other indicator for missing data
                lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, CHART_Y_MIN_VALUE);
            }
            // Serial.println("M7-TempSys: Skipped chart sample: currentTemperature_local (from M4) is NAN.");
        }

        if (ui_tempChart != NULL) {
            lv_obj_invalidate(ui_tempChart); // Trigger redraw for new point and X-axis labels
        }
    }
}
