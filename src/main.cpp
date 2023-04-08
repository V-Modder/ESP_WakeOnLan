#include <Arduino.h>
#ifdef ESP8266
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif
#include <WiFiUdp.h>
#include <Ticker.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

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
#define OUTPUT_PIN D5
#endif
#ifndef HOSTNAME
#define HOSTNAME "NAS-WOL"
#endif
#ifndef OTA_USERNAME
#define OTA_USERNAME ""
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

#include "version.h"

#define MAGIC_PACKET_SIZE 102
#define WOL_PORT 9

uint8_t esp_magic_packet[MAGIC_PACKET_SIZE];
WiFiUDP wifiUdp;
AsyncWebServer server(80);

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

void on_wifi_connected(WiFiEventStationModeConnected wlan)
{
  Serial.printf(PSTR("[WLAN] Connected to: %s [CH %02d] [%02X:%02X:%02X:%02X:%02X:%02X]\n"), wlan.ssid.c_str(), wlan.channel, wlan.bssid[0], wlan.bssid[1], wlan.bssid[2], wlan.bssid[3], wlan.bssid[4], wlan.bssid[5]);
}

void on_wifi_got_ip(WiFiEventStationModeGotIP event)
{
  timer_blink.stop();
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.printf(PSTR("[WLAN] Got ip: ip=%s, mask=%s, gateway=%s\n"), event.ip.toString().c_str(), event.mask.toString().c_str(), event.gw.toString().c_str());
}

String get_auth(uint8 mode) {
  switch(mode) {
    case AUTH_OPEN: return "AUTH_OPEN";
    case AUTH_WEP: return "AUTH_WEP";
    case AUTH_WPA_PSK: return "AUTH_WPA_PSK";
    case AUTH_WPA2_PSK: return "AUTH_WPA2_PSK";
    case AUTH_WPA_WPA2_PSK: return "AUTH_WPA_WPA2_PSK";
    case AUTH_MAX: return "AUTH_MAX";
    default: return "";
  }
}

void on_auth_mode_changed(WiFiEventStationModeAuthModeChanged event) {
  Serial.printf(PSTR("[WLAN] Auth mode changed: oldmode=(%d)%s, newmode=(%d)%s\n"), event.oldMode, get_auth(event.oldMode).c_str(), event.newMode, get_auth(event.newMode).c_str());
}

void connect_wifi()
{
  WiFi.onStationModeConnected(on_wifi_connected);
  WiFi.onStationModeGotIP(on_wifi_got_ip);
  WiFi.onStationModeAuthModeChanged(on_auth_mode_changed);
  WiFi.disconnect();
  WiFi.hostname(HOSTNAME);
  Serial.print("[WLAN] Connecting to Wlan: ");
  Serial.println(WLAN_SSID);
  WiFi.begin(String(WLAN_SSID).c_str(), String(WLAN_PASSWORD).c_str());
}

void read_mac()
{
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.macAddress(mac);
  Serial.printf(PSTR("[WLAN] Mac-address: %s\n"), WiFi.macAddress().c_str());

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
  Serial.printf(PSTR("[WLAN] Wlans found: %d\n"), scanResult);
  if (scanResult > 0)
  {
    Serial.println("[WLAN] Available Wlans:");
    for (int8_t i = 0; i < scanResult; i++)
    {
      WiFi.getNetworkInfo(i, ssid, encryptionType, rssi, bssid, channel, hidden);
      Serial.printf(PSTR("[WLAN]   %02d: [CH %02d] [%02X:%02X:%02X:%02X:%02X:%02X] %ddBm %c %c %s\n"), i, channel, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], rssi, (encryptionType == ENC_TYPE_NONE) ? ' ' : '*', hidden ? 'H' : 'V', ssid.c_str());
    }
  }
}

String get_state() 
{
  int state = digitalRead(OUTPUT_PIN) == 0 ? 1 : 0;
  char json[15];
  sprintf(json, PSTR("{\"state\":%d}"), state);
  return String(json);
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

  read_mac();
  print_available_wlans();
  connect_wifi();
  
  wifiUdp.begin(WOL_PORT);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String("Hi! I am ESP8266. ") + VERSION_STR);
  });
  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", get_state());
  });

  AsyncElegantOTA.setID(VERSION_STR);
  AsyncElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);
  server.begin();
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
