/*
 * NetworkHandler.h
 *
 * Created on: 23.11.2021
 *
 * Copyright (C) 2021 ToMe25.
 * This project is licensed under the MIT License.
 * The MIT license can be found in the project root and at https://opensource.org/licenses/MIT.
 */

#ifndef SRC_NETWORKHANDLER_H_
#define SRC_NETWORKHANDLER_H_

#include <Arduino.h>
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3,0,0)
#include <ETHClass2.h>       //Is to use the modified ETHClass
#define ETH  ETH2
#else
#include <ETH.h>
#endif
#include <SPI.h>
#include <SD.h>
#include <utilities.h>
#include <WiFi.h>
#include <ctime>
#include <string>
#include <ESPAsyncWebServer.h>
#include <AsyncUDP.h>

#define DISPLAY_INTERVAL 1 // time between display updates in sec
#define WOL_STARTUP 60 // minutes after startup until first WOL broadcast
#define WOL_INTERVAL 10 // minutes between WOL broadcasts after initial startup broadcast

//Change to IP and DNS corresponding to your network, gateway
static const IPAddress STATIC_IP(192, 168, 100, 12);
static const IPAddress GATEWAY(192, 168, 100, 1);
static const IPAddress SUBNET(255, 255, 255, 0);
static const IPAddress DNS(192, 168, 100, 1);
static const char HOSTNAME[] = "wol.zs.home";
static const char NTP1[] = "192.168.100.1";
static const char NTP2[] = "pool.ntp.org";
static const std::string timeZone("NZST-12NZDT,M9.5.0,M4.1.0/3"); // Pacific/Auckland time zone

// Includes the content of the file "otapass.txt" in the project root.
// Make sure this file doesn't end with an empty line.
extern const char OTA_PASS[] asm("_binary_otapass_txt_start");

// Includes the content of the file "devices.txt" in the project root.
// Each line in that file should contain one mac address.
extern const char DEVICE_MACS[] asm("_binary_devices_txt_start");

extern const char INDEX_HTML[] asm("_binary_src_html_index_html_start");
extern const char INDEX_JS[] asm("_binary_src_html_index_js_start");
extern const char MAIN_CSS[] asm("_binary_src_html_main_css_start");
extern const char NOT_FOUND_HTML[] asm("_binary_src_html_not_found_html_start");

// Web Server constants
static const uint16_t WEBSERVER_PORT = 80;
static const IPAddress DEFAULT_TARGET_BROADCAST = (uint) 0;// set to 0 to use the local network broadcast ip instead.

// Wake on Lan constants
static const uint16_t TARGET_PORT = 9;

// Device information
struct wolDevice {
	std::string mac;
	std::string name;
	wolDevice(std::string m, std::string n){
		mac = m;
		name = n;
	}
	wolDevice(String m, std::string n){
		mac = m.c_str();
		name = n;
	}
};

bool operator< (const wolDevice &left, const wolDevice &right);
bool operator== (const wolDevice &left, const wolDevice &right);

enum dateTimeType {
	all,
	date_only,
	time_only
};

class NetworkHandler {
public:
	static void setup();
	static std::string getTime(dateTimeType t=all, tm *ti = nullptr);
	static std::string getUptime(dateTimeType t=all);
	static void setNextWOLTime(tm *ti);
	static std::string getNextWOLTime(dateTimeType t=all);
	static void sendWOL();
	static void loop();

private:

	// ETH variables
	static bool ethConnected;

	// NTP 
	static bool ntpConnected;

	// Next WOL Time
	static struct tm nextWOLTime;

	// Web Server variables
	static AsyncWebServer webServer;
	static std::vector<wolDevice> wolDevices;
	static IPAddress targetBroadcast;
	static std::string bootTime;
	// Wake on Lan variables
	static AsyncUDP udp;

	static void setTimezone(std::string timezone);
	static void onETHEvent(WiFiEvent_t event);
	static void setupETH();
	static void setupNTP();
	static void setupWOLTargets();
	static void setupWebServer();
	static void setupOTA();
	static std::string getUptimeRel(dateTimeType=all);
	
	static void onIndexGet(AsyncWebServerRequest *request);
	static void onIndexPost(AsyncWebServerRequest *request);

	/**
	 * Replaces some variables in indexHtml to prepare it for being sent to the client.
	 *
	 * @param device	The currently selected device MAC address.
	 * @param target	The currently selected target broadcast ip address.
	 * @return	The prepared index.html string.
	 */
	static std::string prepareIndexResponse(const String device, const String target);
};

#endif /* SRC_NETWORKHANDLER_H_ */
