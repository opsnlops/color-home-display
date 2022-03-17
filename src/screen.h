
#ifndef _APRILS_CREATURES_SCREEN
#define _APRILS_CREATURES_SCREEN

#include <Arduino.h>
#include <ArduinoJson.h>

#include <Adafruit_GFX.h>
#include <Adafruit_HX8357.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>

#include "logging/logging.h"

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

#define _CLOCK_CANVAS_WIDTH 212
#define _CLOCK_CANVAS_HEIGHT 48
#define _CLOCK_CANVAS_X SCREEN_WIDTH - _CLOCK_CANVAS_WIDTH
#define _CLOCK_CANVAS_Y SCREEN_WIDTH - _CLOCK_CANVAS_HEIGHT

// Screen pins
#define TFT_CS 33
#define TFT_DC 38
#define TFT_RST 1

// Colors
#define BACKGROUND_COLOR HX8357_BLACK
#define CLOCK_COLOR HX8357_MAGENTA

namespace creatures
{

    static Adafruit_HX8357 display  = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

    void printTime(char *clock_display);

    void powerUpDisplay();
    void initScreen();
    unsigned long wipeScreen();
}

#endif