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
#define YAML_DISABLE_CJSON       // disable all cJSON functions
#define YAML_DISABLE_ARDUINOJSON // disable all ArduinoJson functions
#include <ArduinoYaml.h>  // Happy with plain YAML for out needs
#include <ctime>

#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
#include <ETHClass2.h>  //Is to use the modified ETHClass
#define ETH ETH2
#else
#include <ETH.h>
#endif

#define DISPLAY_INTERVAL 1  // time between display updates in sec
#define WEB_SERVER_PORT 80

// Web Server constants
static const IPAddress kDefaultBroadcastAddress =
    (uint)0;  // set to 0 to use the local network broadcast ip instead.

// Wake on Lan constants
static const uint16_t kWolTargetPort = 9;

// Network config
struct NetworkConfig {
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;
  String ntp1;
  String ntp2;
  String hostname;
  String timezone;
  String ota_password;
  uint16_t wol_startup;
  uint16_t wol_repeat;
  uint16_t wol_port;
  bool web_enabled;
  String web_user;
  String web_password;
};

// Device information
struct WolDevice {
  String mac;
  String name;
  WolDevice(){};
  WolDevice(const String &m, const String &n) : mac(m), name(n) {}
};

bool operator<(const WolDevice &left, const WolDevice &right);
bool operator==(const WolDevice &left, const WolDevice &right);

enum DateTimeType { all, date_only, time_only };

class NetworkHandler {
 public:
  static bool Setup(const char *config_file);
  static NetworkConfig &Config() { return config_; };
  static void SetNtpStatus(const bool &n) { ntp_connected_ = n; };
  static String GetTime(DateTimeType t = all, tm *ti = nullptr);
  static String GetUptime(DateTimeType t = all);
  static void SetNextWolTime(const time_t &e) {NetworkHandler::next_wol_time_ = e;}
  static String GetNextWolTime(DateTimeType t = all);
  static void SendWol();
  static void SendWol(const WolDevice &wol_device);
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
  static String boot_time_;
  static AsyncUDP udp_;
  static NetworkConfig config_;
  
  static bool SetupEth();
  static bool SetupNtp();
  static void OnEthEvent(WiFiEvent_t event);
  static void SetupWebServer();
  static void SetupOta();
  static String GetRelativeUptime(const DateTimeType &type = all);
  static String HTMLProcessor(const String &var);
};

#endif /* SRC_NETWORKHANDLER_H_ */
