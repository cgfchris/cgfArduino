
#define DISPLAY_ADDRESS   0x70
//#define CONPIN 6
#define ONE_WIRE_BUS1 46
#define ONE_WIRE_BUS2 48
#define RELAY1 30
#define RELAY2 31
#define RELAY3 32
#define RELAY4 33
#define RELAY5 34
#define RELAY6 35
#define RELAY7 36
#define RELAY8 37
#define RELAY9 38
#define RELAY10 39
#define RELAY11 40
#define RELAY12 41

#define PROPANE_WEST_FAN_RELAY RELAY7
#define PROPANE_WEST_HEAT_RELAY RELAY8
#define PROPANE_EAST_FAN_RELAY RELAY9
#define PROPANE_EAST_HEAT_RELAY RELAY10
#define BOILER_EAST_RELAY RELAY12
#define BOILER_WEST_RELAY RELAY11
#define VENT_OPEN_RELAY RELAY1
#define VENT_CLOSE_RELAY RELAY2
#define VENT_100_RELAY RELAY3
#define CURTAIN_OPEN_RELAY RELAY5
#define CURTAIN_CLOSE_RELAY RELAY6

#define TEMP_MIN -40.0
#define TEMP_MAX 99.9

const int butValUp=27;
const int butValDown=29;
const int butSelUp=25;
const int butSelDown=23;
const int buttonMask=B1111;

DeviceAddress sensor3 = { 0x28, 0xE8, 0xD4, 0x45, 0x92, 0xC, 0x2, 0x20 };
DeviceAddress sensor2 = { 0x28, 0xE6, 0xC8, 0x45, 0x92, 0xC, 0x2, 0x6E };
DeviceAddress sensor1=  { 0x28, 0x25, 0x42, 0x45, 0x92, 0x6, 0x2, 0x39 };

/* cal points
 *  from wadsworth:
 *  every 50 mv equals 9 F or 5 C
 *  and 50 mv over 3.3v ref
 *  is about 15.5 lsbs
 *  so I should do 10 C and 31 LSBs
 */

#define SENSOR4_PIN  0
#define S4_CAL_V1 913
#define S4_CAL_T1 27.5
#define S4_CAL_V2 882 
#define S4_CAL_T2 17.5
#define S4_SLOPE (S4_CAL_T1-S4_CAL_T2)/(S4_CAL_V1-S4_CAL_V2)

#define ISENSE_PIN 2
#define lcd_address 0x26
