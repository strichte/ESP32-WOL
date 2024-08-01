# ESP Wake on Lan
ESP32-WOL Is a simple Wake On LAN server that will wake up pre-configured deives via WOL Magic Packets. 

It's intention is to wake up servers after an unfortunate power outage. You can end up in a race condition where devices power down but the UPS does not before mains power returns. In these situation the servers won't come back online, even with the correct power up BIOS settings, because they actually never lose power after the UPS manager shut them down. If the server are correctly configured for Wake On LAN packages, our little server will bring them back to live.

The ESP32-WOL device must be connected to a power source that is not UPS procted and thus it will turn off during a power outage. Once power returns, the device will wait a preconfigured initial period before sending WOL packets. This time should be long enough to charge the UPS to a safe level, as power might not be stable. It should be hours, rather than minutes.

After the initial period, the ESP32 will continue to send WOL packets at a preconfigured interval. Other than a bit of broadcast traffic, there is no harm sending a WOL packet to a running server.

In addition we proved a simple web interface to send Wake on Lan Magic Packets to the pre-configured devices.

## Requirements
 1. A system with a working [platform io](https://platformio.org/) installation.
 2. A [LilyGO ESP32 POE](https://www.lilygo.cc/products/t-internet-poe) with others.

## Installation
 * Create a file called `otapass.txt` containing the password for [Arduino Over The Air](https://www.arduino.cc/reference/en/libraries/arduinoota/) firmware updates.
 * Make sure these files don't end with an empty line.
 * Put the MAC addresses of the devices to wake up to a file called `devices.txt` in the project root.
 * Put one Name/Identifiere, followed by a |, followed by the MAC address in each line.
 * An example can be seen in `devices.example`.
 * Empty lines in this file are ignored.
 * Attach your ESP to your PC.
 * Run `pio run -t upload -e esp32dev` to compile this project and flash it to your ESP32 or `pio run -t upload -e esp_wroom_02` for ESP8266.
 * To later update it over WiFi run `pio ron -t upload -e esp32dev_ota` for ESP32 or `pio run -t upload -e esp_wroom_02_ota` for ESP8266.
