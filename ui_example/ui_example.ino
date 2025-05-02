#include "Arduino_H7_Video.h"
#include "Arduino_GigaDisplayTouch.h"
#include "lvgl.h"
#include "ui.h"

/* Insert resolution WxH according to your SquareLine studio project settings */
Arduino_H7_Video          Display(800, 480, GigaDisplayShield); 
Arduino_GigaDisplayTouch  Touch;

void setup() {
  delay(3000);
  Display.begin();
  Touch.begin();

  ui_init();
}

void loop() {

  /* Feed LVGL engine */
  lv_timer_handler();
}


// Notes:
/*
 * https://docs.arduino.cc/tutorials/giga-display-shield/square-line-tutorial/
 * 
 * actually found the lv_conf file here:
 * /home/chris/snap/arduino/85/.arduino15/packages/arduino/hardware/mbed_giga/4.2.4/libraries/Arduino_H7_Video
 * 
 * which is super annoying
 */
 
