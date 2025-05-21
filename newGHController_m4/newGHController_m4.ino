// GreenhouseM4_FlashIAP.ino
#include <RPC.h>
#include <Arduino.h>
#include "settings_storage.h" // Our settings module using FlashIAPBlockDevice
#include "config.h"

// --- Global Instance of Settings (used by settings_storage.cpp via extern) ---
GreenhouseSettings currentSettings;
bool settingsDirty = false;

// --- Global State Variables (Operational) ---
// ... (Same as your previous M4 sketch: currentGreenhouseTemp_M4, currentHour_M4, etc.) ...
float currentGreenhouseTemp_M4 = 15.0; 
int currentHour_M4 = 0;               
int currentMinute_M4 = 0;             
int currentVentStage_M4 = 0;   
bool heaterState_M4 = false;   
bool shadeState_M4 = false;    
bool boostModeActive_M4 = false;
bool ventOpenRelayActive = false;
unsigned long ventOpenRelayStopTime = 0;
bool ventCloseRelayActive = false;
unsigned long ventCloseRelayStopTime = 0;
bool shadeOpenRelayActive = false;
unsigned long shadeOpenRelayStopTime = 0;
bool shadeCloseRelayActive = false;
unsigned long shadeCloseRelayStopTime = 0;
unsigned long lastVentRefreshTime = 0;
unsigned long lastShadeRefreshTime = 0;


// --- RPC Exposed Functions & Implementations ---
// ... (ALL your RPC functions: receiveTimeFromM7_impl, getM4Temperature_impl,
//      setVentTempS1_impl, etc. remain THE SAME as in the LittleFS M4 version) ...
// --- RPC Exposed Functions (Getters - M7 calls these) ---
void receiveTimeFromM7_impl(int h, int m) {
    currentHour_M4 = h;
    currentMinute_M4 = m;
}
float getM4Temperature_impl() { return currentGreenhouseTemp_M4; }
int getM4VentStage_impl() { return currentVentStage_M4; }
bool getM4HeaterState_impl() { return heaterState_M4; }
bool getM4ShadeState_impl() { return shadeState_M4; }
bool getM4BoostState_impl() { return boostModeActive_M4; }

// --- NEW RPC Getter functions for relay pulse activity ---
bool getM4VentOpeningActive_impl() {return ventOpenRelayActive;}
bool getM4VentClosingActive_impl() {return ventCloseRelayActive;}
// getM4HeaterState_impl() already gives heater relay status
bool getM4ShadeOpeningActive_impl() {return shadeOpenRelayActive;}
bool getM4ShadeClosingActive_impl() {return shadeCloseRelayActive;}

// --- RPC Implementations for M7 to update settings in M4's RAM ---
void setVentTempS1_impl(float temp) {
    if (abs(currentSettings.ventOpenTempStage1 - temp) > 0.01f) { 
        currentSettings.ventOpenTempStage1 = temp;
        mark_settings_dirty();
        RPC.println("M4: Vent S1 Temp set to " + String(temp, 1) + "C by M7.");
    }
}
void setVentTempS2_impl(float temp) {
    if (abs(currentSettings.ventOpenTempStage2 - temp) > 0.01f){
        currentSettings.ventOpenTempStage2 = temp; 
        mark_settings_dirty(); 
        RPC.println("M4: Vent S2 Temp set to " + String(temp,1) + "C by M7.");
    }
}
void setVentTempS3_impl(float temp) {
    if (abs(currentSettings.ventOpenTempStage3 - temp) > 0.01f){
        currentSettings.ventOpenTempStage3 = temp; 
        mark_settings_dirty(); 
        RPC.println("M4: Vent S3 Temp set to " + String(temp,1) + "C by M7.");
    }
}
void setHeatTempDay_impl(float temp) {
    if (abs(currentSettings.heatSetTempDay - temp) > 0.01f){ 
        currentSettings.heatSetTempDay = temp; 
        mark_settings_dirty(); 
        RPC.println("M4: Heat Day Temp set to " + String(temp,1) + "C by M7.");
    }
}
void setHeatTempNight_impl(float temp) {
    if (abs(currentSettings.heatSetTempNight - temp) > 0.01f){ 
        currentSettings.heatSetTempNight = temp; 
        mark_settings_dirty(); 
        RPC.println("M4: Heat Night Temp set to " + String(temp,1) + "C by M7.");
    }
}
void setHeatBoostTemp_impl(float temp) {
    if (abs(currentSettings.heatBoostTemp - temp) > 0.01f){ 
        currentSettings.heatBoostTemp = temp; 
        mark_settings_dirty(); 
        RPC.println("M4: Heat Boost Temp set to " + String(temp,1) + "C by M7.");
    }
}
void setHysteresis_impl(float temp) {
    if (abs(currentSettings.hysteresis - temp) > 0.01f){ 
        currentSettings.hysteresis = temp; 
        mark_settings_dirty(); 
        RPC.println("M4: Hysteresis set to " + String(temp,1) + "C by M7.");
    }
}
void setDayStartTime_impl(uint8_t hour, uint8_t minute) {
    if (currentSettings.dayStartHour != hour || currentSettings.dayStartMinute != minute) {
        currentSettings.dayStartHour = hour; currentSettings.dayStartMinute = minute;
        mark_settings_dirty();
        RPC.println("M4: Day Start Time set to " + String(hour) + ":" + String(minute<10?"0":"") + String(minute) + " by M7.");
    }
}
void setNightStartTime_impl(uint8_t hour, uint8_t minute) {
    if (currentSettings.nightStartHour != hour || currentSettings.nightStartMinute != minute) {
        currentSettings.nightStartHour = hour; currentSettings.nightStartMinute = minute;
        mark_settings_dirty();
        RPC.println("M4: Night Start Time set to " + String(hour) + ":" + String(minute<10?"0":"") + String(minute) + " by M7.");
    }
}
void setBoostStartTime_impl(uint8_t hour, uint8_t minute) {
    if (currentSettings.boostStartHour != hour || currentSettings.boostStartMinute != minute) {
        currentSettings.boostStartHour = hour; currentSettings.boostStartMinute = minute;
        mark_settings_dirty();
        RPC.println("M4: Boost Start Time set to " + String(hour) + ":" + String(minute<10?"0":"") + String(minute) + " by M7.");
    }
}
void setBoostDuration_impl(uint16_t duration) {
    if (currentSettings.boostDurationMinutes != duration) {
        currentSettings.boostDurationMinutes = duration;
        mark_settings_dirty();
        RPC.println("M4: Boost Duration set to " + String(duration) + " mins by M7.");
    }
}
void setShadeOpenTime_impl(uint8_t hour, uint8_t minute) {
    if (currentSettings.shadeOpenHour != hour || currentSettings.shadeOpenMinute != minute) {
        currentSettings.shadeOpenHour = hour; currentSettings.shadeOpenMinute = minute;
        mark_settings_dirty();
        RPC.println("M4: Shade Open Time set to " + String(hour) + ":" + String(minute<10?"0":"") + String(minute) + " by M7.");
    }
}
void setShadeCloseTime_impl(uint8_t hour, uint8_t minute) {
    if (currentSettings.shadeCloseHour != hour || currentSettings.shadeCloseMinute != minute) {
        currentSettings.shadeCloseHour = hour; currentSettings.shadeCloseMinute = minute;
        mark_settings_dirty();
        RPC.println("M4: Shade Close Time set to " + String(hour) + ":" + String(minute<10?"0":"") + String(minute) + " by M7.");
    }
}

// NEW RPC function to get the entire settings struct
GreenhouseSettings getM4CurrentSettings_impl() {
    RPC.println("M4: M7 requested entire currentSettings struct.");
    return currentSettings; // Return the global struct by value
}

// --- Helper Function to Control Relays ---
void setRelay(int pin, bool on) {
    digitalWrite(pin, on ? HIGH : LOW);
}

// --- M4 Temperature Simulation/Reading ---
void updateM4OwnTemperature() {
    static float simTempM4 = 18.0; 
    static int simTempDirM4 = 1;
    simTempM4 += 0.20 * simTempDirM4; 
    if (simTempM4 > 34.0) simTempDirM4 = -1;
    if (simTempM4 < 11.0) simTempDirM4 = 1;
    currentGreenhouseTemp_M4 = simTempM4;

/*
 * new code:
    float tempM4;
    tempM4 = analogRead(SENSOR4_PIN);
    currentGreenhouseTemp_M4 = float(S4_CAL_T2)+float(S4_SLOPE)*float(s4_in-S4_CAL_V2);
 */

    /* code from temperatureController v1.8:
     *  // read from sensor 4 (aspirator)
    s4_in=analogRead(SENSOR4_PIN);
    SERIALPRINTLN(s4_in);
    //SERIALPRINTLN(S4_SLOPE);
    //SERIALPRINTLN(s4_in-S4_CAL_V2);
    sens[3].val=float(S4_CAL_T2)+float(S4_SLOPE)*float(s4_in-S4_CAL_V2);
    */
}


void setup() {
    RPC.begin();
    initialize_settings_flashiap(); // Use the new initialization function

    // ... (pinModes and setRelay calls for initialization - same as before) ...
    pinMode(VENT_OPEN_RELAY_PIN, OUTPUT);
    pinMode(VENT_CLOSE_RELAY_PIN, OUTPUT);
    pinMode(HEATER_RELAY_PIN1, OUTPUT);
    pinMode(HEATER_RELAY_PIN2, OUTPUT);
    pinMode(SHADE_OPEN_RELAY_PIN, OUTPUT);
    pinMode(SHADE_CLOSE_RELAY_PIN, OUTPUT);

    setRelay(VENT_OPEN_RELAY_PIN, false);
    setRelay(VENT_CLOSE_RELAY_PIN, false);
    setRelay(HEATER_RELAY_PIN1, false);
    setRelay(HEATER_RELAY_PIN2, false);
    setRelay(SHADE_OPEN_RELAY_PIN, false);
    setRelay(SHADE_CLOSE_RELAY_PIN, false);

    // RPC Bindings (same as before)
    RPC.bind("receiveTimeFromM7", receiveTimeFromM7_impl);
    RPC.bind("getM4Temperature", getM4Temperature_impl);
    RPC.bind("getM4VentStage", getM4VentStage_impl);
    RPC.bind("getM4HeaterState", getM4HeaterState_impl);
    RPC.bind("getM4ShadeState", getM4ShadeState_impl);
    RPC.bind("getM4BoostState", getM4BoostState_impl);

    // Bind NEW relay activity getters
    RPC.bind("getVentOpeningActive", getM4VentOpeningActive_impl);
    RPC.bind("getVentClosingActive", getM4VentClosingActive_impl);
    RPC.bind("getShadeOpeningActive", getM4ShadeOpeningActive_impl);
    RPC.bind("getShadeClosingActive", getM4ShadeClosingActive_impl);

    RPC.bind("setVentTempS1", setVentTempS1_impl);
    RPC.bind("setVentTempS2", setVentTempS2_impl);
    RPC.bind("setVentTempS3", setVentTempS3_impl);
    RPC.bind("setHeatTempDay", setHeatTempDay_impl);
    RPC.bind("setHeatTempNight", setHeatTempNight_impl);
    RPC.bind("setHeatBoostTemp", setHeatBoostTemp_impl);
    RPC.bind("setHysteresis", setHysteresis_impl);
    RPC.bind("setDayStartTime", setDayStartTime_impl);
    RPC.bind("setNightStartTime", setNightStartTime_impl);
    RPC.bind("setBoostStartTime", setBoostStartTime_impl);
    RPC.bind("setBoostDuration", setBoostDuration_impl);
    RPC.bind("setShadeOpenTime", setShadeOpenTime_impl);
    RPC.bind("setShadeCloseTime", setShadeCloseTime_impl);
    // NEW RPC Binding for getting all settings
    RPC.bind("getM4AllSettings", getM4CurrentSettings_impl);
    
    RPC.println("M4: Greenhouse Controller Initialized (FlashIAP Storage).");
    RPC.println("M4: Loaded Settings Preview: VentS1=" + String(currentSettings.ventOpenTempStage1,1) +
                "C, Hys=" + String(currentSettings.hysteresis,1) + "C");

    lastVentRefreshTime = millis();
    lastShadeRefreshTime = millis();
}

void loop() {
    unsigned long currentTimeMs = millis();
    updateM4OwnTemperature();

    checkRelayPulses(currentTimeMs);
    manageVents(currentTimeMs);
    manageHeater(currentTimeMs);
    manageShade(currentTimeMs);
    handleRefreshPulses(currentTimeMs);
    
    save_settings_if_dirty(); // Checks flag and saves to FlashIAP if needed

    reportStatus();
    delay(500);
}

// --- Control Logic Functions (checkRelayPulses, manageVents, manageHeater, manageShade, handleRefreshPulses, reportStatus) ---
// PASTE YOUR FULLY UPDATED VERSIONS HERE.
// Ensure they use currentSettings.fieldName and correct hour/minute logic.
// (These are the same implementations as the "LittleFS" M4 version provided previously)
void checkRelayPulses(unsigned long currentTime) {
  if (ventOpenRelayActive && currentTime >= ventOpenRelayStopTime) {
    setRelay(VENT_OPEN_RELAY_PIN, false); ventOpenRelayActive = false;
    RPC.println("M4: Vent Open Pulse FINISHED.");
  }
  if (ventCloseRelayActive && currentTime >= ventCloseRelayStopTime) {
    setRelay(VENT_CLOSE_RELAY_PIN, false); ventCloseRelayActive = false;
    RPC.println("M4: Vent Close Pulse FINISHED.");
  }
  if (shadeOpenRelayActive && currentTime >= shadeOpenRelayStopTime) {
    setRelay(SHADE_OPEN_RELAY_PIN, false); shadeOpenRelayActive = false;
    RPC.println("M4: Shade Open Pulse FINISHED.");
  }
  if (shadeCloseRelayActive && currentTime >= shadeCloseRelayStopTime) {
    setRelay(SHADE_CLOSE_RELAY_PIN, false); shadeCloseRelayActive = false;
    RPC.println("M4: Shade Close Pulse FINISHED.");
  }
}

void manageVents(unsigned long currentTime) {
  if (ventOpenRelayActive || ventCloseRelayActive) return;

  int targetVentStage = currentVentStage_M4; 
  if (currentGreenhouseTemp_M4 >= currentSettings.ventOpenTempStage3) {
    targetVentStage = 3;
  } else if (currentGreenhouseTemp_M4 >= currentSettings.ventOpenTempStage2) {
    targetVentStage = 2;
  } else if (currentGreenhouseTemp_M4 >= currentSettings.ventOpenTempStage1) {
    targetVentStage = 1;
  } else { 
    if (currentVentStage_M4 == 3 && currentGreenhouseTemp_M4 < currentSettings.ventOpenTempStage3 - currentSettings.hysteresis) {
      targetVentStage = 2;
    } else if (currentVentStage_M4 == 2 && currentGreenhouseTemp_M4 < currentSettings.ventOpenTempStage2 - currentSettings.hysteresis) {
      targetVentStage = 1;
    } else if (currentVentStage_M4 == 1 && currentGreenhouseTemp_M4 < currentSettings.ventOpenTempStage1 - currentSettings.hysteresis) {
      targetVentStage = 0;
    }
  }

  if (targetVentStage > currentVentStage_M4) {
    RPC.println("M4: Vents: Target OPEN from " + String(currentVentStage_M4) + " to " + String(targetVentStage));
    setRelay(VENT_OPEN_RELAY_PIN, true); ventOpenRelayActive = true;
    ventOpenRelayStopTime = currentTime + VENT_CHANGE_DURATION_MS * (targetVentStage - currentVentStage_M4);
    currentVentStage_M4 = targetVentStage; lastVentRefreshTime = currentTime;
  } else if (targetVentStage < currentVentStage_M4) {
    RPC.println("M4: Vents: Target CLOSE from " + String(currentVentStage_M4) + " to " + String(targetVentStage));
    setRelay(VENT_CLOSE_RELAY_PIN, true); ventCloseRelayActive = true;
    ventCloseRelayStopTime = currentTime + VENT_CHANGE_DURATION_MS * (currentVentStage_M4 - targetVentStage);
    currentVentStage_M4 = targetVentStage; lastVentRefreshTime = currentTime;
  }
}

void manageHeater(unsigned long currentTime) {
    float actualHeatSetTemp; String modeDescr = "Normal"; 
    uint16_t now_minutes = currentHour_M4 * 60 + currentMinute_M4;
    uint16_t boostStart_minutes = currentSettings.boostStartHour * 60 + currentSettings.boostStartMinute;
    uint16_t boostEnd_minutes = boostStart_minutes + currentSettings.boostDurationMinutes;

    bool isBoostTime = false;
    if (currentSettings.boostDurationMinutes > 0) { 
        if (boostStart_minutes <= boostEnd_minutes) { 
            isBoostTime = (now_minutes >= boostStart_minutes && now_minutes < boostEnd_minutes);
        } else { 
            isBoostTime = (now_minutes >= boostStart_minutes || now_minutes < boostEnd_minutes);
        }
    }
    
    if (isBoostTime) {
        if (!boostModeActive_M4) { RPC.println("M4: Heater Entering BOOST Mode."); }
        boostModeActive_M4 = true; actualHeatSetTemp = currentSettings.heatBoostTemp; modeDescr = "Boost";
    } else {
        if (boostModeActive_M4) { RPC.println("M4: Heater Exiting BOOST Mode."); }
        boostModeActive_M4 = false;
        uint16_t dayStart_minutes = currentSettings.dayStartHour * 60 + currentSettings.dayStartMinute;
        uint16_t nightStart_minutes = currentSettings.nightStartHour * 60 + currentSettings.nightStartMinute;
        bool isDayPeriod;
        if (dayStart_minutes < nightStart_minutes) { 
            isDayPeriod = (now_minutes >= dayStart_minutes && now_minutes < nightStart_minutes);
        } else if (dayStart_minutes > nightStart_minutes) { 
            isDayPeriod = (now_minutes >= dayStart_minutes || now_minutes < nightStart_minutes);
        } else { 
            isDayPeriod = true; 
        }
        if (isDayPeriod) { actualHeatSetTemp = currentSettings.heatSetTempDay; modeDescr = "Day"; }
        else { actualHeatSetTemp = currentSettings.heatSetTempNight; modeDescr = "Night"; }
    }

    bool newHeaterState = heaterState_M4;
    if (heaterState_M4) { 
        if (currentGreenhouseTemp_M4 >= actualHeatSetTemp + currentSettings.hysteresis) {
            newHeaterState = false; 
        }
    } else { 
        if (currentGreenhouseTemp_M4 <= actualHeatSetTemp) {
            newHeaterState = true; 
        }
    }
    if (newHeaterState != heaterState_M4) {
        heaterState_M4 = newHeaterState; setRelay(HEATER_RELAY_PIN1, heaterState_M4);setRelay(HEATER_RELAY_PIN2, heaterState_M4);
        RPC.println("M4: Heater " + String(heaterState_M4 ? "ON" : "OFF") +
                    " (Mode: " + modeDescr + ", SetT: " + String(actualHeatSetTemp,1) +
                    "C, CurrT: " + String(currentGreenhouseTemp_M4,1) + "C)");
    }
}

void manageShade(unsigned long currentTime) {
    if (shadeOpenRelayActive || shadeCloseRelayActive) return;
    bool desiredShadeStateBasedOnTime;
    uint16_t now_minutes = currentHour_M4 * 60 + currentMinute_M4;
    uint16_t shadeOpen_minutes = currentSettings.shadeOpenHour * 60 + currentSettings.shadeOpenMinute;
    uint16_t shadeClose_minutes = currentSettings.shadeCloseHour * 60 + currentSettings.shadeCloseMinute;

    if (shadeOpen_minutes < shadeClose_minutes) { 
        desiredShadeStateBasedOnTime = (now_minutes >= shadeOpen_minutes && now_minutes < shadeClose_minutes);
    } else if (shadeOpen_minutes > shadeClose_minutes) { 
        desiredShadeStateBasedOnTime = (now_minutes >= shadeOpen_minutes || now_minutes < shadeClose_minutes);
    } else { 
        desiredShadeStateBasedOnTime = false; 
    }

    if (desiredShadeStateBasedOnTime != shadeState_M4) {
        String actionVerb = desiredShadeStateBasedOnTime ? "OPEN" : "CLOSE";
        RPC.println("M4: Shade: Target " + actionVerb + " based on time. (" + String(currentHour_M4) + ":" + String(currentMinute_M4 < 10 ? "0" : "") + String(currentMinute_M4) +")");
        setRelay(desiredShadeStateBasedOnTime ? SHADE_OPEN_RELAY_PIN : SHADE_CLOSE_RELAY_PIN, true);
        if (desiredShadeStateBasedOnTime) { 
            shadeOpenRelayActive = true; shadeOpenRelayStopTime = currentTime + SHADE_PULSE_DURATION_MS; 
        } else { 
            shadeCloseRelayActive = true; shadeCloseRelayStopTime = currentTime + SHADE_PULSE_DURATION_MS; 
        }
        shadeState_M4 = desiredShadeStateBasedOnTime; 
        lastShadeRefreshTime = currentTime;
    }
}

void handleRefreshPulses(unsigned long currentTime) {
  if (currentTime - lastVentRefreshTime >= REFRESH_INTERVAL_MS) {
    if (!ventOpenRelayActive && !ventCloseRelayActive) {
      if (currentVentStage_M4 == 3) { 
        RPC.println("M4: Vents: Refreshing FULLY OPEN pulse.");
        setRelay(VENT_OPEN_RELAY_PIN, true); ventOpenRelayActive = true;
        ventOpenRelayStopTime = currentTime + (VENT_PULSE_DURATION_MS); 
      } else if (currentVentStage_M4 == 0) { 
        RPC.println("M4: Vents: Refreshing FULLY CLOSED pulse.");
        setRelay(VENT_CLOSE_RELAY_PIN, true); ventCloseRelayActive = true;
        ventCloseRelayStopTime = currentTime + (VENT_PULSE_DURATION_MS);
      }
    }
    lastVentRefreshTime = currentTime;
  }
  if (currentTime - lastShadeRefreshTime >= REFRESH_INTERVAL_MS) {
     if (!shadeOpenRelayActive && !shadeCloseRelayActive) {
        if (shadeState_M4) { 
            RPC.println("M4: Shade: Refreshing OPEN pulse.");
            setRelay(SHADE_OPEN_RELAY_PIN, true); shadeOpenRelayActive = true;
            shadeOpenRelayStopTime = currentTime + (SHADE_PULSE_DURATION_MS / 2);
        } else { 
            RPC.println("M4: Shade: Refreshing CLOSED pulse.");
            setRelay(SHADE_CLOSE_RELAY_PIN, true); shadeCloseRelayActive = true;
            shadeCloseRelayStopTime = currentTime + (SHADE_PULSE_DURATION_MS / 2);
        }
     }
     lastShadeRefreshTime = currentTime;
  }
}

void reportStatus() {
  static unsigned long lastReportTime = 0;
  unsigned long now = millis();
  if (now - lastReportTime >= 20000) { 
    RPC.println("--- M4 Status Report ---");
    RPC.println("Time: " + String(currentHour_M4) + ":" + String(currentMinute_M4<10?"0":"") + String(currentMinute_M4) +
                " | Temp: " + String(currentGreenhouseTemp_M4, 1) + "C");
    String ventStatusStr = "Vents: ?";
    if (currentVentStage_M4 == 0) ventStatusStr = "Vents: CLOSED";
    else if (currentVentStage_M4 == 1) ventStatusStr = "Vents: S1(25%)";
    else if (currentVentStage_M4 == 2) ventStatusStr = "Vents: S2(50%)";
    else if (currentVentStage_M4 == 3) ventStatusStr = "Vents: S3(100%)";
    RPC.println(ventStatusStr +
                (ventOpenRelayActive ? " (Opening)" : "") +
                (ventCloseRelayActive ? " (Closing)" : ""));
    RPC.println("Heater: " + String(heaterState_M4 ? "ON" : "OFF") + " | Shade: " + String(shadeState_M4 ? "OPEN" : "CLOSED") +
                (shadeOpenRelayActive ? " (Opening)" : "") +
                (shadeCloseRelayActive ? " (Closing)" : ""));
    RPC.println("Boost Mode: " + String(boostModeActive_M4 ? "ACTIVE" : "OFF"));
    RPC.println("Settings Dirty Flag: " + String(settingsDirty ? "YES (awaiting save)" : "NO"));
    RPC.println("--------------------------");
    lastReportTime = now;
  }
}
