#include <Arduino.h>
#include <WiFiUdp.h>
#ifdef ESP32
#include <WiFi.h>
#elif ESP8266
#include <ESP8266WiFi.h>
#endif

#if __has_include("config.h")
#include "config.h"
#endif
#ifndef WLAN_SSID
#define WLAN_SSID ""
#endif
#ifndef WLAN_PASSWORD
#define WLAN_PASSWORD ""
#endif
#ifndef OUTPUT_PIN
#define OUTPUT_PIN 1
#endif

#define MAGIC_PACKET_SIZE 102
#define WOL_PORT 9

#define print_enum(value) #value

uint8_t esp_magic_packet[MAGIC_PACKET_SIZE];
int current_match_index;
WiFiUDP wifiUdp;
os_timer_t *timer0_cfg = NULL;

void connect_wifi()
{
  Serial.print("Connecting to Wlan:");
  WiFi.begin(WLAN_SSID, WLAN_PASSWORD);
  Serial.println(print_enum(WiFi.status()));
}

void set_button(bool enable)
{
  Serial.println(enable ? "Activating button" : "Deactivating button");
  digitalWrite(OUTPUT_PIN, enable ? HIGH : LOW);
}

void timer_callback(void *pArg)
{
  set_button(false);
}

void read_mac()
{
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  Serial.printf("Mac address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  for (int i = 0; i < 6; i++)
  {
    esp_magic_packet[i] = 0xFF;
  }
  for (int i = 0; i < 16; i++)
  {
    memcpy(esp_magic_packet + 6 + (i * WL_MAC_ADDR_LENGTH), mac, WL_MAC_ADDR_LENGTH);
  }
}

void setup()
{
  Serial.begin(9600);
  pinMode(OUTPUT_PIN, OUTPUT);
  set_button(false);
  wifiUdp.begin(WOL_PORT);
  current_match_index = 0;
  read_mac();

  os_timer_setfn(timer0_cfg, timer_callback, NULL);
}

void activate_power_button()
{
  set_button(true);
  os_timer_arm(timer0_cfg, 1000, false);
}

void process_byte(uint8_t byte)
{
  if (byte == esp_magic_packet[current_match_index])
  {
    current_match_index++;
    if (current_match_index == MAGIC_PACKET_SIZE)
    {
      Serial.println("Wol received");
      activate_power_button();
      current_match_index = 0;
    }
  }
  else if (byte == esp_magic_packet[0])
  {
    current_match_index = 1;
  }
  else
  {
    current_match_index = 0;
  }
}

void loop()
{
  if (!WiFi.isConnected())
  {
    connect_wifi();
  }

  if (WiFi.isConnected() && wifiUdp.available())
  {
    process_byte(wifiUdp.read());
  }
}
