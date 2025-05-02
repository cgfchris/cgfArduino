/* LoadAndSaveSettings
 * Joghurt 2010
 * Demonstrates how to load and save settings to the EEPROM
 */
// Contains EEPROM.read() and EEPROM.write()
#include <EEPROM.h>

// ID of the settings block
#define CONFIG_VERSION "cgt6"

// Tell it where to store your config data in EEPROM
#define CONFIG_START 32

// Example settings structure
struct StoreStruct {
  // This is for mere detection if they are your settings
  char version[5];
  // The variables of your settings
  float settings[n_settings];
  int relayState;
} storage={CONFIG_VERSION,{2.0, 2.0, 18.0, 12.0, 22.0, 25.0, 28.0, 0.0, 0.0,1.0, 10.0, 8.5, 0.5,7.5,19.0,85.0,7.5,17.0},0};

void loadConfig() {
  int i;
  // To make sure there are settings, and they are YOURS!
  // If nothing is found it will use the default settings.
  if (EEPROM.read(CONFIG_START + 0) == CONFIG_VERSION[0] &&
      EEPROM.read(CONFIG_START + 1) == CONFIG_VERSION[1] &&
      EEPROM.read(CONFIG_START + 2) == CONFIG_VERSION[2] &&
      EEPROM.read(CONFIG_START + 3) == CONFIG_VERSION[3])
    for (unsigned int t=0; t<sizeof(storage); t++)
      *((char*)&storage + t) = EEPROM.read(CONFIG_START + t);
  else {
    // here we should copy the settings to the storage variable
    strcpy(storage.version, CONFIG_VERSION);
    for (i=0;i<n_settings;i++) {storage.settings[i]=settings[i].val;}
    storage.relayState = 0;
  }
}

void saveConfig() {
  for (unsigned int t=0; t<sizeof(storage); t++)
    EEPROM.write(CONFIG_START + t, *((char*)&storage + t));
}

//void setup() {
//  loadConfig();
//}
//
//void loop() {
//  // [...]
//  int i = storage.c - 'a';
//  // [...]
//
//  // [...]
//  storage.c = 'a';
//  if (ok)
//    saveConfig();
//  // [...]
//}
