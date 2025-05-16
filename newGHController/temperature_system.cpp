// temperature_system.cpp
#include "temperature_system.h"
#include "config.h"
#include "ui.h"           // For ui_tempLabel, ui_tempChart (from SquareLine)
#include <stdio.h>        // For snprintf
#include <math.h>         // For isnan, round, NAN
#include <time.h>         // For time_t, struct tm, strftime, time()
#include <mbed_mktime.h>  // For _rtc_localtime on Portenta/Giga

// --- Global Variables for this module ---
lv_chart_series_t *ui_tempChartSeries = NULL;
float currentTemperature = NAN;
TempSampleData historicalSamples[MAX_TEMP_SAMPLES];
int validSamplesCurrentlyStored = 0; // Can be removed if not strictly needed

unsigned long lastTempReadTime = 0;
unsigned long lastSampleTime = 0;
char tempLabelBuffer[32];


// --- Placeholder Sensor Function ---
// !! REPLACE THIS with your actual sensor reading code !!
float readTemperatureSensor() {
    // --- Example Simulation (Remove this block) ---
    static float simulatedTempBase = 22.0;
    int change = rand() % 3 - 1;
    simulatedTempBase += (float)change * 0.5f;
    if (simulatedTempBase < CHART_Y_MIN_VALUE + 2) simulatedTempBase = CHART_Y_MIN_VALUE + 2;
    if (simulatedTempBase > CHART_Y_MAX_VALUE - 2) simulatedTempBase = CHART_Y_MAX_VALUE - 2;
    // return simulatedTempBase + (rand() % 100) / 150.0f;
    // For testing NAN
    // if ((rand() % 10) == 0) return NAN; 
    return 20.0 + (rand() % 100) / 20.0; // Simpler range for testing
    // --- End Simulation ---
}

// --- Custom Draw Event for Chart X-Axis Tick Labels ---
static void chart_x_axis_draw_event_cb(lv_event_t * e) {
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);

    if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_X && dsc->text != NULL) {
        int chart_point_index = dsc->value;

        if (chart_point_index >= 0 && chart_point_index < MAX_TEMP_SAMPLES &&
            historicalSamples[chart_point_index].isValidTimestamp) {
            time_t sample_ts = historicalSamples[chart_point_index].timestamp;
            struct tm timeinfo;
            
            // Apply timezone for display if timestamps are UTC
            time_t display_ts = sample_ts + (3600L * NTP_TIMEZONE);
            _rtc_localtime(display_ts, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);

            char time_str_buffer[6]; // "HH:MM\0"
            strftime(time_str_buffer, sizeof(time_str_buffer), "%H:%M", &timeinfo);
            lv_snprintf(dsc->text, dsc->text_length, "%s", time_str_buffer);
        } else {
            lv_snprintf(dsc->text, dsc->text_length, "-:-");
        }
    }
}


void initializeTemperatureSystem() {
    for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
        historicalSamples[i].temperature = NAN;
        historicalSamples[i].timestamp = 0;
        historicalSamples[i].isValidTimestamp = false;
    }
    validSamplesCurrentlyStored = 0;

    if (ui_tempChart != NULL) {
        lv_chart_set_type(ui_tempChart, LV_CHART_TYPE_LINE);
        lv_chart_set_update_mode(ui_tempChart, LV_CHART_UPDATE_MODE_SHIFT); // Data shifts left
        lv_chart_set_point_count(ui_tempChart, MAX_TEMP_SAMPLES);
        lv_chart_set_range(ui_tempChart, LV_CHART_AXIS_PRIMARY_Y, CHART_Y_MIN_VALUE, CHART_Y_MAX_VALUE);

        ui_tempChartSeries = lv_chart_add_series(ui_tempChart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
        if (!ui_tempChartSeries) {
            Serial.println("Error: Failed to add series to chart!");
            return;
        }

        // Initialize chart points
        for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
            lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, CHART_Y_MIN_VALUE);
        }
        // After filling with set_next_value for initialization in SHIFT mode,
        // the "next" point is effectively the oldest. Subsequent set_next_value calls will replace it.

        uint8_t num_x_major_ticks = 5;
        lv_coord_t major_tick_len = 7;
        lv_coord_t minor_tick_len = 4;
        uint8_t num_minor_ticks_between_major = (MAX_TEMP_SAMPLES / num_x_major_ticks) > 1 ? (MAX_TEMP_SAMPLES / num_x_major_ticks) / 2 -1 : 0; // Auto calc minor
        bool draw_labels_for_major_ticks = true;
        lv_coord_t extra_draw_space_for_labels = 25;

        lv_chart_set_axis_tick(ui_tempChart, LV_CHART_AXIS_PRIMARY_X,
                       major_tick_len, minor_tick_len,
                       num_x_major_ticks, num_minor_ticks_between_major,
                       draw_labels_for_major_ticks, extra_draw_space_for_labels);
        
        // Optional: Style X-axis tick labels
        // lv_obj_set_style_text_font(ui_tempChart, &lv_font_montserrat_10, LV_PART_TICKS | LV_STATE_DEFAULT);
        // lv_obj_set_style_text_color(ui_tempChart, lv_color_hex(0x808080), LV_PART_TICKS | LV_STATE_DEFAULT);

        lv_obj_add_event_cb(ui_tempChart, chart_x_axis_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
        lv_obj_invalidate(ui_tempChart);
        Serial.println("Temperature Chart Initialized.");
    } else {
        Serial.println("Error: ui_tempChart object not found!");
    }

    lastTempReadTime = millis();
    lastSampleTime = millis();

    currentTemperature = readTemperatureSensor();
    if (ui_tempLabel != NULL) {
        if (!isnan(currentTemperature)) {
            snprintf(tempLabelBuffer, sizeof(tempLabelBuffer), "%.1f C", currentTemperature);
            lv_label_set_text(ui_tempLabel, tempLabelBuffer);
        } else {
            lv_label_set_text(ui_tempLabel, "--.- C");
        }
    }
}

void updateTemperatureSystem() {
    unsigned long currentTimeMs = millis();

    if (currentTimeMs - lastTempReadTime >= TEMP_READ_INTERVAL_MS) {
        lastTempReadTime = currentTimeMs;
        float newTempReading = readTemperatureSensor();

        if (!isnan(newTempReading)) {
            currentTemperature = newTempReading;
            if (ui_tempLabel != NULL) {
                snprintf(tempLabelBuffer, sizeof(tempLabelBuffer), "%.1f C", currentTemperature);
                lv_label_set_text(ui_tempLabel, tempLabelBuffer);
            }
        } else {
            if (ui_tempLabel != NULL) {
                lv_label_set_text(ui_tempLabel, "Err C");
            }
            // Serial.println("Sensor read NAN for label."); // Can be noisy
        }
    }

    if (currentTimeMs - lastSampleTime >= TEMP_SAMPLE_INTERVAL_MS) {
        lastSampleTime = currentTimeMs;
        time_t currentEpochTimeUTC = time(NULL); // System RTC should be UTC
        bool isTimeCurrentlyValid = (currentEpochTimeUTC > MIN_VALID_EPOCH_TIME);

        // Shift historical samples
        for (int i = 0; i < MAX_TEMP_SAMPLES - 1; i++) {
            historicalSamples[i] = historicalSamples[i + 1];
        }
        int newestSampleIndex = MAX_TEMP_SAMPLES - 1;

        if (!isnan(currentTemperature)) { // Use the last good reading for sampling
            historicalSamples[newestSampleIndex].temperature = currentTemperature;
            historicalSamples[newestSampleIndex].timestamp = currentEpochTimeUTC; // Store UTC
            historicalSamples[newestSampleIndex].isValidTimestamp = isTimeCurrentlyValid;

            if (ui_tempChart != NULL && ui_tempChartSeries != NULL) {
                lv_coord_t chartValue = (lv_coord_t)round(currentTemperature);
                // Ensure chartValue is within Y range, or LVGL might clip/behave unexpectedly
                if (chartValue < CHART_Y_MIN_VALUE) chartValue = CHART_Y_MIN_VALUE;
                if (chartValue > CHART_Y_MAX_VALUE) chartValue = CHART_Y_MAX_VALUE;
                lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, chartValue);
            }
             if(isTimeCurrentlyValid) {
                Serial.print("Sampled: "); Serial.print(currentTemperature); Serial.println(" C");
            } else {
                Serial.print("Sampled (no valid time): "); Serial.print(currentTemperature); Serial.println(" C");
            }
        } else { // currentTemperature is NAN
            historicalSamples[newestSampleIndex].temperature = NAN; // Store NAN
            historicalSamples[newestSampleIndex].timestamp = currentEpochTimeUTC;
            historicalSamples[newestSampleIndex].isValidTimestamp = false; // Mark as invalid sample point for chart label

            if (ui_tempChart != NULL && ui_tempChartSeries != NULL) {
                // Plot NAN as min value on chart, or some other indicator
                lv_chart_set_next_value(ui_tempChart, ui_tempChartSeries, CHART_Y_MIN_VALUE);
            }
            Serial.println("Skipped sample: currentTemperature is NAN.");
        }

        if (ui_tempChart != NULL) {
            lv_obj_invalidate(ui_tempChart); // Trigger redraw for new point and labels
        }
    }
}
