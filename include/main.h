#ifndef H_MAIN
#define H_MAIN

#define DEV_BOARD

#define DEBUGSERIAL
#ifdef DEBUGSERIAL
#define LOG(arg) Serial.print("[");Serial.print((const char*)__FUNCTION__);Serial.print("] ");Serial.print(arg);Serial.print("\r\n");
#else
#define LOG(arg) ;
#endif

////////////////////////////////////
// includes
#include <Arduino.h>
// OLED 
// https://github.com/ThingPulse/esp8266-oled-ssd1306
// ESP8266 and ESP32 Oled Driver for SSD1306 display by Daniel Eichhorn
#include <wire.h>
#include <SSD1306Wire.h>
#include "wc_font.h"
// RTC 
// RTClib by Adafruit
#include <RTClib.h>
#include "WIFI_pwd.h"
#include <WiFi.h>
#include <time.h>
// Pixels
// NeoPixelBus by Michael Miller
// https://github.com/Makuna/NeoPixelBus
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>


////////////////////////////////////
// Global Variables
SSD1306Wire  myDisplay(0x3c, 21, 22);
RTC_DS3231 myRtc;

const char* DisplayModeText[] = {"DoW & Date & Time","Big Seconds","Time & Date","Off"};
const char* PanelColorModeText[] = {"Rainbow","White","Blue","Green","Red","60sec Wheel"};
const char* MinutesColorModeText[] = {"Rainbow","White","Blue","Green","Red","60sec Wheel"};

DateTime timeNow;
DateTime prevTimeNow;
const char* dayNames[] = {"Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"};

struct Message {
   String message; // the message to print
   uint32_t disptime; // message was printes at timestamp
   uint16_t showSeconds=10; // message should be shown x seconds
} displayMessage; 
struct Message oldDisplayMessage; 

/////////////////////////////////////
// LEDs
// 1st: the character matrix
// 2nd: the 4 minutes led
#ifdef DEV_BOARD
const uint8_t PanelWidth = 11; //test
const uint8_t PanelHeight = 1;
#else
const uint8_t PanelWidth = 11;
const uint8_t PanelHeight = 10;
#endif
const uint8_t PanelPixelCount = PanelWidth * PanelHeight;
const uint8_t PanelPixelPin = 18;
const uint8_t MinutesPixelCount = 4;
const uint8_t MinutesPixelStart = PanelPixelCount;
NeoTopology <RowMajorLayout> topo(PanelWidth, PanelHeight);
NeoPixelBus <NeoGrbwFeature, Neo800KbpsMethod> PanelStrip(PanelPixelCount + MinutesPixelCount, PanelPixelPin);
NeoPixelAnimator PanelAnimation(PanelPixelCount + MinutesPixelCount, NEO_CENTISECONDS);

struct PanelAnimationState
{
    RgbwColor StartingColor;  // the color the animation starts at
    RgbwColor EndingColor; // the color the animation will end at
}; 

PanelAnimationState StripState[PanelPixelCount + MinutesPixelCount];

/////////////////////////////////////
// brightness (value between 0..1)
const uint8_t LDRPin = A0;
const float minBrightness = 0.05;
const float maxBrightness = 1.0;
float currentBright = 1.0;

/////////////////////////////////////
// switches
const uint8_t BtnPin[] = {5,17,16,25,26,27,14,12}; // Pins of swiches
unsigned long BtnOn[] = {0,0,0,0,0,0,0,0}; // if button was released: store button pressed duration
unsigned long debounceBtn[] = {0,0,0,0,0,0,0,0}; // timestamp of button pressed
const unsigned long debounceMillis = 100; // ms short press
const unsigned long debounceMillisLong = 5000; // ms long press

uint8_t PanelColorMode=0;
//uint8_t PanelAnimMode=0 //todo
uint8_t MinutesColorMode=0;
uint8_t MinutesAnimMode=0; //0: light up all LED; 1: light up only one
bool PanelColorModeDirty;
bool MinutesColorModeDirty;
uint8_t DisplayMode=0;
String message;
time_t messageTimer;
const uint8_t DspMsgTime=5; //display message x seconds

/////////////////////////////////////
uint8_t oldt = 255; // store second value for refresh

/////////////////////////////////////
// 10*16-bit "panelMask" for each pixel (one variable is to small for 110 pixels)
uint16_t panelMask[10];
uint16_t oldPanelMask[10];
bool panelDirty;
/////////////////////////////////////
// 4 byte "minutesMask"
uint8_t minutesMask;
uint8_t oldMinutesMask;
bool minutesDirty;

// ESKISTEFÜNF
// ZEHNZWANZIG
// DREIVIERTEL
// VORNEUMNACH
// HALBXELFÜNF
// EINSTAGZWEI
// DREISTZWÖLF
// SIEBENZVIER
// NACHTASECHS
// ZEHNEUNKUHR

// define panelMasks for each word.
#define ES              panelMask[0] |= 0b1100000000000000
#define IST_1           panelMask[0] |= 0b0001110000000000
#define FUNF_1          panelMask[0] |= 0b0000000111100000
#define ZEHN_1          panelMask[1] |= 0b1111000000000000
#define ZWANZIG         panelMask[1] |= 0b0000111111100000
#define DREI_1          panelMask[2] |= 0b1111000000000000
#define VIER_1          panelMask[2] |= 0b0000111100000000
#define DREIVIERTEL     panelMask[2] |= 0b1111111111100000
#define VIERTEL         panelMask[2] |= 0b0000111111100000
#define VOR             panelMask[3] |= 0b1110000000000000
#define UM              panelMask[3] |= 0b0000011000000000
#define NACH_1          panelMask[3] |= 0b0000000111100000
#define HALB            panelMask[4] |= 0b1111000000000000
#define ELF             panelMask[4] |= 0b0000011100000000
#define FUNF_2          panelMask[4] |= 0b0000000111100000
#define EIN             panelMask[5] |= 0b1110000000000000
#define EINS            panelMask[5] |= 0b1111000000000000
#define TAG             panelMask[5] |= 0b0000111000000000
#define ZWEI            panelMask[5] |= 0b0000000111100000
#define DREI_2          panelMask[6] |= 0b1111000000000000
#define IST_2           panelMask[6] |= 0b0001110000000000
#define ZWOLF           panelMask[6] |= 0b0000001111100000
#define SIEBEN          panelMask[7] |= 0b1111110000000000
#define VIER_2          panelMask[7] |= 0b0000000111100000
#define NACH_2          panelMask[8] |= 0b1111000000000000
#define NACHT           panelMask[8] |= 0b1111100000000000
#define ACHT            panelMask[8] |= 0b0111100000000000
#define SECHS           panelMask[8] |= 0b0000001111100000
#define ZEHN_2          panelMask[9] |= 0b1111000000000000
#define NEUN            panelMask[9] |= 0b0001111000000000
#define UHR             panelMask[9] |= 0b0000000011100000

/////////////////////////////////////
// do something with the buttons
void doButtons(void);

/////////////////////////////////////
// set display depending on mode
void setDisplay(DateTime t); 

/////////////////////////////////////
// set all Masks for current time
void setPixels(void);

/////////////////////////////////////
// get time and set panelMask
void getTimeText(void);

/////////////////////////////////////
// hours text
void HourText(uint8_t h);

/////////////////////////////////////
// setup Pixelsanimation for PanelStrip from panelMask
void SetupPanelAnimation();

/////////////////////////////////////
// get Minutes and set minutesMask
void getMinutesText(void);

/////////////////////////////////////
// set Pixelsanmiation for MinutesStrip from minutesMask
void SetupMinutesAnimation();

/////////////////////////////////////
// simple fading
void FadeAnim(AnimationParam param); 

/////////////////////////////////////
// check if the mask has changed -> new animation
bool MaskHasChanged();

/////////////////////////////////////
// RGB Collor wheel
RgbwColor colorWheel(uint16_t wheelsteps, uint16_t curstep, float currentBright,uint16_t offset);

/////////////////////////////////////
// Tests
void PixelColorWheel(uint8_t from, uint8_t to, uint8_t wait);



//xxxxxxxxxxxxxxxxxx

////////////////////////////////////
// Functions
////////////////////////////////////
// Connect to Wifi
// Get Time using NTP
// Close WiFi
void getNTPTime();

////////////////////////////////////
// Determine summertime
// true: is is summertime in DE
// false: no summertime in DE
boolean summertime_DE(DateTime);

////////////////////////////////////
// Set Display Text accoring to Display Mode or displayMessage content
// does _not_ call display.display()
void setDisplay(DateTime);

#endif