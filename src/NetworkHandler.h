/*
 * NetworkHandler.h
 *
 * Created on: 23.11.2021
 *
 * Copyright (C) 2021 ToMe25.
 * This project is licensed under the MIT License.
 * The MIT license can be found in the project root and at
 * https://opensource.org/licenses/MIT.
 */

#ifndef SRC_NETWORKHANDLER_H_
#define SRC_NETWORKHANDLER_H_

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <AsyncUDP.h>
#include <ESPAsyncWebServer.h>
#include <SPI.h>
#include <WiFi.h>
#include <utilities.h>

#include <ctime>
#include <string>

#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#include <ETHClass2.h>  //Is to use the modified ETHClass
#define ETH ETH2
#else
#include <ETH.h>
#endif

#define DISPLAY_INTERVAL 1  // time between display updates in sec
#define WOL_STARTUP 2       // minutes after startup until first WOL broadcast
#define WOL_INTERVAL \
  1  // minutes between WOL broadcasts after initial
     // startup broadcast

// Change to IP and DNS corresponding to your network, gateway
static const IPAddress kStaticIp(192, 168, 100, 12);
static const IPAddress kGateway(192, 168, 100, 1);
static const IPAddress kSubnet(255, 255, 255, 0);
static const IPAddress kDnsServer(192, 168, 100, 1);
static const char kHostname[] = "wol.zs.home";
static const char kNtpServer1[] = "192.168.100.1";
static const char kNtpServer2[] = "pool.ntp.org";
static const char kTimeZone[] =
    "NZST-12NZDT,M9.5.0,M4.1.0/3";  // Pacific/Auckland time zone

// Includes the content of the file "otapass.txt" in the project root.
// Make sure this file doesn't end with an empty line.
extern const char kOtaPasswordFile[] asm("_binary_otapass_txt_start");

// Includes the content of the file "devices.txt" in the project root.
// Each line in that file should contain one mac address.
extern const char kWolDevicesFile[] asm("_binary_devices_txt_start");

extern const char kIndexHtmlFile[] asm("_binary_src_html_index_html_start");
extern const char kIndexJsFile[] asm("_binary_src_html_index_js_start");
extern const char kCssFile[] asm("_binary_src_html_main_css_start");
extern const char kNotFoundHtmlFile[] asm(
    "_binary_src_html_not_found_html_start");

// Web Server constants
static const uint16_t kWebServerPort = 80;
static const IPAddress kDefaultBroadcastAddress =
    (uint)0;  // set to 0 to use the local network broadcast ip instead.

// Wake on Lan constants
static const uint16_t kWolTargetPort = 9;

// Device information
struct WolDevice {
  std::string mac;
  std::string name;
  WolDevice(const std::string &m, const std::string &n) : mac(m), name(n) {}
  WolDevice(const String &m, const std::string &n) : mac(m.c_str()), name(n) {}
};

bool operator<(const WolDevice &left, const WolDevice &right);
bool operator==(const WolDevice &left, const WolDevice &right);

enum DateTimeType { all, date_only, time_only };

class NetworkHandler {
 public:
  static void Setup();
  static void SetNtpStatus(const bool &n) { ntp_connected_ = n; };
  static std::string GetTime(DateTimeType t = all, tm *ti = nullptr);
  static std::string GetUptime(DateTimeType t = all);
  static void SetNextWolTime(const time_t &e) {NetworkHandler::next_wol_time_ = e;}
  static std::string GetNextWolTime(DateTimeType t = all);
  static void SendWol();
  static const std::vector<WolDevice>& GetWolDevices() { return wol_devices_; }
  static bool FirstWolSent() { return first_wol_sent_; }
  static void Loop() { ArduinoOTA.handle(); };
  static void CbSyncTime(timeval *tv);

 private:
  static bool first_wol_sent_;
  static bool eth_connected_;
  static bool ntp_connected_;
  static time_t next_wol_time_;

  static AsyncWebServer web_server_;
  static std::vector<WolDevice> wol_devices_;
  static IPAddress target_broadcast_;
  static std::string boot_time_;
  static AsyncUDP udp_;

  static void SetupEth();
  static void SetupNtp();
  static void OnEthEvent(WiFiEvent_t event);
  static void SetupWolTargets();
  static void SetupWebServer();
  static void SetupOta();
  static std::string GetRelativeUptime(const DateTimeType &type = all);
  static void OnIndexGet(AsyncWebServerRequest *request);
  static void OnIndexPost(AsyncWebServerRequest *request);

  /**
   * Replaces some variables in indexHtml to prepare it for being sent to the
   * client.
   *
   * @param device	The currently selected device MAC address.
   * @param target	The currently selected target broadcast ip address.
   * @return	The prepared index.html string.
   */
  static std::string PrepareIndexResponse(const String device,
                                          const String target);
};

#endif /* SRC_NETWORKHANDLER_H_ */
