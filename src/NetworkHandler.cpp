/*
 * NetworkHandler.cpp
 *
 * Created on: 23.11.2021
 *
 * Copyright (C) 2021 ToMe25.
 * This project is licensed under the MIT License.
 * The MIT license can be found in the project root and at
 * https://opensource.org/licenses/MIT.
 */

#include "NetworkHandler.h"

#include <ESPmDNS.h>
#include <FS.h>
#include <SD.h>

#include <ctime>
#include <regex>
#include <sstream>

#include "Display.h"
#include "WakeOnLanGenerator.h"
#include "esp_sntp.h"

extern I2CDisplay display;

bool NetworkHandler::eth_connected_ = false;
bool NetworkHandler::ntp_connected_ = false;

AsyncWebServer NetworkHandler::web_server_(kWebServerPort);
std::vector<WolDevice> NetworkHandler::wol_devices_;
IPAddress NetworkHandler::target_broadcast_ = kDefaultBroadcastAddress;
AsyncUDP NetworkHandler::udp_;
std::string NetworkHandler::boot_time_;
time_t NetworkHandler::next_wol_time_;
bool NetworkHandler::first_wol_sent_ = false;
NetworkConfig NetworkHandler::config_;

extern const char kIndexHtmlFile[] =
    "";  /// asm("_binary_src_html_index_html_start");
extern const char kIndexJsFile[] =
    "";                             /// asm("_binary_src_html_index_js_start");
extern const char kCssFile[] = "";  /// asm("_binary_src_html_main_css_start");
extern const char kNotFoundHtmlFile[] = "";

bool operator<(const WolDevice &left, const WolDevice &right) {
  if (left.mac < right.mac) return true;

  return false;
}

bool operator==(const WolDevice &left, const WolDevice &right) {
  if (left.mac == right.mac) return true;

  return false;
}

bool NetworkHandler::Setup(const char *config_file) {
  YAMLNode yaml_config;

  pinMode(SD_MISO_PIN, INPUT_PULLUP);
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
  bool first = true;
  while (!SD.begin(SD_CS_PIN)) {
    if (first) {
      char msg[] =
          "SD Card not found.\nInsert SD card with\n'config.yml'to\ncontinue.";
      display.UpdateMsgPage("Error:", msg);
      first = false;
    }
    delay(1000);
  }

  File file = SD.open(config_file);
  if (!file) {
    char msg[] = "Can't open YAML\nconfig file.";
    display.UpdateMsgPage("Error:", msg);
    delay(2000);
    return false;  // fatal, abort mission
  }

  int num_bytes;
  if (!(num_bytes = deserializeYml(yaml_config, file))) {
    char msg[] = "Unable to deserialize\nYAML file.";
    display.UpdateMsgPage("Error:", msg);
    delay(2000);
    return false;  // fatal, abort mission
  }

  if ("network:ip" != yaml_config.gettext("network:ip")) {
    config_.ip.fromString(yaml_config.gettext("network:ip"));
  } else {
    char msg[] = "No network:ip\nin YAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("network:gateway" != yaml_config.gettext("network:gateway")) {
    config_.gateway.fromString(yaml_config.gettext("network:gateway"));
  } else {
    char msg[] = "No\n'network:gateway'\nin YAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("network:netmask" != yaml_config.gettext("network:netmask")) {
    config_.subnet.fromString(yaml_config.gettext("network:netmask"));
  } else {
    char msg[] = "No\nnetwork:netmask\nin YAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("network:DNS" != yaml_config.gettext("network:DNS")) {
    config_.dns.fromString(yaml_config.gettext("network:DNS"));
  } else {
    char msg[] = "No network:DNS\nin YAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("network:hostname" != yaml_config.gettext("network:hostname")) {
    config_.hostname = yaml_config.gettext("network:hostname");
  } else {
    char msg[] = "No hostname in\nYAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("network:NTP1" != yaml_config.gettext("network:NTP1")) {
    if ("network:NTP2" != yaml_config.gettext("network:NTP2")) {
      config_.ntp1 = yaml_config.gettext("network:NTP1");
      config_.ntp2 = yaml_config.gettext("network:NTP2");
    } else {
      config_.ntp1 = yaml_config.gettext("network:NTP1");
    }
  } else {
    char msg[] = "No NTP server in\nYAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("timezone" != yaml_config.gettext("timezone")) {
    config_.timezone = yaml_config.gettext("timezone");
  } else {
    char msg[] = "No timezone in\nYAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("ota_password" != yaml_config.gettext("ota_password")) {
    config_.ota_password = yaml_config.gettext("ota_password");
  } else {
    char msg[] = "No ota_password in\nYAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("wol:startup" != yaml_config.gettext("wol:startup")) {
    config_.wol_startup = atol(yaml_config.gettext("wol:startup"));
  } else {
    char msg[] = "No WOL startup time\nin YAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("wol:repeat" != yaml_config.gettext("wol:repeat")) {
    config_.wol_repeat = atol(yaml_config.gettext("wol:repeat"));
  } else {
    char msg[] = "No WOL repeat time\nin YAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if ("wol:port" != yaml_config.gettext("wol:port")) {
    config_.wol_port = atol(yaml_config.gettext("wol:port"));
  } else {
    char msg[] = "No WOL port in\nYAML config.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }


  bool wol_success = false;
  bool last_element = false;
  int i = 0;
  while (!last_element) {
    std::ostringstream yaml_path_name;
    yaml_path_name << "devices:" << i << ":name";
    WolDevice device;
    device.name = yaml_config.gettext(yaml_path_name.str().c_str());
    std::ostringstream yaml_path_mac;
    yaml_path_mac << "devices:" << i++ << ":mac";
    device.mac = yaml_config.gettext(yaml_path_mac.str().c_str());
    if (device.name.empty() || device.mac.empty() ||
        device.name ==
            yaml_path_name.str()  // gettext() returns path if not found
        || device.mac == yaml_path_mac.str()) {
      last_element = true;
    } else {
      wol_success = true;
      wol_devices_.push_back(device);
      /* Serial.print("Adding device: ");
        Serial.print(device.name.c_str());
        Serial.print(": ");
        Serial.print(device.mac.c_str());
        Serial.println("");
        */
    }
  }
  SD.end();
  SPI.end();

  if (!wol_success) {
    char msg[] = "No target devices\nconfigured in\n YAML config.'.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  // SO far so good, setup network
  if (!SetupEth()) {
    return false;
  }
  /// SetupWebServer();
  SetupOta();
  return true;
}

std::string NetworkHandler::GetTime(DateTimeType t, tm *ti) {
  char ts[20];
  tm timeinfo;
  if (nullptr == ti) {
    ti = &timeinfo;
    if (!ntp_connected_ || !getLocalTime(ti)) {
      // Serial.println("Failed to obtain time.");
      return "";
    }
  }
  switch (t) {
    case all:
      strftime(ts, 20, "%F %H:%M:%S", ti);
      break;
    case date_only:
      strftime(ts, 20, "%F", ti);
      break;
    case time_only:
      strftime(ts, 20, "%H:%M:%S", ti);
      break;
  }
  return std::string(ts);
}

std::string NetworkHandler::GetUptime(DateTimeType type) {
  if (boot_time_.length() > 0) {
    switch (type) {
      case all:
        return boot_time_;
      case date_only:
        return boot_time_.substr(0, 10);
      case time_only:
        return boot_time_.substr(11);
    }
  }
  return GetRelativeUptime(type);
}

std::string NetworkHandler::GetNextWolTime(DateTimeType type) {
  static int16_t time_left = 0;
  if (next_wol_time_ < 100000) {
    // quick and dirty for detecting NTP not working (yet). Get relative time

    time_t epoche;
    time(&epoche);
    if (epoche < next_wol_time_) {
      time_left = next_wol_time_ - epoche;
      int16_t days = time_left / (24 * 3600);
      time_left = time_left % (24 * 3600);
      int hours = time_left / 3600;
      time_left = time_left % 3600;
      int minutes = time_left / 60;
      time_left = time_left % 60;

      std::vector<std::string> time_left_tokens;
      time_left_tokens.push_back("in");
      if (days) {
        time_left_tokens.push_back(std::to_string(days) + "d");
      }
      if (hours) {
        time_left_tokens.push_back(std::to_string(hours) + "h");
      }
      if (minutes) {
        time_left_tokens.push_back(std::to_string(minutes) + "m");
      }
      time_left_tokens.push_back(std::to_string(time_left) + "s");

      std::string time_left_str("");
      switch (type) {
        case date_only:
          time_left_str += time_left_tokens[0];
          if (time_left_tokens.size() > 1) {
            time_left_str += " " + time_left_tokens[1];
          }
          break;
        case time_only:
          if (time_left_tokens.size() >= 2) {
            for (int i = 2; i != time_left_tokens.size(); ++i) {
              time_left_str += time_left_tokens[i] + " ";
            }
          }
          break;
        case all:
          for (const std::string &token : time_left_tokens) {
            time_left_str += token + " ";
          }
      }
      return time_left_str;
    } else {
      // We got system time now. Set next WOL epoche correctly and continue
      // returning absolute time string
      next_wol_time_ = epoche + time_left - 1;
    }
  }
  return GetTime(type, localtime(&next_wol_time_));
}

void NetworkHandler::SendWol() {
  for (const WolDevice &device : wol_devices_) {
    // NetworkHandler::deviceHostnames.push_back(hostname);
    Serial.print(device.name.c_str());
    Serial.print(": ");
    Serial.print(device.mac.c_str());
    Serial.print(" ");
    Serial.print(ETH.broadcastIP());
    Serial.println("");
    AsyncUDPMessage wakePacket =
        WakeOnLanGenerator::generateWoLPacket(device.mac);
    /// DEBUG udp_.sendTo(wakePacket, ETH.broadcastIP(), config_.wol_port);
    delay(50);
  }
  first_wol_sent_ = true;
}

// Setup callback function for ntp sync notification
void NetworkHandler::CbSyncTime(struct timeval *tv) {
  NetworkHandler::SetNtpStatus(true);
  Serial.println("NTP time synched");
}

bool NetworkHandler::SetupEth() {
  WiFi.onEvent(OnEthEvent);

  if (!ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_RESET_PIN,
                 ETH_CLK_MODE)) {
    char msg[] = "ETH start Failed.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }

  if (ETH.config(config_.ip, config_.gateway, config_.subnet, config_.dns) ==
      false) {
    char msg[] = "ETH configuration\nfailed.";
    display.UpdateMsgPage("Error:", msg);
    return false;
  }
  return true;
}

bool NetworkHandler::SetupNtp() {
  struct tm timeInfo;
  sntp_set_time_sync_notification_cb(CbSyncTime);
  sntp_set_sync_interval(1 * 60 * 60 * 1000UL);  // 1 hour

  if ("" != config_.ntp2) {
    configTime(0, 0, config_.ntp1.c_str(), config_.ntp2.c_str());
  } else {
    configTime(0, 0, config_.ntp1.c_str());
  }
  if (!getLocalTime(&timeInfo)) {
    char msg[] = "Failed to setup NTP.\nNetwork connected?.";
    display.UpdateMsgPage("Error:", msg);
    return true;  // temp failure. Might come back
  }

  if ("" != config_.timezone) {
    setenv("TZ", config_.timezone.c_str(), 1);
    tzset();
  }

  time_t epoche;
  time(&epoche);
  int64_t uptime = esp_timer_get_time() / 1000000;
  time_t boot_time_epoche = epoche - uptime;
  struct tm *ti = localtime(&boot_time_epoche);
  boot_time_ = GetTime(all, ti);
  Serial.print(epoche);
  Serial.print(" sec since the Epoche. ");
  Serial.println(GetTime().c_str());
  Serial.print(boot_time_epoche);
  Serial.print(" boot time Epoche. ");
  Serial.println(GetTime(all, ti).c_str());
  return true;
}

void NetworkHandler::OnEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      // set eth hostname here
      ETH.setHostname(config_.hostname.c_str());
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected_ = true;
      SetupNtp();
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected_ = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected_ = false;
      break;
    default:
      break;
  }
}

void NetworkHandler::SetupWebServer() {
  web_server_.on("/", HTTP_GET, OnIndexGet);
  web_server_.on("/", HTTP_POST, OnIndexPost);
  web_server_.on("/index.html", HTTP_GET, OnIndexGet);
  web_server_.on("/index.html", HTTP_POST, OnIndexPost);

  web_server_.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/javascript", kIndexJsFile);
  });

  web_server_.on("/main.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/css", kCssFile);
  });

  web_server_.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/html", kNotFoundHtmlFile);
  });

  web_server_.begin();
}

void NetworkHandler::SetupOta() {
  ArduinoOTA.setHostname(config_.hostname.c_str());
  ArduinoOTA.setPassword(config_.ota_password.c_str());
  ArduinoOTA.onStart([]() { Serial.println("Start updating firmware."); });
  ArduinoOTA.onProgress([](uint progress, uint total) {
    //    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    Serial.print(".");
  });

  ArduinoOTA.onEnd([]() { Serial.println("\nUpdate Done."); });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed.");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed.");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed.");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed.");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed.");
    }
  });

  ArduinoOTA.begin();
  MDNS.addService("http", "tcp", 80);
}

std::string NetworkHandler::GetRelativeUptime(const DateTimeType &type) {
  int64_t uptime = esp_timer_get_time() / 1000000;

  int16_t days = uptime / (24 * 3600);
  uptime = uptime % (24 * 3600);

  int hours = uptime / 3600;
  uptime = uptime % 3600;

  int minutes = uptime / 60;
  uptime = uptime % 60;

  std::vector<std::string> uptime_tokens;
  if (days) {
    uptime_tokens.push_back(std::to_string(days) + "d");
  }
  if (hours) {
    uptime_tokens.push_back(std::to_string(hours) + "h");
  }
  if (minutes) {
    uptime_tokens.push_back(std::to_string(minutes) + "m");
  }
  uptime_tokens.push_back(std::to_string(uptime) + "s");
  uptime_tokens.push_back("ago");

  std::string uptime_str("");
  switch (type) {
    case date_only:
      uptime_str += uptime_tokens[0];
      if (uptime_tokens.size() > 1) {
        uptime_str += " " + uptime_tokens[1];
      }
      break;
    case time_only:
      if (uptime_tokens.size() >= 2) {
        for (int i = 2; i != uptime_tokens.size(); ++i) {
          uptime_str += uptime_tokens[i] + " ";
        }
      }
      break;
    case all:
      for (const std::string &token : uptime_tokens) {
        uptime_str += token + " ";
      }
  }
  return uptime_str;
}

void NetworkHandler::OnIndexGet(AsyncWebServerRequest *request) {
  std::string response = PrepareIndexResponse("", target_broadcast_.toString());
  request->send(200, "text/html", response.c_str());
}

void NetworkHandler::OnIndexPost(AsyncWebServerRequest *request) {
  if (request->hasParam("device", true) && request->hasParam("target", true)) {
    String device = request->getParam("device", true)->value();
    String target = request->getParam("target", true)->value();

    if (device == "Other") {
      if (request->hasParam("custom_device", true)) {
        device = request->getParam("custom_device", true)->value();
      } else {
        device = "";
      }
    }

    if (target == "Other") {
      if (request->hasParam("custom_target", true)) {
        target = request->getParam("custom_target", true)->value();
      } else {
        target = "";
      }
    }

    IPAddress originalBroadcastTarget = target_broadcast_;

    if (!WakeOnLanGenerator::isValidMac(device.c_str())) {
      std::string message = "error\">Received invalid device mac address \"";
      message += device.c_str();
      message += "\".";

      std::string response = PrepareIndexResponse(device, target);
      response = std::regex_replace(
          response, std::regex("\\$message_type\" hidden>\\$message"),
          message.c_str());
      request->send(400, "text/html", response.c_str());
    } else if (!target_broadcast_.fromString(target)) {
      target_broadcast_ = originalBroadcastTarget;
      std::string message = "error\">Received invalid target IP address \"";
      message += target.c_str();
      message += "\".";

      std::string response = PrepareIndexResponse(device, target);
      response = std::regex_replace(
          response, std::regex("\\$message_type\" hidden>\\$message"),
          message.c_str());
      request->send(400, "text/html", response.c_str());
    } else {
      std::string n;
      if (std::count(wol_devices_.begin(), wol_devices_.end(),
                     WolDevice(device, n)) == 0) {
        wol_devices_.push_back(WolDevice(device, n));
      }

      Serial.print("Waking up device ");
      Serial.print(device);
      Serial.println('.');

      AsyncUDPMessage wakePacket =
          WakeOnLanGenerator::generateWoLPacket(device.c_str());
      udp_.sendTo(wakePacket, target_broadcast_, kWolTargetPort);

      std::string message = "success\">Woke up device ";
      message += device.c_str();
      message += '.';

      std::string response = PrepareIndexResponse(device, target);
      response = std::regex_replace(
          response, std::regex("\\$message_type\" hidden>\\$message"),
          message.c_str());
      request->send(200, "text/html", response.c_str());
    }
  } else {
    OnIndexGet(request);
  }
}

std::string NetworkHandler::PrepareIndexResponse(const String device,
                                                 const String target) {
  std::string response = kIndexHtmlFile;

  std::string devices = "\n";
  for (const WolDevice &wol_device : wol_devices_) {
    devices += "<option value=\"";
    devices += wol_device.mac;
    devices += "\">";
    devices += wol_device.mac;
    devices += " ";
    devices += wol_device.name;
    devices += "</option>\n";
  }
  response =
      std::regex_replace(response, std::regex("\\$devices"), devices.c_str());

  std::vector<std::string> targetIPs;
  targetIPs.reserve(4);
  if (target.length() > 6) {
    targetIPs.push_back(std::string(target.c_str()));
  }
  std::string ip(target_broadcast_.toString().c_str());
  if (std::count(targetIPs.begin(), targetIPs.end(), ip) == 0) {
    targetIPs.push_back(ip);
  }
  if (kDefaultBroadcastAddress == IPAddress((uint32_t)0)) {
    IPAddress localBroadcast = ETH.broadcastIP();
    ip = std::string(localBroadcast.toString().c_str());
    if (std::count(targetIPs.begin(), targetIPs.end(), ip) == 0) {
      targetIPs.push_back(ip);
    }
  } else {
    ip = std::string(kDefaultBroadcastAddress.toString().c_str());
    if (std::count(targetIPs.begin(), targetIPs.end(), ip) == 0) {
      targetIPs.push_back(ip);
    }
  }
  IPAddress globalBroadcast(255, 255, 255, 255);
  ip = std::string(globalBroadcast.toString().c_str());
  if (std::count(targetIPs.begin(), targetIPs.end(), ip) == 0) {
    targetIPs.push_back(ip);
  }

  std::string targets = "\n";
  for (const std::string &target : targetIPs) {
    targets += "<option value=\"";
    targets += target;
    targets += "\">";
    targets += target;
    targets += "</option>\n";
  }
  response =
      std::regex_replace(response, std::regex("\\$targets"), targets.c_str());

  response =
      std::regex_replace(response, std::regex("\\$device"), device.c_str());
  response =
      std::regex_replace(response, std::regex("\\$target"), target.c_str());

  return response;
}
