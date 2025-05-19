#include <RPC.h>

// --- Configuration Constants (same as before) ---
// ... (Relay Pins, Temp Setpoints, Time Settings, Durations) ...
const int VENT_OPEN_RELAY_PIN = 2;
const int VENT_CLOSE_RELAY_PIN = 3;
const int HEATER_RELAY_PIN = 4;
const int SHADE_OPEN_RELAY_PIN = 5;
const int SHADE_CLOSE_RELAY_PIN = 6;

const float VENT_OPEN_TEMP_STAGE1 = 25.0;
const float VENT_OPEN_TEMP_STAGE2 = 27.0;
const float VENT_OPEN_TEMP_STAGE3 = 29.0;

const float HEAT_SET_TEMP_DAY = 20.0;
const float HEAT_SET_TEMP_NIGHT = 18.0;
const float HEAT_BOOST_TEMP = 22.0;
const float HYSTERESIS = 1.0;

const int DAY_START_HOUR = 7;
const int NIGHT_START_HOUR = 19;
const int BOOST_START_HOUR = 6;
const int BOOST_DURATION_MINUTES = 60;
const int SHADE_OPEN_HOUR = 8;
const int SHADE_CLOSE_HOUR = 18;

const unsigned long VENT_PULSE_DURATION_MS = 5000;
const unsigned long SHADE_PULSE_DURATION_MS = 10000;
const unsigned long REFRESH_INTERVAL_MS = 60UL * 60UL * 1000UL;

// --- Global State Variables ---
float currentGreenhouseTemp = 15.0; // M4's own temperature
int currentHour = 0;                // Updated by M7
int currentMinute = 0;              // Updated by M7

// ... (currentVentStage, heaterState, shadeState, relay timers, refresh timers, boost state - same as before) ...
int currentVentStage = 0;
bool heaterState = false;
bool shadeState = false;

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

bool boostModeActive = false;


// --- RPC Exposed Functions (for M7 to call) ---
void receiveTimeFromM7_impl(int h, int m) {
  currentHour = h;
  currentMinute = m;
  // RPC.println("M4: Time updated from M7 to " + String(h) + ":" + String(m));
}

// M7 calls this to get M4's current temperature
float getM4Temperature_impl() {
    // In the future, this will read a physical sensor connected to M4.
    // For now, it returns the M4's simulated temperature.
    return currentGreenhouseTemp;
}
// ... (getM4VentStage_impl, getM4HeaterState_impl, getM4ShadeState_impl, getM4BoostState_impl - same as before) ...
int getM4VentStage_impl() {
    return currentVentStage;
}
bool getM4HeaterState_impl() {
    return heaterState;
}
bool getM4ShadeState_impl() {
    return shadeState;
}
bool getM4BoostState_impl() {
    return boostModeActive;
}


// --- Helper Function to Control Relays (same as before) ---
void setRelay(int pin, bool on) {
  digitalWrite(pin, on ? HIGH : LOW);
}

// --- M4 Temperature Simulation/Reading ---
void updateM4Temperature() {
    // **RE-INTRODUCE SIMULATION or add actual sensor reading here**
    // For now, simple simulation:
    static float simTemp = 18.0; // Start temp for simulation
    static int dir = 1;
    simTemp += 0.25 * dir; // Make it change a bit faster for testing
    if (simTemp > 32) dir = -1;
    if (simTemp < 12) dir = 1;
    currentGreenhouseTemp = simTemp;

    // If you had a sensor on M4:
    // currentGreenhouseTemp = readActualSensor();
}


// --- Setup ---
void setup() {
  RPC.begin();

  pinMode(VENT_OPEN_RELAY_PIN, OUTPUT);
  // ... (Initialize all pins and relays OFF as before) ...
  pinMode(VENT_CLOSE_RELAY_PIN, OUTPUT);
  pinMode(HEATER_RELAY_PIN, OUTPUT);
  pinMode(SHADE_OPEN_RELAY_PIN, OUTPUT);
  pinMode(SHADE_CLOSE_RELAY_PIN, OUTPUT);

  setRelay(VENT_OPEN_RELAY_PIN, false);
  setRelay(VENT_CLOSE_RELAY_PIN, false);
  setRelay(HEATER_RELAY_PIN, false);
  setRelay(SHADE_OPEN_RELAY_PIN, false);
  setRelay(SHADE_CLOSE_RELAY_PIN, false);

  // Bind RPC functions
  RPC.bind("receiveTimeFromM7", receiveTimeFromM7_impl);
  RPC.bind("getM4Temperature", getM4Temperature_impl); // M7 will call this
  RPC.bind("getM4VentStage", getM4VentStage_impl);
  RPC.bind("getM4HeaterState", getM4HeaterState_impl);
  RPC.bind("getM4ShadeState", getM4ShadeState_impl);
  RPC.bind("getM4BoostState", getM4BoostState_impl);

  RPC.println("M4: Greenhouse Controller Initialized (Self-Temp). RPC functions bound.");
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

  // 1. Update M4's own temperature (simulated or from sensor)
  updateM4Temperature();

  // Time is updated by M7 via RPC calls.

  // 2. Control Logic
  checkRelayPulses(currentTime);
  manageVents(currentTime);
  manageHeater(currentTime);
  manageShade(currentTime);
  handleRefreshPulses(currentTime);
  reportStatus(); // M4's own debugging prints

  delay(500); // M4 control loop update interval
}

// --- Control Logic Functions (checkRelayPulses, manageVents, manageHeater, manageShade, handleRefreshPulses, reportStatus) ---
// ... (These functions remain EXACTLY THE SAME as in your previous M4 sketch that worked) ...
// For brevity, I'm omitting them again, but PASTE THEM HERE.
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
    if (currentVentStage == 3 && currentGreenhouseTemp < VENT_OPEN_TEMP_STAGE3 - HYSTERESIS) {
      targetVentStage = 2;
    } else if (currentVentStage == 2 && currentGreenhouseTemp < VENT_OPEN_TEMP_STAGE2 - HYSTERESIS) {
      targetVentStage = 1;
    } else if (currentVentStage == 1 && currentGreenhouseTemp < VENT_OPEN_TEMP_STAGE1 - HYSTERESIS) {
      targetVentStage = 0;
    } else {
      targetVentStage = currentVentStage;
    }
  }

  if (targetVentStage > currentVentStage) {
    RPC.println("M4: Vents: Target OPEN to stage " + String(targetVentStage));
    setRelay(VENT_OPEN_RELAY_PIN, true);
    ventOpenRelayActive = true;
    ventOpenRelayStopTime = currentTime + VENT_PULSE_DURATION_MS * (targetVentStage - currentVentStage);
    currentVentStage = targetVentStage;
    lastVentRefreshTime = currentTime;
  } else if (targetVentStage < currentVentStage) {
    RPC.println("M4: Vents: Target CLOSE to stage " + String(targetVentStage));
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

  unsigned long currentMinutesSinceMidnight = currentHour * 60 + currentMinute;
  unsigned long boostStartMinutesSinceMidnight = BOOST_START_HOUR * 60;
  unsigned long boostEndMinutesSinceMidnight = boostStartMinutesSinceMidnight + BOOST_DURATION_MINUTES;

  if (currentMinutesSinceMidnight >= boostStartMinutesSinceMidnight &&
      currentMinutesSinceMidnight < boostEndMinutesSinceMidnight) {
    if (!boostModeActive) {
        boostModeActive = true;
    }
    actualHeatSetTemp = HEAT_BOOST_TEMP;
    mode = "Boost";
  } else {
    if (boostModeActive) {
        boostModeActive = false;
    }
    if (currentHour >= DAY_START_HOUR && currentHour < NIGHT_START_HOUR) {
      actualHeatSetTemp = HEAT_SET_TEMP_DAY;
      mode = "Day";
    } else {
      actualHeatSetTemp = HEAT_SET_TEMP_NIGHT;
      mode = "Night";
    }
  }

  bool newHeaterState = heaterState;
  if (heaterState) {
    if (currentGreenhouseTemp >= actualHeatSetTemp + HYSTERESIS) {
      newHeaterState = false;
    }
  } else {
    if (currentGreenhouseTemp <= actualHeatSetTemp) {
      newHeaterState = true;
    }
  }

  if (newHeaterState != heaterState) {
    heaterState = newHeaterState;
    setRelay(HEATER_RELAY_PIN, heaterState);
    RPC.println("M4: Heater " + String(heaterState ? "ON" : "OFF") +
                " (Mode: " + mode + ", SetT: " + String(actualHeatSetTemp,1) +
                ", CurrT: " + String(currentGreenhouseTemp,1) + ")");
  }
}

// --- Shade Management ---
void manageShade(unsigned long currentTime) {
  if (shadeOpenRelayActive || shadeCloseRelayActive) return;

  bool targetShadeState = shadeState;
  if (!shadeState && currentHour == SHADE_OPEN_HOUR && currentMinute == 0) {
      targetShadeState = true;
  } else if (shadeState && currentHour == SHADE_CLOSE_HOUR && currentMinute == 0) {
      targetShadeState = false;
  }

  if (targetShadeState != shadeState) {
    if (targetShadeState) {
        RPC.println("M4: Shade: Target OPEN.");
        setRelay(SHADE_OPEN_RELAY_PIN, true);
        shadeOpenRelayActive = true;
        shadeOpenRelayStopTime = currentTime + SHADE_PULSE_DURATION_MS;
    } else {
        RPC.println("M4: Shade: Target CLOSE.");
        setRelay(SHADE_CLOSE_RELAY_PIN, true);
        shadeCloseRelayActive = true;
        shadeCloseRelayStopTime = currentTime + SHADE_PULSE_DURATION_MS;
    }
    shadeState = targetShadeState;
    lastShadeRefreshTime = currentTime;
  }
}

// --- Handle Refresh Pulses ---
void handleRefreshPulses(unsigned long currentTime) {
  if (currentTime - lastVentRefreshTime >= REFRESH_INTERVAL_MS) {
    if (!ventOpenRelayActive && !ventCloseRelayActive) {
      if (currentVentStage == 3) {
        RPC.println("M4: Vents: Refreshing FULLY OPEN pulse.");
        setRelay(VENT_OPEN_RELAY_PIN, true);
        ventOpenRelayActive = true;
        ventOpenRelayStopTime = currentTime + (VENT_PULSE_DURATION_MS / 2);
      } else if (currentVentStage == 0) {
        RPC.println("M4: Vents: Refreshing FULLY CLOSED pulse.");
        setRelay(VENT_CLOSE_RELAY_PIN, true);
        ventCloseRelayActive = true;
        ventCloseRelayStopTime = currentTime + (VENT_PULSE_DURATION_MS / 2);
      }
    }
    lastVentRefreshTime = currentTime;
  }

  if (currentTime - lastShadeRefreshTime >= REFRESH_INTERVAL_MS) {
     if (!shadeOpenRelayActive && !shadeCloseRelayActive) {
        if (shadeState) {
            RPC.println("M4: Shade: Refreshing OPEN pulse.");
            setRelay(SHADE_OPEN_RELAY_PIN, true);
            shadeOpenRelayActive = true;
            shadeOpenRelayStopTime = currentTime + (SHADE_PULSE_DURATION_MS / 2);
        } else {
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
  unsigned long now = millis();
  if (now - lastReportTime >= 15000) { // Report every 15 seconds
    RPC.println("--- M4 Status ---");
    RPC.println("Time: " + String(currentHour) + ":" + String(currentMinute<10?"0":"") + String(currentMinute) +
                " Temp: " + String(currentGreenhouseTemp, 1) + "C"); // Display M4's temp
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
    lastReportTime = now;
  }
}
