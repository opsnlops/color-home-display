
#pragma once

#include <WiFi.h>


#define MQTT_ON "on"
#define MQTT_OFF "off"

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
}

#include <AsyncMqttClient.h>

#define LCD_WIDTH 30
#define DISPLAY_QUEUE_LENGTH 5


enum MessageType {
  clock_display_message,
  flamethrower_message,
  home_event_message,
  temperature_message
};


// One message for the display
struct DisplayMessage
{
  MessageType type;
  char text[LCD_WIDTH + 1];
} __attribute__((packed));


char *ultoa(unsigned long val, char *s);


void updateHouseStatus();
void print_flamethrower(const char *room, boolean on);

void print_temperature(const char *room, const char *temperature);

void handle_mqtt_message(char *topic, char *payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);

void printFlamethrowerMessage(char* message);


portTASK_FUNCTION_PROTO(printLocalTimeTask, pvParameters);
portTASK_FUNCTION_PROTO(messageQueueReaderTask, pvParameters);
portTASK_FUNCTION_PROTO(updateDisplayTask, pvParameters);
