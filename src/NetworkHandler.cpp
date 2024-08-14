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

#include <ctime>
#include <regex>
#include <sstream>

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "WakeOnLanGenerator.h"
#include "esp_sntp.h"

bool NetworkHandler::eth_connected_ = false;
bool NetworkHandler::ntp_connected_ = false;

AsyncWebServer NetworkHandler::web_server_(kWebServerPort);
std::vector<WolDevice> NetworkHandler::wol_devices_;
IPAddress NetworkHandler::target_broadcast_ = kDefaultBroadcastAddress;
AsyncUDP NetworkHandler::udp_;
std::string NetworkHandler::boot_time_;
struct tm NetworkHandler::next_wol_time_;

bool operator<(const WolDevice &left, const WolDevice &right) {
  if (left.mac < right.mac) return true;

  return false;
}

bool operator==(const WolDevice &left, const WolDevice &right) {
  if (left.mac == right.mac) return true;

  return false;
}

void NetworkHandler::Setup() {
  SetupEth();
  SetupWolTargets();
  SetupWebServer();
  SetupOta();
}

void NetworkHandler::SetupEth() {
  WiFi.onEvent(OnEthEvent);

#ifdef ETH_POWER_PIN
  pinMode(ETH_POWER_PIN, OUTPUT);
  digitalWrite(ETH_POWER_PIN, HIGH);
#endif

#if CONFIG_IDF_TARGET_ESP32
  if (!ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_RESET_PIN,
                 ETH_CLK_MODE)) {
    Serial.println("ETH start Failed!");
  }
#else
  if (!ETH.begin(ETH_PHY_W5500, 1, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                 SPI3_HOST, ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN)) {
    Serial.println("ETH start Failed!");
  }
#endif

  if (ETH.config(kStaticIp, kGateway, kSubnet, kDnsServer) == false) {
    Serial.println("Configuration failed.");
  }
}

void NetworkHandler::SetupNtp() {
  struct tm timeInfo;

  Serial.println("Setting up NTP");
  sntp_set_sync_interval(1 * 60 * 60 * 1000UL); // 1 hour
  configTime(0, 0, kNtpServer1, kNtpServer2);
  if (!getLocalTime(&timeInfo)) {
    Serial.println("  Failed to setup NTP");
    return;
  }
  Serial.println("  Got the time from NTP");
  // Now we can set the real timezone
  SetTimezone(kTimeZone);
  ntp_connected_ = true;

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
}

std::string NetworkHandler::GetTime(DateTimeType t, tm *ti) {
  char ts[20];
  tm timeinfo;
  if (nullptr == ti) {
    ti = &timeinfo;
  if (!ntp_connected_ || !getLocalTime(ti)) {
      //Serial.println("Failed to obtain time.");
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

std::string NetworkHandler::GetNextWolTime(DateTimeType t) {
  return GetTime(t, &next_wol_time_);
}

void NetworkHandler::SetTimezone(const std::string &timezone) {
  Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1);  //  Now adjust the TZ.  Clock settings are
                                      //  adjusted to show the new local time
  tzset();
}

void NetworkHandler::SetNextWolTime(const tm *ti) {
  if (ti) {
    NetworkHandler::next_wol_time_ = *ti;
  }
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
        if(uptime_tokens.size()>1){
          uptime_str += " " + uptime_tokens[1];
        } 
      break;
    case time_only:
      if(uptime_tokens.size() >= 2){
        for (int i=2; i!=uptime_tokens.size(); ++i) {
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

void NetworkHandler::Loop() { ArduinoOTA.handle(); }

void NetworkHandler::OnEthEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      // set eth hostname here
      ETH.setHostname(kHostname);
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
      ntp_connected_ = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected_ = false;
      break;
    default:
      break;
  }
}

void NetworkHandler::SetupWolTargets() {
  std::string deviceMacs = kWolDevicesFile;
  deviceMacs.erase(std::remove(deviceMacs.begin(), deviceMacs.end(), '\r'),
                   deviceMacs.end());

  std::istringstream deviceStream(deviceMacs);
  std::string deviceStr;
  while (std::getline(deviceStream, deviceStr, '\n')) {
    if (deviceStr.length() > 0) {
      std::string mac =
          deviceStr.substr(deviceStr.find('|') + 1, deviceStr.length());
      mac.erase(std::remove_if(mac.begin(), mac.end(), ::isspace), mac.end());
      WolDevice device(mac, deviceStr.substr(0, deviceStr.find('|')));
      NetworkHandler::wol_devices_.push_back(device);
      Serial.print("Adding device: ");
      Serial.print(device.name.c_str());
      Serial.print(": ");
      Serial.print(device.mac.c_str());
      Serial.println("");
    }
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
  ArduinoOTA.setHostname(kHostname);
  ArduinoOTA.setPassword(kOtaPasswordFile);

  ArduinoOTA.onStart([]() { Serial.println("Start updating sketch."); });

  ArduinoOTA.onProgress([](uint progress, uint total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
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
    udp_.sendTo(wakePacket, ETH.broadcastIP(), kWolTargetPort);
    delay(50);
  }
}