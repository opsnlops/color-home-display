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

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1325.h>

#include "creature.h"

#include "logging/logging.h"
#include "network/connection.h"
#include "creatures/creatures.h"
#include "mqtt/mqtt.h"
#include "time/time.h"
#include "mdns/creature-mdns.h"
#include "mdns/magicbroker.h"

#include "ota.h"

using namespace creatures;

TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

// Queue for updates to the display
QueueHandle_t displayQueue;

TaskHandle_t displayUpdateTaskHandler;
TaskHandle_t localTimeTaskHandler;

uint8_t startup_counter = 0;

#define OLED_CS 5
#define OLED_RESET 15
#define OLED_DC 13
Adafruit_SSD1325 display(OLED_DC, OLED_RESET, OLED_CS);

// I'll most likely need to figure out a better way to do this, but this will work for now
char clock_display[LCD_WIDTH];
char sl_concurrency[LCD_WIDTH];
char home_message[LCD_WIDTH];
char temperature[LCD_WIDTH];

// Keep a link to our logger
static Logger l;
static MQTT mqtt = MQTT(String(CREATURE_NAME));

// Clear the entire LCD and print a message
void paint_lcd(String top_line, String bottom_line)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(top_line);
    display.print(bottom_line);
    display.display();
}

void __show_big_message(String header, String line1, String line2)
{
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(2);
    display.println(header);
    display.setTextSize(1);
    display.println("");
    display.println(line1);
    display.println(line2);
    display.display();
}

// Cleanly show an error message
void show_error(String line1, String line2)
{
    __show_big_message("Oh no! :(", line1, line2);
}

// Cleanly show an error message
void show_startup(String line1)
{
    char buffer[3] = {'\0', '\0', '\0'};
    itoa(startup_counter++, buffer, 10);
    __show_big_message("Booting...", buffer, line1);
}

void WiFiEvent(WiFiEvent_t event)
{
    log_v("[WiFi-event] event: %d\n", event);
    switch (event)
    {
    case SYSTEM_EVENT_WIFI_READY:
        l.debug("wifi ready");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        l.info("WiFi connected");
        l.info("IP address: %s", WiFi.localIP().toString().c_str());
        show_startup(WiFi.localIP().toString());
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        l.warning("WiFi lost connection");
        show_error("Unable to connect to Wifi network", ":(");
        xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        xTimerStart(wifiReconnectTimer, 0);
        // onWifiDisconnect(); // Tell the broker we lost Wifi
        break;
    }
}

void set_up_lcd()
{
    l.info("setting up the OLED display");
    display.begin();
    display.display();
    delay(1000);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    paint_lcd(CREATURE_NAME, "Starting up...");

    // Initialize our display vars
    memset(clock_display, '\0', LCD_WIDTH);
    memset(sl_concurrency, '\0', LCD_WIDTH);
    memset(home_message, '\0', LCD_WIDTH);
    memset(temperature, '\0', LCD_WIDTH);

    delay(1000);
}

void setup()
{
    // Fire up the logging framework first
    l.init();

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    l.info("--- STARTED UP ---");

    // Get the display set up
    set_up_lcd();
    show_startup("display set up");

    // Create the message queue
    displayQueue = xQueueCreate(DISPLAY_QUEUE_LENGTH, sizeof(struct DisplayMessage));
    show_startup("queue made");

    show_startup("starting up Wifi");
    NetworkConnection network = NetworkConnection();
    network.connectToWiFi();

    // Register ourselves in mDNS
    CreatureMDNS creatureMDNS = CreatureMDNS(CREATURE_NAME, CREATURE_POWER);
    creatureMDNS.registerService(666);
    creatureMDNS.addStandardTags();

    Time time = Time();
    time.init();
    time.obtainTime();

    // Get the location of the magic broker
    MagicBroker magicBroker;
    magicBroker.find();

    // Connect to MQTT
    mqtt.connect(magicBroker.ipAddress, magicBroker.port);
    mqtt.subscribe(String("cmd"), 0);

    mqtt.subscribeGlobalNamespace(OUTSIDE_TEMPERATURE_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(OUTSIDE_WIND_SPEED_TOPIC, 0);
    mqtt.subscribeGlobalNamespace(FAMILY_ROOM_TEMPERATURE_TOPIC, 0);

    // mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
    // wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWiFi));
    // l.debug("created the timers");
    // show_startup("timers made");

     /*
    xTaskCreate(updateDisplayTask,
                "updateDisplayTask",
                10240,
                NULL,
                2,
                &displayUpdateTaskHandler);
   
    xTaskCreate(printLocalTimeTask,
                "printLocalTimeTask",
                2048,
                NULL,
                1,
                &localTimeTaskHandler);

    show_startup("timers running");
     */

    // Enable OTA
    setup_ota(String(CREATURE_NAME));
    start_ota();

    // Tell MQTT we're alive
    mqtt.publish(String("status"), String("I'm alive!!"), 0, false);
    mqtt.startHeartbeat();

    digitalWrite(LED_BUILTIN, LOW);

    // Start the task to read the queue
    l.debug("starting the message reader task");
    TaskHandle_t messageReaderTaskHandle;
    xTaskCreatePinnedToCore(messageQueueReaderTask,
                            "messageQueueReaderTask",
                            10240,
                            NULL,
                            1,
                            &messageReaderTaskHandle,
                            1);
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
    if (strncmp(SL_CONNCURRENCY_TOPIC, topic, topic_length) == 0)
    {
        unsigned long concurrency = atol(message);
        char buffer[14];
        char *number_string = ultoa(concurrency, buffer);

        struct DisplayMessage update_message;
        update_message.type = sl_concurrency_message;
        memcpy(&update_message.text, number_string, sizeof(buffer));
        xQueueSendToBackFromISR(displayQueue, &update_message, NULL);

#ifdef CREATURE_DEBUG
        Serial.print("processed SL concurrency: ");
        Serial.println(ultoa(concurrency, buffer));
#endif
    }
    else if (strncmp(BATHROOM_MOTION_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp(MQTT_ON, message, 2) == 0)
        {
            show_home_message("Bathroom Motion");
        }
        else
        {
            show_home_message("Bathroom Cleared");
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
    else if (strncmp(LAUNDRY_ROOM_MOTION_TOPIC, topic, topic_length) == 0)
    {
        if (strncmp(MQTT_ON, message, 2) == 0)
        {
            show_home_message("Laundry Room Motion");
        }
        else
        {
            show_home_message("Laundry Room Cleared");
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
    else if (strncmp(BATHROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Bathroom", message);
    }
    else if (strncmp(BEDROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Bedroom", message);
    }
    else if (strncmp(OFFICE_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Office", message);
    }
    else if (strncmp(LAUNDRY_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Laundry", message);
    }
    else if (strncmp(FAMILY_ROOM_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Family Room", message);
    }
    else if (strncmp(WORKSHOP_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Workshop", message);
    }

    else if (strncmp(OUTSIDE_TEMPERATURE_TOPIC, topic, topic_length) == 0)
    {
        print_temperature("Outside", message);
    }

    else
    {
        l.warning("Unknown message! topic: %s, message %s", topic, message);
        show_home_message(topic);
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

                switch (message.type)
                {
                case sl_concurrency_message:
                    memcpy(sl_concurrency, message.text, LCD_WIDTH);
                    break;
                case home_event_message:
                    memcpy(home_message, message.text, LCD_WIDTH);
                    break;
                case clock_display_message:
                    memcpy(clock_display, message.text, LCD_WIDTH);
                    break;
                case temperature_message:
                    memcpy(temperature, message.text, LCD_WIDTH);
                }

                // The display is buffered, so this just means wipe out what's there
                display.clearDisplay();
                display.setCursor(0, 0);
                display.setTextSize(2);
                display.println(sl_concurrency);
                display.setTextSize(1);
                display.println("");
                display.println(home_message);
                display.println(temperature);
                display.println("");
                display.println("");
                display.print("          ");
                display.println(clock_display);
                display.display();

#ifdef CREATURE_DEBUG

                l.debug("Read message from queue: type: %s, text: %s, size: %d", message.type, message.text, sizeof(message));
#else
                l.debug("message read from queue: %s", message.text);
#endif
            }
        }
        vTaskDelay(TickType_t pdMS_TO_TICKS(100));
    }
}

// Create a task to add the local time to the queue to print
void printLocalTimeTask(void *pvParameters)
{
    struct tm timeinfo;

    for (;;)
    {
        if (getLocalTime(&timeinfo))
        {

            struct DisplayMessage message;
            message.type = clock_display_message;

            strftime(message.text, LCD_WIDTH, "%I:%M:%S %p", &timeinfo);

#ifdef CREATURE_DEBUG
            Serial.print("Current time: ");
            Serial.println(message.text);
#endif

            // Drop this message into the queue, giving up after 500ms if there's no
            // space in the queue
            xQueueSendToBack(displayQueue, &message, pdMS_TO_TICKS(500));
        }
        else
        {
            l.warning("Failed to obtain time");
        }

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
            display_message(message.topic, message.payload);
        }
    }
}

void loop()
{
    vTaskDelete(NULL);
}
