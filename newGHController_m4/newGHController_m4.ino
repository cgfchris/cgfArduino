#include <RPC.h>

// --- Configuration Constants (Hardcoded for now) ---
// Relay Pins
const int VENT_OPEN_RELAY_PIN = 2; // Example pin
const int VENT_CLOSE_RELAY_PIN = 3;
const int HEATER_RELAY_PIN = 4;
const int SHADE_OPEN_RELAY_PIN = 5;
const int SHADE_CLOSE_RELAY_PIN = 6;

// Temperature Setpoints
const float VENT_OPEN_TEMP_STAGE1 = 25.0; // Temp to open to 25%
const float VENT_OPEN_TEMP_STAGE2 = 27.0; // Temp to open to 50% (example)
const float VENT_OPEN_TEMP_STAGE3 = 29.0; // Temp to open to 100%
// Closing will use these with hysteresis

const float HEAT_SET_TEMP_DAY = 20.0;
const float HEAT_SET_TEMP_NIGHT = 18.0;
const float HEAT_BOOST_TEMP = 22.0;

const float HYSTERESIS = 1.0; // e.g., if heat turns on at 18, it turns off at 18+1=19

// Time Settings (24-hour format)
const int DAY_START_HOUR = 7;
const int NIGHT_START_HOUR = 19;

const int BOOST_START_HOUR = 6; // e.g., 6 AM
const int BOOST_DURATION_MINUTES = 60;

const int SHADE_OPEN_HOUR = 8;
const int SHADE_CLOSE_HOUR = 18;

// Durations
const unsigned long VENT_PULSE_DURATION_MS = 5000; // 5 seconds to move one vent stage (calibrate this)
const unsigned long SHADE_PULSE_DURATION_MS = 10000; // 10 seconds to fully open/close shade (calibrate)
const unsigned long REFRESH_INTERVAL_MS = 60UL * 60UL * 1000UL; // 1 hour for refresh pulse

// --- Global State Variables ---
// Simulated current conditions
float currentGreenhouseTemp = 15.0;
int currentHour = 0;
int currentMinute = 0;

// Device States
// Vent: 0=Closed, 1=Stage1 (25%), 2=Stage2 (50%), 3=Stage3 (100%)
int currentVentStage = 0;
bool heaterState = false; // false=OFF, true=ON
bool shadeState = false;  // false=CLOSED, true=OPEN

// Relay Activation Timers (for pulsed operations)
bool ventOpenRelayActive = false;
unsigned long ventOpenRelayStopTime = 0;
bool ventCloseRelayActive = false;
unsigned long ventCloseRelayStopTime = 0;

bool shadeOpenRelayActive = false;
unsigned long shadeOpenRelayStopTime = 0;
bool shadeCloseRelayActive = false;
unsigned long shadeCloseRelayStopTime = 0;

// Refresh Timers
unsigned long lastVentRefreshTime = 0;
unsigned long lastShadeRefreshTime = 0;

// Boost State
bool boostModeActive = false;
unsigned long boostEndTime = 0;

// --- Helper Function to Control Relays ---
void setRelay(int pin, bool on) {
  digitalWrite(pin, on ? HIGH : LOW);
  // RPC.println("Relay Pin " + String(pin) + (on ? " ON" : " OFF")); // Very verbose
}

// --- Setup ---
void setup() {
  RPC.begin(); // M4 needs this for RPC.println as per Giga docs

  pinMode(VENT_OPEN_RELAY_PIN, OUTPUT);
  pinMode(VENT_CLOSE_RELAY_PIN, OUTPUT);
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(SHADE_OPEN_RELAY_PIN, OUTPUT);
  pinMode(SHADE_CLOSE_RELAY_PIN, OUTPUT);

  // Initialize relays to OFF
  setRelay(VENT_OPEN_RELAY_PIN, false);
  setRelay(VENT_CLOSE_RELAY_PIN, false);
  setRelay(HEATER_RELAY_PIN, false);
  setRelay(SHADE_OPEN_RELAY_PIN, false);
  setRelay(SHADE_CLOSE_RELAY_PIN, false);

  RPC.println("M4: Greenhouse Controller Initialized.");
  RPC.println("M4: Vent Setpoints: S1=" + String(VENT_OPEN_TEMP_STAGE1) +
              " S2=" + String(VENT_OPEN_TEMP_STAGE2) + " S3=" + String(VENT_OPEN_TEMP_STAGE3));
  RPC.println("M4: Heat Setpoints: Day=" + String(HEAT_SET_TEMP_DAY) +
              " Night=" + String(HEAT_SET_TEMP_NIGHT) + " Boost=" + String(HEAT_BOOST_TEMP));
  RPC.println("M4: Hysteresis: " + String(HYSTERESIS));

  lastVentRefreshTime = millis();
  lastShadeRefreshTime = millis();
}

// --- Main Loop ---
void loop() {
  unsigned long currentTime = millis();

  // 1. Simulate/Update Time and Temperature (Later from M7 or sensors)
  simulateTimeAndTemp(); // Call function to update currentHour, currentMinute, currentGreenhouseTemp

  // 2. Manage Relay Pulse Durations (Turn them off if time is up)
  checkRelayPulses(currentTime);

  // 3. Main Control Logic (only if no pulse is active for that system)
  manageVents(currentTime);
  manageHeater(currentTime);
  manageShade(currentTime);

  // 4. Periodic Refresh Pulses
  handleRefreshPulses(currentTime);

  // 5. Status Reporting
  reportStatus();

  delay(1000); // Main control loop update interval
}


// --- Simulation Function (for testing M4 standalone) ---
void simulateTimeAndTemp() {
  // Simulate time passing (1 minute per loop approx for faster testing)
  currentMinute++;
  if (currentMinute >= 60) {
    currentMinute = 0;
    currentHour++;
    if (currentHour >= 24) {
      currentHour = 0;
    }
  }

  // Simulate temperature changes (example: slow sine wave)
  // For now, let's just make it toggle for testing different states
  static int tempDirection = 1;
  currentGreenhouseTemp += 0.5 * tempDirection;
  if (currentGreenhouseTemp > 35.0) tempDirection = -1;
  if (currentGreenhouseTemp < 10.0) tempDirection = 1;

  //RPC.println("Sim: Time=" + String(currentHour) + ":" + String(currentMinute) + " Temp=" + String(currentGreenhouseTemp));
}

// --- Check and Stop Relay Pulses ---
void checkRelayPulses(unsigned long currentTime) {
  if (ventOpenRelayActive && currentTime >= ventOpenRelayStopTime) {
    setRelay(VENT_OPEN_RELAY_PIN, false);
    ventOpenRelayActive = false;
    RPC.println("M4: Vent Open Pulse FINISHED.");
  }
  if (ventCloseRelayActive && currentTime >= ventCloseRelayStopTime) {
    setRelay(VENT_CLOSE_RELAY_PIN, false);
    ventCloseRelayActive = false;
    RPC.println("M4: Vent Close Pulse FINISHED.");
  }
  if (shadeOpenRelayActive && currentTime >= shadeOpenRelayStopTime) {
    setRelay(SHADE_OPEN_RELAY_PIN, false);
    shadeOpenRelayActive = false;
    RPC.println("M4: Shade Open Pulse FINISHED.");
  }
  if (shadeCloseRelayActive && currentTime >= shadeCloseRelayStopTime) {
    setRelay(SHADE_CLOSE_RELAY_PIN, false);
    shadeCloseRelayActive = false;
    RPC.println("M4: Shade Close Pulse FINISHED.");
  }
}


// --- Vent Management ---
void manageVents(unsigned long currentTime) {
  if (ventOpenRelayActive || ventCloseRelayActive) return; // Don't interfere with active pulse

  int targetVentStage = 0;
  if (currentGreenhouseTemp >= VENT_OPEN_TEMP_STAGE3) {
    targetVentStage = 3;
  } else if (currentGreenhouseTemp >= VENT_OPEN_TEMP_STAGE2) {
    targetVentStage = 2;
  } else if (currentGreenhouseTemp >= VENT_OPEN_TEMP_STAGE1) {
    targetVentStage = 1;
  } else {
    // Check for closing with hysteresis
    if (currentVentStage == 3 && currentGreenhouseTemp < VENT_OPEN_TEMP_STAGE3 - HYSTERESIS) {
      targetVentStage = 2;
    } else if (currentVentStage == 2 && currentGreenhouseTemp < VENT_OPEN_TEMP_STAGE2 - HYSTERESIS) {
      targetVentStage = 1;
    } else if (currentVentStage == 1 && currentGreenhouseTemp < VENT_OPEN_TEMP_STAGE1 - HYSTERESIS) {
      targetVentStage = 0;
    } else {
      targetVentStage = currentVentStage; // No change based on closing hysteresis
    }
  }

  if (targetVentStage > currentVentStage) {
    RPC.println("M4: Vents: Need to OPEN from stage " + String(currentVentStage) + " to " + String(targetVentStage));
    setRelay(VENT_OPEN_RELAY_PIN, true);
    ventOpenRelayActive = true;
    ventOpenRelayStopTime = currentTime + VENT_PULSE_DURATION_MS * (targetVentStage - currentVentStage); // Pulse for number of stages
    currentVentStage = targetVentStage; // Assume it will reach the target after pulse
    lastVentRefreshTime = currentTime; // Reset refresh timer as we just moved
  } else if (targetVentStage < currentVentStage) {
    RPC.println("M4: Vents: Need to CLOSE from stage " + String(currentVentStage) + " to " + String(targetVentStage));
    setRelay(VENT_CLOSE_RELAY_PIN, true);
    ventCloseRelayActive = true;
    ventCloseRelayStopTime = currentTime + VENT_PULSE_DURATION_MS * (currentVentStage - targetVentStage);
    currentVentStage = targetVentStage;
    lastVentRefreshTime = currentTime;
  }
}

// --- Heater Management ---
void manageHeater(unsigned long currentTime) {
  float actualHeatSetTemp;
  String mode = "Normal";

  // Check for Boost Mode
  unsigned long currentMinutesSinceMidnight = currentHour * 60 + currentMinute;
  unsigned long boostStartMinutesSinceMidnight = BOOST_START_HOUR * 60;
  unsigned long boostEndMinutesSinceMidnight = boostStartMinutesSinceMidnight + BOOST_DURATION_MINUTES;

  if (currentMinutesSinceMidnight >= boostStartMinutesSinceMidnight &&
      currentMinutesSinceMidnight < boostEndMinutesSinceMidnight) {
    if (!boostModeActive) {
        RPC.println("M4: Heater Entering BOOST Mode.");
        boostModeActive = true;
    }
    actualHeatSetTemp = HEAT_BOOST_TEMP;
    mode = "Boost";
  } else {
    if (boostModeActive) {
        RPC.println("M4: Heater Exiting BOOST Mode.");
        boostModeActive = false;
    }
    // Day/Night Mode
    if (currentHour >= DAY_START_HOUR && currentHour < NIGHT_START_HOUR) {
      actualHeatSetTemp = HEAT_SET_TEMP_DAY;
      mode = "Day";
    } else {
      actualHeatSetTemp = HEAT_SET_TEMP_NIGHT;
      mode = "Night";
    }
  }

  bool newHeaterState = heaterState;
  if (heaterState) { // If heater is ON
    if (currentGreenhouseTemp >= actualHeatSetTemp + HYSTERESIS) {
      newHeaterState = false; // Turn OFF
    }
  } else { // If heater is OFF
    if (currentGreenhouseTemp <= actualHeatSetTemp) {
      newHeaterState = true; // Turn ON
    }
  }

  if (newHeaterState != heaterState) {
    heaterState = newHeaterState;
    setRelay(HEATER_RELAY_PIN, heaterState);
    RPC.println("M4: Heater " + String(heaterState ? "ON" : "OFF") +
                " (Mode: " + mode + ", SetT: " + String(actualHeatSetTemp) +
                ", CurrT: " + String(currentGreenhouseTemp) + ")");
  }
}

// --- Shade Management ---
void manageShade(unsigned long currentTime) {
  if (shadeOpenRelayActive || shadeCloseRelayActive) return; // Don't interfere with active pulse

  bool targetShadeState = shadeState; // Assume no change initially

  // Check for opening time
  if (!shadeState && currentHour == SHADE_OPEN_HOUR && currentMinute == 0) { // Target: OPEN
      targetShadeState = true;
  }
  // Check for closing time
  else if (shadeState && currentHour == SHADE_CLOSE_HOUR && currentMinute == 0) { // Target: CLOSE
      targetShadeState = false;
  }

  if (targetShadeState != shadeState) {
    if (targetShadeState) { // Need to OPEN
        RPC.println("M4: Shade: Need to OPEN.");
        setRelay(SHADE_OPEN_RELAY_PIN, true);
        shadeOpenRelayActive = true;
        shadeOpenRelayStopTime = currentTime + SHADE_PULSE_DURATION_MS;
    } else { // Need to CLOSE
        RPC.println("M4: Shade: Need to CLOSE.");
        setRelay(SHADE_CLOSE_RELAY_PIN, true);
        shadeCloseRelayActive = true;
        shadeCloseRelayStopTime = currentTime + SHADE_PULSE_DURATION_MS;
    }
    shadeState = targetShadeState; // Assume it reaches target
    lastShadeRefreshTime = currentTime; // Reset refresh timer
  }
}

// --- Handle Refresh Pulses ---
void handleRefreshPulses(unsigned long currentTime) {
  // Vent Refresh
  if (currentTime - lastVentRefreshTime >= REFRESH_INTERVAL_MS) {
    if (!ventOpenRelayActive && !ventCloseRelayActive) { // Only if no other vent operation is active
      if (currentVentStage == 3) { // Fully Open
        RPC.println("M4: Vents: Refreshing FULLY OPEN pulse.");
        setRelay(VENT_OPEN_RELAY_PIN, true);
        ventOpenRelayActive = true;
        ventOpenRelayStopTime = currentTime + (VENT_PULSE_DURATION_MS / 2); // Shorter pulse for refresh
      } else if (currentVentStage == 0) { // Fully Closed
        RPC.println("M4: Vents: Refreshing FULLY CLOSED pulse.");
        setRelay(VENT_CLOSE_RELAY_PIN, true);
        ventCloseRelayActive = true;
        ventCloseRelayStopTime = currentTime + (VENT_PULSE_DURATION_MS / 2);
      }
    }
    lastVentRefreshTime = currentTime; // Reset timer regardless of action
  }

  // Shade Refresh
  if (currentTime - lastShadeRefreshTime >= REFRESH_INTERVAL_MS) {
     if (!shadeOpenRelayActive && !shadeCloseRelayActive) {
        if (shadeState) { // Shade is Open
            RPC.println("M4: Shade: Refreshing OPEN pulse.");
            setRelay(SHADE_OPEN_RELAY_PIN, true);
            shadeOpenRelayActive = true;
            shadeOpenRelayStopTime = currentTime + (SHADE_PULSE_DURATION_MS / 2);
        } else { // Shade is Closed
            RPC.println("M4: Shade: Refreshing CLOSED pulse.");
            setRelay(SHADE_CLOSE_RELAY_PIN, true);
            shadeCloseRelayActive = true;
            shadeCloseRelayStopTime = currentTime + (SHADE_PULSE_DURATION_MS / 2);
        }
     }
     lastShadeRefreshTime = currentTime;
  }
}


// --- Status Reporting (via RPC.println) ---
void reportStatus() {
  static unsigned long lastReportTime = 0;
  if (millis() - lastReportTime >= 10000) { // Report every 10 seconds
    RPC.println("--- M4 Status ---");
    RPC.println("Time: " + String(currentHour) + ":" + String(currentMinute<10?"0":"") + String(currentMinute) +
                " Temp: " + String(currentGreenhouseTemp, 1) + "C");
    String ventStatusStr = "UNKNOWN";
    if (currentVentStage == 0) ventStatusStr = "CLOSED (0%)";
    else if (currentVentStage == 1) ventStatusStr = "STAGE1 (25%)";
    else if (currentVentStage == 2) ventStatusStr = "STAGE2 (50%)";
    else if (currentVentStage == 3) ventStatusStr = "STAGE3 (100%)";
    RPC.println("Vents: " + ventStatusStr +
                (ventOpenRelayActive ? " (Opening)" : "") +
                (ventCloseRelayActive ? " (Closing)" : ""));
    RPC.println("Heater: " + String(heaterState ? "ON" : "OFF"));
    RPC.println("Shade: " + String(shadeState ? "OPEN" : "CLOSED") +
                (shadeOpenRelayActive ? " (Opening)" : "") +
                (shadeCloseRelayActive ? " (Closing)" : ""));
    RPC.println("Boost Active: " + String(boostModeActive ? "YES" : "NO"));
    RPC.println("------------------");
    lastReportTime = millis();
  }
}
