/*
 * Author: Chris DeVries
 * Date:2018/11/11
 * Email:chris@commongroundfarm.ca
 * Function:
 * release notes:
 * 1.4 - new build (v2 hardware) reset all relay numbers, etc
 * 1.5 - change boost program to use boiler too
 * 1.6 - take over and use only follower controller (relays)
 *     - add failsafe to turn on boiler controls if high boiler temp seen - done not tested
 * 1.7 - installing watchdog timer 
 *     - turning all debug serial ports off when not debugging
 *     - trying to get rid of String types
 *     - fixing the temp initially causing propane to come on
 * 1.8 - adding control for energy curtain based on time
 *     - further remove Strings! hope this helps!
 */
//#define DEBUG

#ifdef DEBUG
  #define SERIALPRINT(x) Serial.print(x)
  #define SERIALPRINTLN(x) Serial.println(x)
#else
  #define SERIALPRINT(x)
  #define SERIALPRINTLN(x)
#endif

#define MEGABUILD
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include "RTClib.h"
#include "hardwareDef.h"
#include <controlSetup.h>
#include "config.h"
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <avr/wdt.h>

// timer setup
RTC_DS3231 rtc;
 // Setup a oneWire instance to communicate with a OneWire device
OneWire oneWire1(ONE_WIRE_BUS1);
OneWire oneWire2(ONE_WIRE_BUS2);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature frontTemp(&oneWire1);
DallasTemperature supplyTemp(&oneWire2);
Adafruit_7segment tempSevSeg = Adafruit_7segment();

// global vars for sensor reading
int n_CurrentSense=0;
unsigned long currentRead=0;
const int sensReadRate = 10000;
unsigned long lastSensRead = sensReadRate;  // force an immediate sensor read

// globals for settings
unsigned long settingsChangeTimer=0;
bool settingsChanged=false; 
bool boostOn=false; 
const unsigned int settingsStoreRate=60*1000;   // store every 1 min or so

// global variables for the first display line
LiquidCrystal_I2C lcd(lcd_address,20,4);  // define I2c lcd
unsigned long lastSensDisplayUpdate = 0;
int sensDisplayState = 0;
const int n_sensDisplayState = n_sensors;
const int sensDisplayRate = 2000;

// global variables for second display line
unsigned long lastUIDisplayUpdate = 0;
const int UIDisplayTempTimeout = 30000;
const int n_UIDisplayState = n_settings+1; // last one for time display
int UIDisplayState = n_UIDisplayState-1;

// button state
unsigned int buttonStateLast = 0;
unsigned int buttonStateLastAction = 0;
unsigned long lastDebounceTime = 0;
unsigned int buttonDebounceDelay = 50;

// relay update
unsigned long lastRelayUpdate = 0;
unsigned long relayUpdateRate = 5000;

// vent settings
// status settings for temperature (set in relay update because of legacy)
bool vent1temp=false;
bool vent2temp=false;
bool vent3temp=false;
bool ventMoving=false;
unsigned int ventState=0;  // indicates where the vent currently is
#define VENT_0 0
#define VENT_25 1
#define VENT_50 2
#define VENT_100 3
const unsigned int ventOpenTimeout = 30000;    // how much time to move between states (fully open takes 110 seconds)
const unsigned int ventReDoTimeout = 5000;      // if fully closed or fully open, how long to re-open or re-close
unsigned long ventReDoInterval = 120000;  // how often to re-open or re-close
unsigned long ventTimer=0;
unsigned long ventReDoTimer=0;
int ventTimeout;                       // vent timeout (can be either open or redo)

void setup()                                                                                     
{
  int i;
  settingsInit();
  loadConfig();
  // copy settings into active use
  for (i=0;i<n_settings;i++) {settings[i].val=storage.settings[i];}
  Serial.begin(115200);
  Serial1.begin(9600);  //serial1 to communicate with a nodemcu
  
  printSettings();
  tempSevSeg.begin(DISPLAY_ADDRESS);
  lcd.init();  //initialize the lcdv
  lcd.backlight();  //open the backlight 
   
  frontTemp.begin();
  frontTemp.setResolution(11);
  supplyTemp.begin();
  supplyTemp.setResolution(11);
  analogReference(EXTERNAL);
  analogRead(0);
  
  lcd.noAutoscroll();
  // button setup
  pinMode(butValUp, INPUT);
  pinMode(butValDown, INPUT);
  pinMode(butSelUp, INPUT);  
  pinMode(butSelDown, INPUT);
  // analog input setup
  pinMode(SENSOR4_PIN,INPUT);
  
  //relay setup
  //pinMode(settings[i].pin,OUTPUT);
  pinMode(PROPANE_WEST_FAN_RELAY,OUTPUT);
  pinMode(PROPANE_WEST_HEAT_RELAY,OUTPUT);
  pinMode(PROPANE_EAST_FAN_RELAY,OUTPUT);
  pinMode(PROPANE_EAST_HEAT_RELAY,OUTPUT);
  pinMode(BOILER_EAST_RELAY,OUTPUT);
  pinMode(BOILER_WEST_RELAY,OUTPUT);
  pinMode(VENT_OPEN_RELAY,OUTPUT);
  pinMode(VENT_CLOSE_RELAY,OUTPUT);
  pinMode(VENT_100_RELAY,OUTPUT);
  pinMode(CURTAIN_OPEN_RELAY,OUTPUT);  
  pinMode(CURTAIN_CLOSE_RELAY,OUTPUT);   

  SERIALPRINTLN("setting up relays");
  //read stored relay state from config
  relayState = storage.relayState;
  relayOutput();
  lastRelayUpdate = millis();
  // watchdog timer setup
  wdt_enable(WDTO_8S);

  Serial.print("loop starting");
}
void loop()
{
  getTemps();

  // print to the LCD display:
  sensorDisplay();
  // update the UI Display
  UIDisplay();
  // act on button inputs:
  buttonInputs();
  // relay outputs
  if (millis()-lastRelayUpdate>relayUpdateRate){
      relayUpdate();
      lastRelayUpdate = millis(); 
  }
  ventUpdate();

  // store settings if necessary
  storeSettings();

  // reset the watchdog timer
  wdt_reset();
 }

 void curtainStateCheck(){
  DateTime now;
  float hrmin;
  hrmin = now.hour()+float(now.minute())/60;
  if ((hrmin>settings[CURTAIN_MORNING].val)&(hrmin<settings[CURTAIN_EVENING].val)) {
    relayState=relayState | (1<<CURTAIN_OPEN);
    relayState=relayState & ~(1<<CURTAIN_CLOSE);
  }
  else {
    relayState=relayState | (1<<CURTAIN_CLOSE);
    relayState=relayState & ~(1<<CURTAIN_OPEN);
  }

 }

void storeSettings(){
  int i;
  //SERIALPRINTLN("check if settings changed");
  // check if it's time to store settings
  if(settingsChanged){
    SERIALPRINT(settingsStoreRate);SERIALPRINT("..");SERIALPRINTLN(millis()-settingsChangeTimer);
    if(millis()-settingsChangeTimer>settingsStoreRate){
      for (i=0;i<n_settings;i++) {storage.settings[i]=settings[i].val;}
      storage.relayState = relayState;
      saveConfig();
      SERIALPRINT("settings stored");
      printSettings();
      settingsChanged=false;
      SERIALPRINTLN("settings changes stored");
    }
  }
  else {
    //printSettings(settings);
    //check if settings have changed, if so will be stored after timer expires
    for (i=0;i<n_settings;i++) {
      if (settings[i].val!=storage.settings[i]) {SERIALPRINTLN("settings changed"); settingsChanged=true; settingsChangeTimer=millis();}
    }
  }
}

void printSettings() {
  int i;
  SERIALPRINTLN(" Setting    Value   Controlled By    Pin  ");
  for (i=0;i<n_settings;i++) {
    SERIALPRINT(settings[i].disp);
    SERIALPRINT("\t");
    SERIALPRINT(settings[i].val);
    SERIALPRINT("\t");
    SERIALPRINT(settings[i].sens);
    SERIALPRINT("\t");
    SERIALPRINTLN(settings[i].relayIndex);
    }
}

void relayUpdate()
{
// this function will update all relays
int i;
float hysteresis = settings[HYS].val;
float tempSet;
DateTime now;
float hrmin;
now = rtc.now();
hrmin = now.hour()+float(now.minute())/60;
for (i=0;i<n_settings;i++)
{
  switch (i){
    case BOILD:   // take care of both boilers
      // check for boost
      if ((hrmin>settings[START].val)&&(hrmin<settings[START].val+settings[DUR].val)) {  
          SERIALPRINT("Boost on at ");SERIALPRINTLN(settings[BOOST].val);
          tempSet = settings[BOOST].val;
          boostOn=true; 
          }
      else if ((hrmin>settings[DAY_START].val)&&(hrmin<settings[DAY_END].val)) {
        tempSet=settings[BOILD].val;
      }
      else {tempSet=settings[BOILN].val;}
      // now check sensor value and compare to the tempSet value and set the relay state
      if (sens[settings[BOILD].sens].val<tempSet) {
        // set the relay status to high
        relayState = relayState | (1<<BOILER_EAST);
        relayState = relayState | (1<<BOILER_WEST);
        SERIALPRINTLN("Boiler on, setting: " + String(tempSet) + " sensor: "+String(sens[settings[BOILD].sens].val));
      }
      else if (sens[settings[BOILD].sens].val>tempSet+hysteresis) {
        relayState = relayState & ~(1<<BOILER_EAST);
        relayState = relayState & ~(1<<BOILER_WEST);
      }   
      // check the boiler incoming water temperature.  If it is too high, then turn relays on:
      if (sens[settings[BOILOVER].sens].val>settings[BOILOVER].val) {
        // set the relay status to high
        relayState = relayState | (1<<BOILER_EAST);
        relayState = relayState | (1<<BOILER_WEST);
        SERIALPRINTLN("boiler supply high, boilers turning on! temp: " +String(sens[settings[BOILOVER].sens].val) + " setting:" + String(settings[BOILOVER].val));
      }
    break;
    case PROPW: //"Propane W":
    case PROPE: //"Propane E":
        //SERIALPRINT(hrmin);SERIALPRINT("..");SERIALPRINT(settings[11]);SERIALPRINT("..");SERIALPRINTLN(settings[11]+settings[12]);
        if ((hrmin>settings[START].val)&&(hrmin<settings[START].val+settings[DUR].val)) {  // boost temperature for propane
          SERIALPRINT("Boost on at ");SERIALPRINTLN(settings[BOOST].val);
          tempSet = settings[BOOST].val;
          boostOn=true; 
          }
        else {
          tempSet = settings[i].val;
          boostOn=false; 
        }
        if (sens[settings[i].sens].val<tempSet) {
          relayState = relayState | (1<<settings[i].relayIndex);
          SERIALPRINTLN(settings[i].disp + " on, setting: " + String(settings[i].val) + " sensor: "+String(sens[settings[i].sens].val));
        }
        else if (sens[settings[i].sens].val>tempSet+hysteresis) {
          relayState = relayState & ~(1<<settings[i].relayIndex);
        }        
    break;
    case VENT1: //"vent 25%":
    // change vent state
      if (sens[settings[i].sens].val>settings[i].val) {
        vent1temp=true; 
      }
      else if (sens[settings[i].sens].val<settings[i].val-hysteresis) {
        vent1temp=false;
      }        
      break;
    case VENT2: //"vent 50%":
    // change vent state
      if (sens[settings[i].sens].val>settings[i].val) {
        vent2temp=true; 
      }
      else if (sens[settings[i].sens].val<settings[i].val-hysteresis) {
        vent2temp=false;
      }        
      break;
    case VENT3: //"vent 100%":
    // change vent state
      if (sens[settings[i].sens].val>settings[i].val) {
        vent3temp=true; 
      }
      else if (sens[settings[i].sens].val<settings[i].val-hysteresis) {
        vent3temp=false;
      }        
      break;
      
      
    case FANW: //"Fan W":
    case FANE: //"Fan E":
        if (settings[i].val>0.05) {
          relayState = relayState | (1<<settings[i].relayIndex);
          SERIALPRINTLN(settings[i].disp + " on");
          settings[i].val==1.0;
        }
        else {
          relayState = relayState & ~(1<<settings[i].relayIndex);
          settings[i].val==0.0;
        }
      break;
    case CURTAIN_MORNING:
    case CURTAIN_EVENING:
      if ((hrmin>settings[CURTAIN_MORNING].val)&(hrmin<settings[CURTAIN_EVENING].val)) {
        relayState=relayState | (1<<CURTAIN_OPEN);
        relayState=relayState & ~(1<<CURTAIN_CLOSE);
      }
      else {
        relayState=relayState | (1<<CURTAIN_CLOSE);
        relayState=relayState & ~(1<<CURTAIN_OPEN);
      }
      break;
    
  }
} 
// relay output
relayOutput();
SERIALPRINT("Relay Output: ");
SERIALPRINTLN(relayState);
}

void relayOutput(){
  // given relay state, update all the outputs
   if (relayState&(1<<PROPANE_WEST_HEAT))   {digitalWrite(PROPANE_WEST_HEAT_RELAY,LOW);} else {digitalWrite(PROPANE_WEST_HEAT_RELAY,HIGH);}
   if (relayState&(1<<PROPANE_WEST_FAN))   {digitalWrite(PROPANE_WEST_FAN_RELAY,LOW);} else {digitalWrite(PROPANE_WEST_FAN_RELAY,HIGH);}
   if (relayState&(1<<PROPANE_EAST_HEAT))   {digitalWrite(PROPANE_EAST_HEAT_RELAY,LOW);} else {digitalWrite(PROPANE_EAST_HEAT_RELAY,HIGH);}
   if (relayState&(1<<PROPANE_EAST_FAN))   {digitalWrite(PROPANE_EAST_FAN_RELAY,LOW);} else {digitalWrite(PROPANE_EAST_FAN_RELAY,HIGH);}
   if (relayState&(1<<BOILER_EAST))   {digitalWrite(BOILER_EAST_RELAY,LOW);} else {digitalWrite(BOILER_EAST_RELAY,HIGH);}
   if (relayState&(1<<BOILER_WEST))   {digitalWrite(BOILER_WEST_RELAY,LOW);} else {digitalWrite(BOILER_WEST_RELAY,HIGH);}
   if (relayState&(1<<VENT_OPEN))   {digitalWrite(VENT_OPEN_RELAY,LOW);} else {digitalWrite(VENT_OPEN_RELAY,HIGH);}
   if (relayState&(1<<VENT_CLOSE))   {digitalWrite(VENT_CLOSE_RELAY,LOW);} else {digitalWrite(VENT_CLOSE_RELAY,HIGH);}
   if (relayState&(1<<CURTAIN_OPEN))   {digitalWrite(CURTAIN_OPEN_RELAY,LOW);} else {digitalWrite(CURTAIN_OPEN_RELAY,HIGH);} 
   if (relayState&(1<<CURTAIN_CLOSE))   {digitalWrite(CURTAIN_CLOSE_RELAY,LOW);} else {digitalWrite(CURTAIN_CLOSE_RELAY,HIGH);} 
}
void ventClose(){
SERIALPRINTLN("vent closing, state: " + String(ventState));
        ventTimeout=ventOpenTimeout;
        relayState=relayState | (1<<VENT_CLOSE);
        ventTimer=millis();
        relayOutput();
        if (ventState>0) ventState--;
        ventMoving=true;
        sens[4].val = float(ventState);  // used for displaying ventState on LCD
        SERIALPRINT("Relay Output: ");
SERIALPRINTLN(relayState);
}
void ventOpen(){
        SERIALPRINTLN("vent opening, state: " + String(ventState));
        ventTimeout=ventOpenTimeout;
        relayState=relayState | (1<<VENT_OPEN);
        ventTimer=millis();
        relayOutput();
        ventState++;
        ventMoving=true;
        sens[4].val = float(ventState);
        SERIALPRINT("Relay Output: ");
SERIALPRINTLN(relayState);
}
void ventUpdate() {
  // check if vent is opening or closing
  if (ventMoving) {
    // vent is opening or closing, wait for timer
    if (millis()-ventTimer>ventTimeout) {
      SERIALPRINTLN("vent opening or closing, timer expired; stopping");
      // shut off relay
      relayState = relayState & ~(1<<VENT_OPEN);
      relayState = relayState & ~(1<<VENT_CLOSE);
      relayOutput();
      ventMoving=false;
    }
  }
  else  {// vents not currently opening
    if (vent3temp) {    // above highest temp
      if (ventState==VENT_100) {  
        if (millis()-ventReDoTimer > ventReDoInterval) {
          SERIALPRINTLN("vent 100, opening with re-do timer");
          ventTimeout=ventReDoTimeout;  
          relayState=relayState | (1<<VENT_OPEN);
          ventTimer=millis();
          relayOutput();
          ventMoving=true;
          ventReDoTimer=millis();
        }
      }
      else { // not all the way open, need to open further
        SERIALPRINTLN("vent 100, opening");
        ventOpen();
      }
    }
    else if (vent2temp) {
        if (ventState>VENT_50) {ventClose(); SERIALPRINTLN("vent 50, closing");}
        else if (ventState<VENT_50) {ventOpen(); SERIALPRINTLN("vent 50, opening");}
    }
    else if (vent1temp) {
        if (ventState>VENT_25) {ventClose(); SERIALPRINTLN("vent 25, closing");}
        else if (ventState<VENT_25) {ventOpen(); SERIALPRINTLN("vent 25, opening");}
    }
    else { // vent temp is below lowest temp 
      if (ventState==VENT_0) {
        if (millis()-ventReDoTimer > ventReDoInterval) {
          SERIALPRINTLN("vent closed, closing with re-do timer");
          SERIALPRINTLN(String(ventReDoTimer) + " " + String(ventReDoInterval) + " " + String(millis()));
          ventTimeout=ventReDoTimeout;  
          relayState=relayState | (1<<VENT_CLOSE);
          ventTimer=millis();
          relayOutput();
          ventMoving=true;
          ventReDoTimer=millis();
        }
      }
      else {
        ventClose();
        //SERIALPRINTLN(String(millis()-ventReDoTimer) + " " + String(ventReDoInterval) + " " + String(millis())); 
        SERIALPRINTLN("vent 0, closing");
        }
    }
    
  }
}
void buttonInputs()
{
  unsigned int buttonState=0;
  bool dir=true;  // value change direction (true is up)
  // debounce buttons
  buttonState = buttonState | digitalRead(butSelUp);
  buttonState = buttonState | digitalRead(butSelDown)<<1;
  buttonState = buttonState | digitalRead(butValUp)<<2;
  buttonState = buttonState | digitalRead(butValDown)<<3;
  // invert button press since they are pulled up
  buttonState = (~buttonState) & buttonMask;
  //SERIALPRINTLN("buttons " + buttonState);
  if (buttonState!=buttonStateLast){
    lastDebounceTime=millis();
  }
  if (((millis()-lastDebounceTime)>buttonDebounceDelay)&(buttonState!=buttonStateLastAction))
  {
    SERIALPRINTLN("debounced button press ... " + String(buttonState));
    lastUIDisplayUpdate=millis(); // prevent display from timing out back to time
    //read sel button up
    if (buttonState&2){
      // move selection state up
      UIDisplayState++;
      if (UIDisplayState>=n_UIDisplayState) {
        UIDisplayState=0;
      }
    }
    else if (buttonState&1) {
      // move selection state down
      UIDisplayState--;
      if (UIDisplayState<0) {
        UIDisplayState=n_UIDisplayState-1;
      }
    }
    else if (buttonState&4) {
      // move value up
      settings[UIDisplayState].val += 0.5;
        
    }
    else if (buttonState&8) {
      // move value down
      settings[UIDisplayState].val -= 0.5;
    }
    // update last action button press
    buttonStateLastAction=buttonState;
    
  }
  // update last button read
  buttonStateLast=buttonState;
  
 // artificially cause settings to change
 //settings[1].val = random(-10,35);     
}

void getTemps() {
  int s4_in;
  int i;
  // procedure for reading all sensors in list
  float tempReading;
  if (millis()-lastSensRead>sensReadRate)
    {
    // get temperatures:
    SERIALPRINT("Requesting temperatures...");
    frontTemp.requestTemperatures(); // Send the command to get temperatures
    supplyTemp.requestTemperatures(); // Send the command to get temperatures
    SERIALPRINTLN("DONE");
    
  
    // get temperature 1
    if (checkTemp(tempReading=frontTemp.getTempC(sensor1))) {
      sens[0].val=tempReading;
    }
    //sens[0].val=random(100);
    //SERIALPRINT("Temperature Sensor 1: ");
    //SERIALPRINT(tempReading); 
    // get temperature 2
    if (checkTemp(tempReading=supplyTemp.getTempC(sensor2))) {
      sens[1].val=tempReading;
    }
    SERIALPRINT("Temperature Sensor 2: ");
    SERIALPRINTLN(tempReading); 
    // get temperature 3
    if (checkTemp(tempReading=supplyTemp.getTempC(sensor3))) {
      sens[2].val=tempReading;
    }
    SERIALPRINT("Temperature Sensor 3: ");
    SERIALPRINTLN(tempReading); 
    
    // read from sensor 4 (aspirator)
    s4_in=analogRead(SENSOR4_PIN);
    SERIALPRINTLN(s4_in);
    //SERIALPRINTLN(S4_SLOPE);
    //SERIALPRINTLN(s4_in-S4_CAL_V2);
    sens[3].val=float(S4_CAL_T2)+float(S4_SLOPE)*float(s4_in-S4_CAL_V2);
/*
    // calculate avg power (W) and store in sensor 5
    // 5/1024/50*100/50m*240
    sens[4].val = float(currentRead) / n_CurrentSense*46.8;
    currentRead=0;n_CurrentSense=0;
*/

    mega2esp(PRINT_TIME);
    mega2esp(PRINT_SETTINGS);
    mega2esp(PRINT_SENSORS);
    mega2esp(PRINT_RELAY_STATUS);
      
    // print the sensor which is controlling the first relay
    tempSevSeg.print(sens[settings[1].sens].val);
    // print current reading
    //tempSevSeg.print(sens[4]);
    tempSevSeg.writeDisplay();
    lastSensRead=millis();

    
    }
    
  // read from the current sensor outside of sensor timer and integrate
  //currentRead+=analogRead(ISENSE_PIN);
  //n_CurrentSense++;
  //SERIALPRINT(sens[4]);
  //SERIALPRINT("  ");
  //SERIALPRINTLN(currentRead);
}

void mega2esp(int printOption){
  int i;
  if (printOption==PRINT_TIME) {
    Serial1.write(codeStart-1);
    DateTime now = rtc.now();
    Serial1.print(now.unixtime());
    //SERIALPRINTLN(now.unixtime());
    /*
     * 
    
    Serial1.print(now.hour(), DEC);
    Serial1.print(':');
    Serial1.print(now.minute(),DEC);
    Serial1.print(':');
    Serial1.print(now.second(),DEC);
    Serial1.print(" ");
    Serial1.print(now.day(),DEC);
    Serial1.print('/');
    Serial1.print(now.month(),DEC);
    Serial1.print('/');
    Serial1.println(now.year(),DEC);
    */
  }
  if (printOption==PRINT_SETTINGS){
  // first print all settings to the serial1 port
    for (i=0;i<n_settings;i++) {
      Serial1.write(codeStart+i);
      Serial1.print(settings[i].val);
      //Serial.write(codeStart+i);
      //SERIALPRINT(settings[i].val);
    }
  }
  else if (printOption==PRINT_SENSORS) {
    for (i=0;i<n_sensors;i++) {
      Serial1.write(codeStart+i+n_settings);
      Serial1.print(sens[i].val);
      //Serial.write(codeStart+i+n_settings);
      //SERIALPRINT(settings[i].val);
    }
  }
  else if (printOption==PRINT_RELAY_STATUS) {
    Serial1.write(codeStart+n_sensors+n_settings);
    Serial1.print(relayState);
    //Serial.write(codeStart+n_sensors+n_settings);
    //SERIALPRINTLN(relayState);
  }
}
bool checkTemp(float tempIn)
  {
    //SERIALPRINTLN(tempIn);
    // validate the temperature to be withing a certain range
    if ((tempIn>TEMP_MIN)&&(tempIn<TEMP_MAX)) {
      return(true);
    }
    else {
      SERIALPRINT("Invalid Temp reading: ");
      SERIALPRINTLN(tempIn);
      return(false);
    }
  }
  
  
void sensorDisplay() {
  if (millis()-lastSensDisplayUpdate>sensDisplayRate)
    {
    lcd.setCursor(0,0);
    lcd.print(sens[sensDisplayState].disp);
    lcd.print(": ");
    lcd.print(sens[sensDisplayState].val,3);
    lcd.print("      ");
    sensDisplayState++;
    if (sensDisplayState>=n_sensDisplayState){
      sensDisplayState=0;
      }
    lastSensDisplayUpdate=millis();
    }

}
void UIDisplay() {
    DateTime now;
    lcd.setCursor(0,1);
    //if (UIDisplayState>n_UIDisplayState-1) {UIDisplayState==0;}
    if (UIDisplayState<n_UIDisplayState-1) {
      // display setting
      lcd.print(settings[UIDisplayState].disp);
      lcd.print(": ");
      lcd.print(String(settings[UIDisplayState].val,1)); 
      lcd.print("         ");
    }
    else {
    // print time
      now = rtc.now();
      lcd.print(now.hour(), DEC);
      lcd.print(':');
      lcd.print(timeFormat(now.minute()));
      lcd.print(':');
      lcd.print(timeFormat(now.second()));
      lcd.print("  ");
      if (boostOn) {lcd.print("Boost   ");}
      else {
        lcd.print(timeFormat(now.day()));
        lcd.print('/');
        lcd.print(timeFormat(now.month()));
        lcd.print("       ");
      }
      lastUIDisplayUpdate=millis();
      }
      
      if ((millis()-lastUIDisplayUpdate)>UIDisplayTempTimeout) {
        // switch back to time of day display:
        UIDisplayState=n_UIDisplayState-1;
      }
}
String timeFormat(int val){
  if (val<10) {return("0" + String(val));}
  else {return(String(val));}
}
