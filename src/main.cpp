// This isn't gonna work on anything but an ESP32
#if !defined(ESP32)
#error This code is intended to run only on the ESP32 board
#endif

#include <Arduino.h>
#include <Wire.h>

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
}

#include "main.h"
#include "config.h"

#include "creature.h"

#include "logging/logging.h"
#include "network/connection.h"
#include "creatures/creatures.h"
#include "mqtt/mqtt.h"
#include "time/time.h"
#include "mdns/creature-mdns.h"
#include "mdns/magicbroker.h"
#include "home/data-feed.h"

#include "ota.h"
#include "screen.h"

using namespace creatures;

TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// Queue for updates to the display
QueueHandle_t displayQueue;

TaskHandle_t displayUpdateTaskHandler;
TaskHandle_t localTimeTaskHandler;

uint8_t startup_counter = 0;

// Display buffers

/*
    Configuration!

    This isn't saved anywhere on the MCU itself. All of the config is kept in a retained
    MQTT topic, which is read when the Creature boots.

    This is why where's "local" and "global" topics in MQTT. The config is local and is
    keyed to CREATURE_NAME.

*/
boolean gDisplayOn = true;

// Keep a link to our logger
static Logger l;
static MQTT mqtt = MQTT(String(CREATURE_NAME));
TouchDisplay display;

void setup()
{
    // Fire up the logging framework first
    l.init();
    l.debug("hi, logger!");
    display = TouchDisplay();

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    // Fire up the screen
    l.debug("firing up the display...");
    display.initScreen();
    display.showSystemMessage("display set up");

    l.info("Helllllo! I'm up and running on a %s!", ARDUINO_VARIANT);

    display.showSystemMessage("starting up Wifi");
    NetworkConnection network = NetworkConnection();
    if (!network.connectToWiFi())
    {
        display.showError("I can't WiFi :(\nCheck provisioning!");
        while (1)
            ;
    }

    // Create the message queue
    displayQueue = xQueueCreate(DISPLAY_QUEUE_LENGTH, sizeof(struct DisplayMessage));
    l.debug("displayQueue made");

    // Register ourselves in mDNS
    display.showSystemMessage("Starting in mDNS");
    CreatureMDNS creatureMDNS = CreatureMDNS(CREATURE_NAME, CREATURE_POWER);
    creatureMDNS.registerService(666);
    creatureMDNS.addStandardTags();

    // Set the time
    display.showSystemMessage("Setting time");
    Time time = Time();
    time.init();
    time.obtainTime();

    // Get the location of the magic broker
    display.showSystemMessage("Finding the broker");
    MagicBroker magicBroker;
    magicBroker.find();

    // Connect to MQTT
    display.showSystemMessage("Starting MQTT");
    mqtt.connect(magicBroker.ipAddress, magicBroker.port);
    mqtt.subscribe(String("cmd"), 0);
    mqtt.subscribe(String("config"), 0);

    mqtt.subscribeGlobalNamespace(OUTSIDE_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(OUTSIDE_WIND_SPEED_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(HOME_POWER_USE_WATTS, 0);

    mqtt.subscribeGlobalNamespace(BUNNYS_ROOM_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(FAMILY_ROOM_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(GUEST_ROOM_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(KITCHEN_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(OFFICE_TEMPERATURE_TOPIC, 0);

    mqtt.subscribeGlobalNamespace(FAMILY_ROOM_FLAMETHROWER_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(OFFICE_FLAMETHROWER_TOPIC, 0);

    // mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
    // wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWiFi));
    // l.debug("created the timers");
    // show_startup("timers made");

    xTaskCreate(updateDisplayTask,
                "updateDisplayTask",
                10240,
                NULL,
                2,
                &displayUpdateTaskHandler);

    xTaskCreate(printLocalTimeTask,
                "printLocalTimeTask",
                4096,
                NULL,
                1,
                &localTimeTaskHandler);

    // Enable OTA
    // setup_ota(String(CREATURE_NAME));
    // start_ota();

    // Tell MQTT we're alive
    mqtt.publish(String("status"), String("I'm alive!!"), 0, false);
    mqtt.startHeartbeat();

    digitalWrite(LED_BUILTIN, LOW);
    display.wipeScreen();

    // Start the task to read the queue
    l.debug("starting the message reader task");
    TaskHandle_t messageReaderTaskHandle;
    xTaskCreate(messageQueueReaderTask,
                "messageQueueReaderTask",
                10240,
                NULL,
                1,
                &messageReaderTaskHandle);
}

// Stolen from StackOverflow
char *ultoa(unsigned long val, char *s)
{
    char *p = s + 13;
    *p = '\0';
    do
    {
        if ((p - s) % 4 == 2)
            *--p = ',';
        *--p = '0' + val % 10;
        val /= 10;
    } while (val);
    return p;
}

// Print the temperature we just got from an event. Note that there's
// a bug here if the length of the string is too big, it'll crash.
void print_temperature(const char *room, const char *temperature)
{
    l.debug("printing temperature, room: %s", room);
    char buffer[LCD_WIDTH + 1];

    struct DisplayMessage message;
    message.type = temperature_message;

    memset(buffer, '\0', LCD_WIDTH + 1);
    sprintf(message.text, "%s: %sF", room, temperature);

    xQueueSendToBackFromISR(displayQueue, &message, NULL);
}

void print_flamethrower(const char *room, boolean on)
{
    l.debug("printing flamethrower, room: %s", room);
    char buffer[LCD_WIDTH + 1];

    char *action = "Off";
    if (on)
        action = "On";

    struct DisplayMessage message;
    message.type = flamethrower_message;

    memset(buffer, '\0', LCD_WIDTH + 1);
    sprintf(message.text, "%s %s", room, action);

    xQueueSendToBackFromISR(displayQueue, &message, NULL);
}

void show_home_message(const char *message)
{
    struct DisplayMessage home_message;
    home_message.type = home_event_message;

    memset(home_message.text, '\0', LCD_WIDTH + 1);
    memcpy(home_message.text, message, LCD_WIDTH);

    xQueueSendToBackFromISR(displayQueue, &home_message, NULL);
}

// Process a tricky event path to know how to update the display... this
// should be cleaned up when I have the time!
void display_message(const char *topic, const char *message)
{
    int topic_length = strlen(topic);
    if (strncmp(HALLWAY_BATHROOM_MOTION_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp(MQTT_ON, message, 2) == 0)
        {
            show_home_message("Hallway Bathroom Motion");
        }
        else
        {
            show_home_message("Hallway Bathroom Cleared");
        }
    }
    else if (strncmp(BEDROOM_MOTION_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp(MQTT_ON, message, 2) == 0)
        {
            show_home_message("Bedroom Motion");
        }
        else
        {
            show_home_message("Bedroom Cleared");
        }
    }
    else if (strncmp(OFFICE_MOTION_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp(MQTT_ON, message, 2) == 0)
        {
            show_home_message("Office Motion");
        }
        else
        {
            show_home_message("Office Cleared");
        }
    }
    else if (strncmp(LIVING_ROOM_MOTION_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp(MQTT_ON, message, 2) == 0)
        {
            show_home_message("Living Room Motion");
        }
        else
        {
            show_home_message("Living Room Cleared");
        }
    }
    else if (strncmp(WORKSHOP_MOTION_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp(MQTT_ON, message, 2) == 0)
        {
            show_home_message("Workshop Motion");
        }
        else
        {
            show_home_message("Workshop Cleared");
        }
    }
    else if (strncmp(HALF_BATHROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Half Bathroom", message);
    }
    else if (strncmp(BUNNYS_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Bunny's Room", message);
    }
    else if (strncmp(OFFICE_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Office", message);
    }
    else if (strncmp(FAMILY_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Family Room", message);
    }
    else if (strncmp(WORKSHOP_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Workshop", message);
    }
    else if (strncmp(GUEST_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Guest Room", message);
    }
    else if (strncmp(KITCHEN_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Kitchen", message);
    }

    else if (strncmp(OUTSIDE_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        display.printTemperature(atof(message));
    }

    else if (strncmp(OUTSIDE_WIND_SPEED_TOPIC, topic, topic_length) == 0)
    {
        display.printWindspeed(atof(message));
    }

    else if (strncmp(HOME_POWER_USE_WATTS, topic, topic_length) == 0)
    {
        display.printPowerUsed(atof(message));
    }

    else if (strncmp(FAMILY_ROOM_FLAMETHROWER_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp("false", message, strlen(message)) == 0)
        {
            print_flamethrower("Family Room", false);
        }
        else
        {
            print_flamethrower("Family Room", true);
        }
    }
    else if (strncmp(OFFICE_FLAMETHROWER_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp("false", message, strlen(message)) == 0)
        {
            print_flamethrower("Office", false);
        }
        else
        {
            print_flamethrower("Office", true);
        }
    }
    else
    {
        l.warning("Unknown message! topic: %s, message %s", topic, message);
        // show_home_message(topic);
    }
}

portTASK_FUNCTION(updateDisplayTask, pvParameters)
{

    struct DisplayMessage message;

    for (;;)
    {
        if (displayQueue != NULL)
        {
            if (xQueueReceive(displayQueue, &message, (TickType_t)10) == pdPASS)
            {

                // TODO: Handle gDisplay being off

                if (message.text != NULL)
                {
                    l.verbose("got a message: %s", message.text);
                    switch (message.type)
                    {
                    case home_event_message:
                        display.printHouseMessage(message.text);
                        break;
                    case flamethrower_message:
                        display.printFlamethrowerMessage(message.text);
                        break;
                    case clock_display_message:
                        display.printTime(message.text);
                        break;
                    case temperature_message:
                        display.printHouseMessage(message.text);
                        break;
                    }
                }
                else
                {
                    l.warning("NULL message pulled from the displayQueue");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Create a task to add the local time to the queue to print
portTASK_FUNCTION(printLocalTimeTask, pvParameters)
{
    Time time = Time();

    for (;;)
    {
        l.verbose("tick");

        String currentTime = time.getCurrentTime("%I:%M:%S %p");

        struct DisplayMessage message;
        message.type = clock_display_message;
        memcpy(message.text, currentTime.c_str(), currentTime.length());

        l.verbose("Current time: %s", message.text);

        // Drop this message into the queue, giving up after 500ms if there's no
        // space in the queue
        xQueueSendToBack(displayQueue, &message, pdMS_TO_TICKS(500));

        // Wait before repeating
        vTaskDelay(TickType_t pdMS_TO_TICKS(1000));
    }
}

// Read from the queue and print it to the screen for now
portTASK_FUNCTION(messageQueueReaderTask, pvParameters)
{

    QueueHandle_t incomingQueue = mqtt.getIncomingMessageQueue();
    for (;;)
    {
        struct MqttMessage message;
        if (xQueueReceive(incomingQueue, &message, (TickType_t)5000) == pdPASS)
        {
            l.debug("Incoming message! local topic: %s, global topic: %s, payload: %s",
                    message.topic,
                    message.topicGlobalNamespace,
                    message.payload);

            // Is this a config message?
            if (strncmp("config", message.topic, strlen(message.topic)) == 0)
            {
                l.info("Got a config message from MQTT: %s", message.payload);
                updateConfig(String(message.payload));
            }
            else
            {
                display_message(message.topic, message.payload);
            }
        }
    }
}

void loop()
{
    vTaskDelete(NULL);
}
