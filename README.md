# Wol Listener
A Esp8266/Esp32 project to make any system wol ready.

## Config
1. Create a header file `config.h` with content:
    ```
    #pragma once

    #define WLAN_SSID "YOUR_SSID"
    #define WLAN_PASSWORD "YOUR_PASSWORD"
    #define OUTPUT_PIN D1
    ```
1. In `ESP8266WiFiGeneric.cpp` change `WiFiEventHandler &handler = *it;` to `WiFiEventHandler handler = *it;`, to get wlan events to work.
