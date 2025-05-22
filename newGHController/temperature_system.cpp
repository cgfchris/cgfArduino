// temperature_system.cpp
#include <Arduino.h>
#include "temperature_system.h"
#include "config.h"
#include "ui.h"
#include "ntp_time.h" // For is_time_valid()
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <mbed_mktime.h>

// --- Module-level storage for current states to be charted ---
static int8_t currentChartVentStage = 0;    // From M4's m4_vent_stage
static bool currentChartHeaterState = false; // From M4's m4_heater_state

// --- LVGL Chart Series Pointers ---
lv_chart_series_t *ui_tempChartSeriesTemp = NULL; // For Temperature
lv_chart_series_t *ui_tempChartSeriesVent = NULL; // For Vent State
lv_chart_series_t *ui_tempChartSeriesHeater = NULL; // For Heater State

// ... (currentTemperature_local, historicalSamples, lastSampleTime - same as before) ...
float currentTemperature_local = NAN;
TempSampleData historicalSamples[MAX_TEMP_SAMPLES];
unsigned long lastSampleTime = 0;

static void chart_x_axis_draw_event_cb(lv_event_t * e);

// updateCurrentTemperatureFromM4 - same as before
void updateCurrentTemperatureFromM4(float m4_temp) {
    if (!isnan(m4_temp)) {
        currentTemperature_local = m4_temp;
    } else {
        currentTemperature_local = NAN;
    }
}

// --- NEW: Update functions called by main .ino ---
void updateCurrentVentStageForChart(int m4_vent_stage) {
    if (m4_vent_stage >= 0 && m4_vent_stage <= 3) {
        currentChartVentStage = (int8_t)m4_vent_stage;
    } else {
        currentChartVentStage = 0; // Default to closed if invalid
    }
}

void updateCurrentHeaterStateForChart(bool m4_heater_on) {
    currentChartHeaterState = m4_heater_on;
}


void initializeTemperatureSystem() {
    time_t current_utc_for_init = 0;
    bool time_is_valid_for_init = is_time_valid();
    if (time_is_valid_for_init) {
        time_t current_local_time = time(NULL);
        current_utc_for_init = current_local_time - ((long)NTP_TIMEZONE * 3600L);
    }

    for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
        historicalSamples[i].temperature = NAN;
        if (time_is_valid_for_init) {
            historicalSamples[i].timestamp = current_utc_for_init - ((MAX_TEMP_SAMPLES - 1 - i) * (TEMP_SAMPLE_INTERVAL_MS / 1000L));
            historicalSamples[i].isValidTimestamp = true;
        } else {
            historicalSamples[i].timestamp = 0;
            historicalSamples[i].isValidTimestamp = false;
        }
        historicalSamples[i].ventStateNumeric = 0;   // Default to vent closed
        historicalSamples[i].heaterStateNumeric = 0; // Default to heater OFF
    }

    if (ui_tempChart != NULL) {
        lv_chart_set_type(ui_tempChart, LV_CHART_TYPE_LINE);
        lv_chart_set_update_mode(ui_tempChart, LV_CHART_UPDATE_MODE_SHIFT);
        lv_chart_set_point_count(ui_tempChart, MAX_TEMP_SAMPLES);
        lv_chart_set_range(ui_tempChart, LV_CHART_AXIS_PRIMARY_Y, CHART_Y_MIN_VALUE, CHART_Y_MAX_VALUE);
        // Optionally, a secondary Y axis if scales differ too much, but let's try primary first.
        // lv_chart_set_axis_tick(ui_tempChart, LV_CHART_AXIS_SECONDARY_Y, ...);

        // Temperature Series (Blue)
        ui_tempChartSeriesTemp = lv_chart_add_series(ui_tempChart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
        if(!ui_tempChartSeriesTemp) { Serial.println("M7-TempSys: Error! Failed to add TEMP series!"); return; }

        // Vent Status Series (e.g., Green)
        ui_tempChartSeriesVent = lv_chart_add_series(ui_tempChart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
        if(!ui_tempChartSeriesVent) { Serial.println("M7-TempSys: Error! Failed to add VENT series!"); return; }
        // lv_chart_set_series_opa(ui_tempChart, ui_tempChartSeriesVent, LV_OPA_70); // Make it slightly transparent if desired

        // Heater Status Series (e.g., Red)
        ui_tempChartSeriesHeater = lv_chart_add_series(ui_tempChart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
        if(!ui_tempChartSeriesHeater) { Serial.println("M7-TempSys: Error! Failed to add HEATER series!"); return; }
        // lv_chart_set_series_opa(ui_tempChart, ui_tempChartSeriesHeater, LV_OPA_70);

        // Initialize all chart points to a baseline
        for (int i = 0; i < MAX_TEMP_SAMPLES; i++) {
            lv_chart_set_next_value(ui_tempChart, ui_tempChartSeriesTemp, CHART_Y_MIN_VALUE);
            lv_chart_set_next_value(ui_tempChart, ui_tempChartSeriesVent, (lv_coord_t)VENT_STATE_CHART_BASE + 0);     // Vent closed
            lv_chart_set_next_value(ui_tempChart, ui_tempChartSeriesHeater, (lv_coord_t)HEATER_STATE_CHART_BASE + 0); // Heater OFF
        }
        
        // X-axis tick setup (same as before)
        // ...
        uint8_t num_x_major_ticks = 5;
        lv_coord_t major_tick_len = 7;
        lv_coord_t minor_tick_len = 4;
        uint8_t num_minor_ticks_between_major = (MAX_TEMP_SAMPLES / (num_x_major_ticks +1) ) > 1 ? (MAX_TEMP_SAMPLES / (num_x_major_ticks +1) ) / 2 -1 : 0;
        num_minor_ticks_between_major = (num_minor_ticks_between_major > 0) ? num_minor_ticks_between_major : 0;
        bool draw_labels_for_major_ticks = true;
        lv_coord_t extra_draw_space_for_labels = 25;

        lv_chart_set_axis_tick(ui_tempChart, LV_CHART_AXIS_PRIMARY_X,
                       major_tick_len, minor_tick_len,
                       num_x_major_ticks, num_minor_ticks_between_major,
                       draw_labels_for_major_ticks, extra_draw_space_for_labels);
        
        lv_obj_add_event_cb(ui_tempChart, chart_x_axis_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
        lv_obj_invalidate(ui_tempChart);
        Serial.println("M7-TempSys: Temperature Chart Initialized with Vent & Heater series.");
    } else {
        Serial.println("M7-TempSys: Error! ui_tempChart object not found!");
    }
    lastSampleTime = millis();
}

void updateTemperatureSystem() {
    unsigned long currentTimeMs = millis();

    if (currentTimeMs - lastSampleTime >= TEMP_SAMPLE_INTERVAL_MS) {
        lastSampleTime = currentTimeMs;
        time_t currentEpochLocal = time(NULL);
        time_t currentEpochTimeUTC = currentEpochLocal - ((long)NTP_TIMEZONE * 3600L);
        bool isTimeCurrentlyValid = (currentEpochTimeUTC > MIN_VALID_EPOCH_TIME) && is_time_valid();

        for (int i = 0; i < MAX_TEMP_SAMPLES - 1; i++) {
            historicalSamples[i] = historicalSamples[i + 1];
        }
        int newestSampleIndex = MAX_TEMP_SAMPLES - 1;

        // Temperature data
        if (!isnan(currentTemperature_local)) {
            historicalSamples[newestSampleIndex].temperature = currentTemperature_local;
            if (ui_tempChart != NULL && ui_tempChartSeriesTemp != NULL) {
                lv_coord_t chartValue = (lv_coord_t)round(currentTemperature_local);
                if (chartValue < CHART_Y_MIN_VALUE) chartValue = CHART_Y_MIN_VALUE;
                if (chartValue > CHART_Y_MAX_VALUE) chartValue = CHART_Y_MAX_VALUE;
                lv_chart_set_next_value(ui_tempChart, ui_tempChartSeriesTemp, chartValue);
            }
        } else {
            historicalSamples[newestSampleIndex].temperature = NAN;
            if (ui_tempChart != NULL && ui_tempChartSeriesTemp != NULL) {
                lv_chart_set_next_value(ui_tempChart, ui_tempChartSeriesTemp, CHART_Y_MIN_VALUE);
            }
        }

        // Vent state data
        historicalSamples[newestSampleIndex].ventStateNumeric = currentChartVentStage; // Use the M4-sourced value
        if (ui_tempChart != NULL && ui_tempChartSeriesVent != NULL) {
            // Plot vent stages slightly above the Y-axis baseline for visibility
            // Ensure these values are within your CHART_Y_MIN_VALUE and CHART_Y_MAX_VALUE
            lv_coord_t ventChartVal = (lv_coord_t)(VENT_STATE_CHART_BASE + currentChartVentStage);
            if (ventChartVal > CHART_Y_MAX_VALUE) ventChartVal = CHART_Y_MAX_VALUE; // Cap it
            lv_chart_set_next_value(ui_tempChart, ui_tempChartSeriesVent, ventChartVal);
        }
        
        // Heater state data
        historicalSamples[newestSampleIndex].heaterStateNumeric = currentChartHeaterState ? 1 : 0; // Use M4-sourced
        if (ui_tempChart != NULL && ui_tempChartSeriesHeater != NULL) {
            // Plot heater state. Ensure these values are within Y range and distinct from vent
            lv_coord_t heaterChartVal = (lv_coord_t)(HEATER_STATE_CHART_BASE + (currentChartHeaterState ? 1 : 0));
            if (heaterChartVal > CHART_Y_MAX_VALUE) heaterChartVal = CHART_Y_MAX_VALUE; // Cap it
            lv_chart_set_next_value(ui_tempChart, ui_tempChartSeriesHeater, heaterChartVal);
        }

        // Timestamp
        historicalSamples[newestSampleIndex].timestamp = currentEpochTimeUTC;
        historicalSamples[newestSampleIndex].isValidTimestamp = isTimeCurrentlyValid;

        if (ui_tempChart != NULL) {
            lv_obj_invalidate(ui_tempChart);
        }
    }
}

// chart_x_axis_draw_event_cb - make sure it's still present and correct
static void chart_x_axis_draw_event_cb(lv_event_t * e) {
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    if (dsc->part == LV_PART_TICKS && dsc->id == LV_CHART_AXIS_PRIMARY_X && dsc->text != NULL) {
        int chart_point_index = dsc->value;
        if (chart_point_index >= 0 && chart_point_index < MAX_TEMP_SAMPLES &&
            historicalSamples[chart_point_index].isValidTimestamp) {
            time_t sample_ts = historicalSamples[chart_point_index].timestamp;
            struct tm timeinfo;
            time_t display_ts = sample_ts + (3600L * NTP_TIMEZONE);
            _rtc_localtime(display_ts, &timeinfo, RTC_FULL_LEAP_YEAR_SUPPORT);
            char time_str_buffer[6];
            strftime(time_str_buffer, sizeof(time_str_buffer), "%H:%M", &timeinfo);
            lv_snprintf(dsc->text, dsc->text_length, "%s", time_str_buffer);
        } else {
            lv_snprintf(dsc->text, dsc->text_length, "-:-");
        }
    }
}
