// temperature_system.h
#ifndef TEMPERATURE_SYSTEM_H
#define TEMPERATURE_SYSTEM_H

#include "lvgl.h"
#include <stddef.h>
#include <time.h>

// From your temperature_system.h (or config.h)
//#define CHART_Y_MIN_VALUE 0
//#define CHART_Y_MAX_VALUE 40 // Example for temperature range

// Y-values for Heater Status on the chart
#define HEATER_PLOT_Y_OFF  (CHART_Y_MIN_VALUE + 2)  // e.g., Plot Heater OFF at Y=2
#define HEATER_PLOT_Y_ON   (CHART_Y_MIN_VALUE + 8)  // e.g., Plot Heater ON  at Y=6 (a 4-unit jump)

// Y-values for Vent Stages on the chart
// Let's place these above the heater states to avoid overlap
#define VENT_PLOT_Y_S0     (CHART_Y_MIN_VALUE + 12)  // e.g., Plot Vent Closed (S0) at Y=8
#define VENT_PLOT_Y_S1     (CHART_Y_MIN_VALUE + 17) // e.g., Plot Vent S1      at Y=10 (2-unit jump per stage)
#define VENT_PLOT_Y_S2     (CHART_Y_MIN_VALUE + 22) // e.g., Plot Vent S2      at Y=12
#define VENT_PLOT_Y_S3     (CHART_Y_MIN_VALUE + 27) // e.g., Plot Vent S3      at Y=14

const uint32_t TEMP_SERIES_ID = 0;   // Assuming it's the first series added
const uint32_t VENT_SERIES_ID = 1;   // Assuming it's the second series added
const uint32_t HEATER_SERIES_ID = 2; // Assuming it's the third series added

typedef struct {
    float temperature;
    time_t timestamp;
    bool isValidTimestamp;
    int8_t ventStateNumeric;   // 0 for closed, 1 for S1, 2 for S2, 3 for S3
    int8_t heaterStateNumeric; // 0 for OFF, 1 for ON
} TempSampleData;

// Function declarations
void initializeTemperatureSystem();
void updateTemperatureSystem();
void updateCurrentTemperatureFromM4(float m4_temp);
// NEW: Functions to update current operational states for charting
void updateCurrentVentStageForChart(int m4_vent_stage);
void updateCurrentHeaterStateForChart(bool m4_heater_on);


#endif // TEMPERATURE_SYSTEM_H
