#include <Arduino.h>
#include <WiFiUdp.h>
#include <ESP8266WiFi.h>

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

uint8_t esp_mac[WL_MAC_ADDR_LENGTH];
WiFiUDP wifiUdp;
os_timer_t *timer0_cfg = NULL;

void connect_wifi()
{
  Serial.print("Connecting to Wlan:");
  WiFi.begin(WLAN_SSID, WLAN_PASSWORD);
  Serial.println(WiFi.status());
}

void setButton(bool enable)
{
  Serial.println(enable ? "Activating button" : "Deactivating button");
  digitalWrite(OUTPUT_PIN, enable ? HIGH : LOW);
}

void timerCallback(void *pArg)
{
  setButton(false);
}

void setup()
{
  pinMode(OUTPUT_PIN, OUTPUT);
  setButton(false);
  wifiUdp.begin(7);

  WiFi.macAddress(esp_mac);

  os_timer_setfn(timer0_cfg, timerCallback, NULL);
}

bool wol_received()
{
  uint8_t magic_packet[MAGIC_PACKET_SIZE];
  wifiUdp.readBytes(magic_packet, MAGIC_PACKET_SIZE);

  int i;
  for (i = 0; i < 6; i++)
  {
    if (magic_packet[i] != 0xFF)
    {
      return false;
    }
  }
  for (; i < MAGIC_PACKET_SIZE; i++)
  {
    if (magic_packet[i] != esp_mac[i - 6])
    {
      return false;
    }
  }

  Serial.println("Wol received");
  return true;
}

void activate_power_button()
{
  setButton(true);
  os_timer_arm(timer0_cfg, 1000, false);
}

bool wifi_connected_and_packets_available_and_wol_received() {
  return WiFi.isConnected() 
    && wifiUdp.available() >= MAGIC_PACKET_SIZE 
    && wol_received();
}

void loop()
{
  if (!WiFi.isConnected())
  {
    connect_wifi();
  }

  if (wifi_connected_and_packets_available_and_wol_received())
  {
    activate_power_button();
  }
}
