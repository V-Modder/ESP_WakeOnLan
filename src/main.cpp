//#define DEBUG_ESP_WIFI
//#define DEBUG_ESP_PORT Serial

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Ticker.h>

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
#define OUTPUT_PIN D4
#endif
#ifndef HOSTNAME
#define HOSTNAME ""
#endif

#define MAGIC_PACKET_SIZE 102
#define WOL_PORT 9

uint8_t esp_magic_packet[MAGIC_PACKET_SIZE];
WiFiUDP wifiUdp;

void set_button(bool enable)
{
  digitalWrite(OUTPUT_PIN, enable ? LOW : HIGH);
  digitalWrite(LED_BUILTIN, enable ? LOW : HIGH);
  Serial.println(enable ? "[Relay] Activating button" : "[Relay] Deactivating button");
}

void timer_callback()
{
  set_button(false);
}

void blink_callback()
{
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

Ticker timer(timer_callback, 2000, 1, MILLIS);
Ticker timer_blink(blink_callback, 2000, 0, MILLIS);

void on_wifi_connected(WiFiEventStationModeConnected wlan) {
  timer_blink.stop();
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.printf(PSTR("[WLAN] Connected to: %s [CH %02d] [%02X:%02X:%02X:%02X:%02X:%02X]\n"), wlan.ssid, wlan.channel, wlan.bssid[0], wlan.bssid[1], wlan.bssid[2], wlan.bssid[3], wlan.bssid[4], wlan.bssid[5]);
}

void on_wifi_got_ip(WiFiEventStationModeGotIP event) {
  Serial.print("[WLAN] Got ip: ");
  event.ip.printTo(Serial);
  Serial.println();
}

void connect_wifi()
{
  WiFi.onStationModeConnected(on_wifi_connected);
  WiFi.onStationModeGotIP(on_wifi_got_ip);
  WiFi.disconnect();
  WiFi.setHostname(HOSTNAME);
  WiFi.persistent(false);
  timer_blink.start();
  Serial.printf(PSTR("[WLAN] Connecting to Wlan: %s\n"), WLAN_SSID);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WLAN_SSID, WLAN_PASSWORD);
}

void read_mac()
{
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  Serial.print("[WLAN] Mac-address: ");
  Serial.println(WiFi.macAddress());

  for (int i = 0; i < 6; i++)
  {
    esp_magic_packet[i] = 0xFF;
  }
  for (int i = 0; i < 16; i++)
  {
    memcpy(esp_magic_packet + 6 + (i * WL_MAC_ADDR_LENGTH), mac, WL_MAC_ADDR_LENGTH);
  }
}

void print_available_wlans()
{
  int scanResult = WiFi.scanNetworks();
  String ssid;
  int32_t rssi;
  uint8_t encryptionType;
  uint8_t *bssid;
  int32_t channel;
  bool hidden;
  Serial.print("[WLAN] Wlans found:");
  Serial.println(scanResult);
  Serial.println("[WLAN] Available Wlans:");
  for (int8_t i = 0; i < scanResult; i++)
  {
    WiFi.getNetworkInfo(i, ssid, encryptionType, rssi, bssid, channel, hidden);
    const bss_info *bssInfo = WiFi.getScanInfoByIndex(i);
    String phyMode;
    const char *wps = "";
    if (bssInfo)
    {
      phyMode.reserve(12);
      phyMode = F("802.11");
      String slash;
      if (bssInfo->phy_11b)
      {
        phyMode += 'b';
        slash = '/';
      }
      if (bssInfo->phy_11g)
      {
        phyMode += slash + 'g';
        slash = '/';
      }
      if (bssInfo->phy_11n)
      {
        phyMode += slash + 'n';
      }
      if (bssInfo->wps)
      {
        wps = PSTR("WPS");
      }
    }
    Serial.printf(PSTR("[WLAN]   %02d: [CH %02d] [%02X:%02X:%02X:%02X:%02X:%02X] %ddBm %c %c %-11s %3S %s\n"), i, channel, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], rssi, (encryptionType == ENC_TYPE_NONE) ? ' ' : '*', hidden ? 'H' : 'V', phyMode.c_str(), wps, ssid.c_str());
  }
}

void setup()
{
  Serial.begin(115200);
  while (!Serial || !Serial.availableForWrite());
  Serial.flush();
  Serial.println("\nStarting");
  pinMode(OUTPUT_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  set_button(false);
  
  wifiUdp.begin(WOL_PORT);
  read_mac();
  print_available_wlans();

  connect_wifi();
}

void activate_power_button()
{
  set_button(true);
  timer.start();
}

bool packet_matches(uint8_t *new_packet)
{
  for (int i = 0; i < MAGIC_PACKET_SIZE; i++)
  {
    if (new_packet[i] != esp_magic_packet[i])
    {
      return false;
    }
  }
  return true;
}

void process_packet()
{
  uint8_t new_packet[MAGIC_PACKET_SIZE];
  wifiUdp.read(new_packet, MAGIC_PACKET_SIZE);

  if (packet_matches(new_packet))
  {
    Serial.println("[Wol] received");
    activate_power_button();
  }
}

void loop()
{
  if (WiFi.isConnected() && wifiUdp.parsePacket() == MAGIC_PACKET_SIZE)
  {
    process_packet();
  }
  timer.update();
  timer_blink.update();
}
