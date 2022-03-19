
#include <Arduino.h>

#include "screen.h"

namespace creatures
{

    static Logger l = Logger();

    TouchDisplay::TouchDisplay()
    {
        l.verbose("hi!");
    }

    void TouchDisplay::initScreen()
    {
        display = new Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

        powerUpDisplay();
        display->begin();

        l.debug("Screen POST: ");
        uint8_t x = display->readcommand8(HX8357_RDPOWMODE);
        l.debug("  Display Power Mode: %#04x", x);
        x = display->readcommand8(HX8357_RDMADCTL);
        l.debug("  MADCTL Mode: %#04x", x);
        x = display->readcommand8(HX8357_RDCOLMOD);
        l.debug("  Pixel Format: %#04x", x);
        x = display->readcommand8(HX8357_RDDIM);
        l.debug("  Image Format: %#04x", x);
        x = display->readcommand8(HX8357_RDDSDR);
        l.debug("  Self Diagnostic: %#04x", x);

        wipeScreen();

        l.debug("setting screen rotation");
        display->setRotation(1);

        // Create the canvases
        errorCanvas = new GFXcanvas1(_ERROR_CANVAS_WIDTH, _ERROR_CANVAS_HEIGHT);
        systemMessageCanvas = new GFXcanvas1(_SYSTEM_MESSAGE_CANVAS_WIDTH, _SYSTEM_MESSAGE_CANVAS_HEIGHT);
        clockCanvas = new GFXcanvas1(_CLOCK_CANVAS_WIDTH, _CLOCK_CANVAS_HEIGHT);
        temperatureCanvas = new GFXcanvas1(_TEMPERATURE_CANVAS_WIDTH, _TEMPERATURE_CANVAS_HEIGHT);
        windCanvas = new GFXcanvas1(_WIND_CANVAS_WIDTH, _WIND_CANVAS_HEIGHT);
        powerUseCanvas = new GFXcanvas1(_POWER_USE_CANVAS_WIDTH, _POWER_USE_CANVAS_HEIGHT);
        houseMessageCanvas = new GFXcanvas1(_HOUSE_MESSAGE_CANVAS_WIDTH, _HOUSE_MESSAGE_CANVAS_HEIGHT);
        flamethrowerCanvas = new GFXcanvas1(_FLAMETHROWER_CANVAS_WIDTH, _FLAMETHROWER_CANVAS_HEIGHT);

        l.info("set up the display!");
    }

    void TouchDisplay::powerUpDisplay()
    {
        // Turn on LDO2 for the display
        l.debug("powering up the display");
        pinMode(21, OUTPUT);
        digitalWrite(21, HIGH);

        l.debug("powered up, pausing for 1000ms");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    unsigned long TouchDisplay::wipeScreen()
    {
        l.debug("wiping the screen");
        unsigned long start = micros();
        display->fillScreen(BACKGROUND_COLOR);
        return micros() - start;
    }

    void TouchDisplay::showError(const char *errorMessage)
    {
        l.verbose("showing error message: %s", errorMessage);
        errorCanvas->fillScreen(BACKGROUND_COLOR);
        errorCanvas->setTextSize(1);
        errorCanvas->setFont(&FreeSans18pt7b);
        errorCanvas->setCursor(6, 35);

        errorCanvas->print(errorMessage);
        display->drawBitmap(_ERROR_CANVAS_X,
                            _ERROR_CANVAS_Y,
                            errorCanvas->getBuffer(),
                            _ERROR_CANVAS_WIDTH,
                            _ERROR_CANVAS_HEIGHT,
                            HX8357_RED,
                            BACKGROUND_COLOR);
    }

    void TouchDisplay::showSystemMessage(const char *systemMessage)
    {
        l.verbose("showing system message: %s", systemMessage);
        systemMessageCanvas->setTextSize(1);
        systemMessageCanvas->setFont(&FreeSans18pt7b);
        systemMessageCanvas->setCursor(6, 35);

        systemMessageCanvas->print(systemMessage);
        display->drawBitmap(_SYSTEM_MESSAGE_CANVAS_X,
                            _SYSTEM_MESSAGE_CANVAS_Y,
                            systemMessageCanvas->getBuffer(),
                            _SYSTEM_MESSAGE_CANVAS_WIDTH,
                            _SYSTEM_MESSAGE_CANVAS_HEIGHT,
                            HX8357_GREEN,
                            BACKGROUND_COLOR);

        // Reset for next time
        systemMessageCanvas->fillScreen(BACKGROUND_COLOR);
    }

    void TouchDisplay::printTime(char *clockDisplay)
    {
        l.debug("printing time: %s", clockDisplay);

        clockCanvas->setTextSize(1);
        clockCanvas->setFont(&FreeSans18pt7b);
        clockCanvas->setCursor(6, 35);

        clockCanvas->print(clockDisplay);
        display->drawBitmap(_CLOCK_CANVAS_X,
                            _CLOCK_CANVAS_Y,
                            clockCanvas->getBuffer(),
                            _CLOCK_CANVAS_WIDTH,
                            _CLOCK_CANVAS_HEIGHT,
                            CLOCK_COLOR,
                            BACKGROUND_COLOR);

        // Get ready for the next pass
        clockCanvas->fillScreen(BACKGROUND_COLOR);

        l.verbose("done printing time");
    }

    void TouchDisplay::printTemperature(float temperature)
    {

        l.debug("printing temperature: %.1f", temperature);

        temperatureCanvas->setTextSize(1);
        temperatureCanvas->setFont(&FreeSans18pt7b);
        temperatureCanvas->setCursor(6, 35);

        // Create a small buffer
        char temp[8];
        memset(temp, '\0', 8);
        sprintf(temp, "%.1fF", temperature);

        temperatureCanvas->print(temp);
        display->drawBitmap(_TEMPERATURE_CANVAS_X,
                            _TEMPERATURE_CANVAS_Y,
                            temperatureCanvas->getBuffer(),
                            _TEMPERATURE_CANVAS_WIDTH,
                            _TEMPERATURE_CANVAS_HEIGHT,
                            TEMPERATURE_COLOR,
                            BACKGROUND_COLOR);

        // Get ready for the next pass
        temperatureCanvas->fillScreen(BACKGROUND_COLOR);

        l.verbose("done printing temperature");
    }

    void TouchDisplay::printWindspeed(float speed)
    {
        l.debug("printing wind speed: %.1f", speed);

        windCanvas->setTextSize(1);
        windCanvas->setFont(&FreeSans18pt7b);
        windCanvas->setCursor(6, 35);

        // Create a small buffer
        char temp[9];
        memset(temp, '\0', 9);
        sprintf(temp, "%.1f MPH", speed);

        windCanvas->print(temp);
        display->drawBitmap(_WIND_CANVAS_X,
                            _WIND_CANVAS_Y,
                            windCanvas->getBuffer(),
                            _WIND_CANVAS_WIDTH,
                            _WIND_CANVAS_HEIGHT,
                            WIND_COLOR,
                            BACKGROUND_COLOR);

        // Get ready for the next pass
        windCanvas->fillScreen(BACKGROUND_COLOR);

        l.verbose("done printing wind speed");
    }

    void TouchDisplay::printPowerUsed(float powerUsed)
    {
        l.debug("printing power use: %.1f", powerUsed);

        powerUseCanvas->setTextSize(1);
        powerUseCanvas->setFont(&FreeSans18pt7b);
        powerUseCanvas->setCursor(6, 35);

        // Create a small buffer
        char temp[8];
        memset(temp, '\0', 8);
        sprintf(temp, "%.0fW", powerUsed);

        powerUseCanvas->print(temp);
        display->drawBitmap(_POWER_USE_CANVAS_X,
                            _POWER_USE_CANVAS_Y,
                            powerUseCanvas->getBuffer(),
                            _POWER_USE_CANVAS_WIDTH,
                            _POWER_USE_CANVAS_HEIGHT,
                            POWER_USED_COLOR,
                            BACKGROUND_COLOR);

        // Get ready for the next pass
        powerUseCanvas->fillScreen(BACKGROUND_COLOR);

        l.verbose("done printing power use");
    }

    void TouchDisplay::printHouseMessage(char *message)
    {
        l.debug("printlng a house message: %s", message);

        houseMessageCanvas->setTextSize(1);
        houseMessageCanvas->setFont(&FreeSans18pt7b);
        houseMessageCanvas->setCursor(6, 35);

        houseMessageCanvas->print(message);
        display->drawBitmap(_HOUSE_MESSAGE_CANVAS_X,
                            _HOUSE_MESSAGE_CANVAS_Y,
                            houseMessageCanvas->getBuffer(),
                            _HOUSE_MESSAGE_CANVAS_WIDTH,
                            _HOUSE_MESSAGE_CANVAS_HEIGHT,
                            HOUSE_MESSAGE_COLOR,
                            BACKGROUND_COLOR);

        // Get ready for the next pass
        houseMessageCanvas->fillScreen(BACKGROUND_COLOR);

        l.verbose("done printing house message");
    }

    void TouchDisplay::printFlamethrowerMessage(char *message)
    {
        l.debug("printlng a flamethwoer message: %s", message);

        flamethrowerCanvas->setTextSize(1);
        flamethrowerCanvas->setFont(&FreeSans18pt7b);
        flamethrowerCanvas->setCursor(6, 35);

        flamethrowerCanvas->print(message);
        display->drawBitmap(_FLAMETHROWER_CANVAS_X,
                            _FLAMETHROWER_CANVAS_Y,
                            flamethrowerCanvas->getBuffer(),
                            _FLAMETHROWER_CANVAS_WIDTH,
                            _FLAMETHROWER_CANVAS_HEIGHT,
                            FLAMETHROWER_COLOR,
                            BACKGROUND_COLOR);

        // Get ready for the next pass
        flamethrowerCanvas->fillScreen(BACKGROUND_COLOR);

        l.verbose("done printing house message");
    }
}