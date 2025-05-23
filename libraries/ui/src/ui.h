// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.1
// LVGL version: 8.3.11
// Project name: tempController_2_gui

#ifndef _TEMPCONTROLLER_2_GUI_UI_H
#define _TEMPCONTROLLER_2_GUI_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined __has_include
#if __has_include("lvgl.h")
#include "lvgl.h"
#elif __has_include("lvgl/lvgl.h")
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif
#else
#include "lvgl.h"
#endif

#include "ui_helpers.h"
#include "ui_events.h"

// SCREEN: ui_MainScreen
void ui_MainScreen_screen_init(void);
extern lv_obj_t * ui_MainScreen;
extern lv_obj_t * ui_Panel1;
extern lv_obj_t * ui_tempLabel;
extern lv_obj_t * ui_Label2;
extern lv_obj_t * ui_Image1;
extern lv_obj_t * ui_Panel2;
extern lv_obj_t * ui_wifiStatusLabel;
extern lv_obj_t * ui_ventStatusLabel;
extern lv_obj_t * ui_shadeStatusLabel;
extern lv_obj_t * ui_heaterStatusLabel;
extern lv_obj_t * ui_wifiStatusLabel1;
extern lv_obj_t * ui_ventStatusLabel1;
extern lv_obj_t * ui_heaterStatusLabel1;
extern lv_obj_t * ui_shadeStatusLabel1;
extern lv_obj_t * ui_otherLabel1;
extern lv_obj_t * ui_Panel3;
extern lv_obj_t * ui_Label3;
extern lv_obj_t * ui_Label1;
extern lv_obj_t * ui_otherLabel;
extern lv_obj_t * ui_Label5;
extern lv_obj_t * ui_Label6;
extern lv_obj_t * ui_Label7;
extern lv_obj_t * ui_Label8;
extern lv_obj_t * ui_Label9;
extern lv_obj_t * ui_vent25;
extern lv_obj_t * ui_vent50;
extern lv_obj_t * ui_vent100;
extern lv_obj_t * ui_Label4;
extern lv_obj_t * ui_Label10;
extern lv_obj_t * ui_Label11;
extern lv_obj_t * ui_heatNight;
extern lv_obj_t * ui_heatDay;
extern lv_obj_t * ui_startDay;
extern lv_obj_t * ui_Label12;
extern lv_obj_t * ui_startNight;
extern lv_obj_t * ui_startShade;
extern lv_obj_t * ui_Label13;
extern lv_obj_t * ui_Label14;
extern lv_obj_t * ui_endShade;
extern lv_obj_t * ui_Label15;
extern lv_obj_t * ui_Label16;
extern lv_obj_t * ui_Label17;
extern lv_obj_t * ui_boostStart;
extern lv_obj_t * ui_boostTemp;
extern lv_obj_t * ui_boostDur;
extern lv_obj_t * ui_tempChart;
extern lv_obj_t * ui_date;
extern lv_obj_t * ui_time;
extern lv_obj_t * ui_Relay_States;
extern lv_obj_t * ui_boostStatusLabel;
extern lv_obj_t * ui_boxVentOpenIndicator;
extern lv_obj_t * ui_boxVentCloseIndicator;
extern lv_obj_t * ui_boxHeaterIndicator;
extern lv_obj_t * ui_boxShadeCloseIndicator;
extern lv_obj_t * ui_boxShadeOpenIndicator;
extern lv_obj_t * ui_Relay_States1;
extern lv_obj_t * ui_Relay_States2;
extern lv_obj_t * ui_Relay_States3;
extern lv_obj_t * ui_Relay_States4;
extern lv_obj_t * ui_Relay_States5;
extern lv_obj_t * ui_Relay_States6;
extern lv_obj_t * ui_Relay_States7;
// CUSTOM VARIABLES

// EVENTS
extern lv_obj_t * ui____initial_actions0;

// IMAGES AND IMAGE SETS
LV_IMG_DECLARE(ui_img_cgflogo_png);    // assets/cgfLogo.png

// UI INIT
void ui_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
