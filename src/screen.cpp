
#include <Arduino.h>

#include "screen.h"

namespace creatures
{

    // Define the graphics objects
    static GFXcanvas1 clockCanvas(_CLOCK_CANVAS_WIDTH, _CLOCK_CANVAS_HEIGHT);

    static Logger l = Logger();

    void initScreen()
    {

        powerUpDisplay();
        display.begin();

        l.debug("Screen POST: ");
        uint8_t x = display.readcommand8(HX8357_RDPOWMODE);
        l.debug("  Display Power Mode: %#04x", x);
        x = display.readcommand8(HX8357_RDMADCTL);
        l.debug("  MADCTL Mode: %#04x", x);
        x = display.readcommand8(HX8357_RDCOLMOD);
        l.debug("  Pixel Format: %#04x", x);
        x = display.readcommand8(HX8357_RDDIM);
        l.debug("  Image Format: %#04x", x);
        x = display.readcommand8(HX8357_RDDSDR);
        l.debug("  Self Diagnostic: %#04x", x);

        wipeScreen();

        l.debug("setting screen rotation");
        display.setRotation(1);

        l.info("set up the display!");
        
    }

    void powerUpDisplay()
    {
        // Turn on LDO2 for the display
        l.debug("powering up the display");
        pinMode(21, OUTPUT);
        digitalWrite(21, HIGH);

        l.debug("powered up, pausing for 1000ms");
       vTaskDelay(pdMS_TO_TICKS(1000));
    }

    unsigned long wipeScreen()
    {
        l.debug("wiping the screen");
        unsigned long start = micros();
        display.fillScreen(BACKGROUND_COLOR);
        return micros() - start;
    }

    void printTime(char *clock_display)
    {

        int canvasWidth = 212;
        int canvasHeight = 48;
        // The display is 320x480

        int x = 480 - canvasWidth;
        int y = 320 - canvasHeight;

        clockCanvas.setTextSize(1);
        clockCanvas.setFont(&FreeSans18pt7b);
        clockCanvas.setCursor(6, 35);

        clockCanvas.print(clock_display);
        display.drawBitmap(x, y, clockCanvas.getBuffer(), canvasWidth, canvasHeight, CLOCK_COLOR, BACKGROUND_COLOR);
    }
}