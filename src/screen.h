
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

#define _ERROR_CANVAS_WIDTH 380
#define _ERROR_CANVAS_HEIGHT 200
#define _ERROR_CANVAS_X 50
#define _ERROR_CANVAS_Y 60

#define _SYSTEM_MESSAGE_CANVAS_WIDTH 380
#define _SYSTEM_MESSAGE_CANVAS_HEIGHT 200
#define _SYSTEM_MESSAGE_CANVAS_X 50
#define _SYSTEM_MESSAGE_CANVAS_Y 60

#define _CLOCK_CANVAS_WIDTH 212
#define _CLOCK_CANVAS_HEIGHT 48
#define _CLOCK_CANVAS_X SCREEN_WIDTH - _CLOCK_CANVAS_WIDTH // Lower right
#define _CLOCK_CANVAS_Y SCREEN_HEIGHT - _CLOCK_CANVAS_HEIGHT

#define _TEMPERATURE_CANVAS_WIDTH 100
#define _TEMPERATURE_CANVAS_HEIGHT 48
#define _TEMPERATURE_CANVAS_X 5
#define _TEMPERATURE_CANVAS_Y 5

#define _WIND_CANVAS_WIDTH 180
#define _WIND_CANVAS_HEIGHT 48
#define _WIND_CANVAS_X 150
#define _WIND_CANVAS_Y 5

#define _POWER_USE_CANVAS_WIDTH 160
#define _POWER_USE_CANVAS_HEIGHT 48
#define _POWER_USE_CANVAS_X SCREEN_WIDTH - _POWER_USE_CANVAS_WIDTH + 5
#define _POWER_USE_CANVAS_Y 5

#define _HOUSE_MESSAGE_CANVAS_WIDTH 400
#define _HOUSE_MESSAGE_CANVAS_HEIGHT 48
#define _HOUSE_MESSAGE_CANVAS_X 15
#define _HOUSE_MESSAGE_CANVAS_Y 180

#define _FLAMETHROWER_CANVAS_WIDTH 400
#define _FLAMETHROWER_CANVAS_HEIGHT 48
#define _FLAMETHROWER_CANVAS_X 15
#define _FLAMETHROWER_CANVAS_Y 125


// Screen pins
#define TFT_CS 33
#define TFT_DC 38
#define TFT_RST 1

// Colors
#define BACKGROUND_COLOR HX8357_BLACK
#define CLOCK_COLOR HX8357_MAGENTA
#define TEMPERATURE_COLOR HX8357_WHITE
#define WIND_COLOR HX8357_BLUE
#define POWER_USED_COLOR HX8357_RED
#define HOUSE_MESSAGE_COLOR HX8357_CYAN
#define FLAMETHROWER_COLOR HX8357_YELLOW

namespace creatures
{

    class TouchDisplay
    {

    public:
        TouchDisplay();
        void showError(const char *errorMessage);
        void showSystemMessage(const char *systemMessage);
        void printTime(char *clockDisplay);
        void printTemperature(float temperature);
        void printWindspeed(float speed);
        void printPowerUsed(float powerUsed);
        void printHouseMessage(char *message);
        void printFlamethrowerMessage(char *message);

        void initScreen();
        unsigned long wipeScreen();

    private:
        void powerUpDisplay();
        Adafruit_HX8357 *display;
        GFXcanvas1 *errorCanvas;
        GFXcanvas1 *systemMessageCanvas;
        GFXcanvas1 *clockCanvas;
        GFXcanvas1 *temperatureCanvas;
        GFXcanvas1 *windCanvas;
        GFXcanvas1 *powerUseCanvas;
        GFXcanvas1 *houseMessageCanvas;
        GFXcanvas1 *flamethrowerCanvas;
    };
}

#endif