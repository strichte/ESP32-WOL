/*
 * NetworkHandler.cpp
 *
 * Created on: 23.11.2021
 *
 * Copyright (C) 2021 ToMe25.
 * This project is licensed under the MIT License.
 * The MIT license can be found in the project root and at https://opensource.org/licenses/MIT.
 */

#include <NetworkHandler.h>
#include <WakeOnLanGenerator.h>
#include <ctime>
#include <regex>
#include <sstream>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

bool NetworkHandler::ethConnected = false;
bool NetworkHandler::ntpConnected = false;

AsyncWebServer NetworkHandler::webServer(WEBSERVER_PORT);
std::vector<wolDevice> NetworkHandler::wolDevices;
IPAddress NetworkHandler::targetBroadcast = DEFAULT_TARGET_BROADCAST;
AsyncUDP NetworkHandler::udp;
std::string NetworkHandler::bootTime;
struct tm NetworkHandler::nextWOLTime;

bool operator< (const wolDevice &left, const wolDevice &right)
{
  if(left.mac < right.mac)
    return true;

  return false;
}

bool operator== (const wolDevice &left, const wolDevice &right)
{
  if(left.mac == right.mac)
    return true;

  return false;
}

void NetworkHandler::setup() {
	setupETH();
	setupWOLTargets();
	setupWebServer();
	setupOTA();
}

void NetworkHandler::setupETH(){
	WiFi.onEvent(onETHEvent);

#ifdef ETH_POWER_PIN
    pinMode(ETH_POWER_PIN, OUTPUT);
    digitalWrite(ETH_POWER_PIN, HIGH);
#endif

#if CONFIG_IDF_TARGET_ESP32
    if (!ETH.begin(ETH_TYPE, ETH_ADDR, ETH_MDC_PIN,
                   ETH_MDIO_PIN, ETH_RESET_PIN, ETH_CLK_MODE)) {
        Serial.println("ETH start Failed!");
    }
#else
    if (!ETH.begin(ETH_PHY_W5500, 1, ETH_CS_PIN, ETH_INT_PIN, ETH_RST_PIN,
                   SPI3_HOST,
                   ETH_SCLK_PIN, ETH_MISO_PIN, ETH_MOSI_PIN)) {
        Serial.println("ETH start Failed!");
    }
#endif

    if (ETH.config(STATIC_IP, GATEWAY, SUBNET, DNS) == false) {
        Serial.println("Configuration failed.");
    }
}

void NetworkHandler::setupNTP(){
	struct tm timeInfo;

	Serial.println("Setting up NTP");
	configTime(0, 0, NTP1, NTP2);
	if(!getLocalTime(&timeInfo)){
 		Serial.println("  Failed to setup NTP");
    	return;
  	}
	Serial.println("  Got the time from NTP");
  	// Now we can set the real timezone
  	setTimezone(timeZone);
	ntpConnected = true;

	time_t epoche;
	time(&epoche);
	int64_t uptime = esp_timer_get_time() / 1000000;
	time_t boot_time_epoche = epoche - uptime;
	struct tm * ti = localtime(&boot_time_epoche);
	bootTime = getTime(all, ti);
	Serial.print(epoche);
	Serial.print(" sec since the Epoche. ");
	Serial.println(getTime().c_str());		
	Serial.print(boot_time_epoche);
	Serial.print(" boot time Epoche. ");
	Serial.println(getTime(all, ti).c_str());		
}

std::string NetworkHandler::getTime(dateTimeType t, tm *ti){
  char ts[20];
  tm timeinfo;
  if(nullptr == ti){
	ti = &timeinfo;
    if(!getLocalTime(ti)){
    	Serial.println("Failed to obtain time.");
    	return "";
  	}
  }
  switch(t){
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

std::string NetworkHandler::getNextWOLTime(dateTimeType t){
	return getTime(t,&nextWOLTime);
}

void NetworkHandler::setTimezone(std::string timezone){
  	Serial.printf("  Setting Timezone to %s\n",timezone.c_str());
  	setenv("TZ",timezone.c_str(),1);  //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
  	tzset();	
}

void NetworkHandler::setNextWOLTime(tm *ti){
	if(ti){
		NetworkHandler::nextWOLTime = *ti;
	}
}

std::string NetworkHandler::getUptime(dateTimeType type){
	if(bootTime.length() > 0){
		switch(type){
			case all: 
				return bootTime;
			case date_only:
				return bootTime.substr(0,10);
			case time_only:
				return bootTime.substr(11);
		}
	}	
	return getUptimeRel(type);
}

std::string NetworkHandler::getUptimeRel(dateTimeType type){
	int64_t uptime = esp_timer_get_time() / 1000000;
	int16_t days = uptime / (24 *3600);
	uptime = uptime % (24 * 3600);
	
	int hours = uptime / 3600;
	uptime = uptime % 3600;
	
	int minutes = uptime / 60;
	uptime = uptime % 60;
	std::string uptimeStr;
	if(days) {
		uptimeStr += std::to_string(days) + "d ";
	}
	if(hours) {
		uptimeStr += std::to_string(hours) + "h ";
	}
	if(minutes) {
		uptimeStr += std::to_string(minutes) + "m ";
	}
	uptimeStr += std::to_string(uptime) + "s";
	switch(type){
		case time_only:
			return "ago";
		case date_only:
			return uptimeStr;
	}
	return uptimeStr + " ago";
}

void NetworkHandler::loop() {
	ArduinoOTA.handle();
}

void NetworkHandler::onETHEvent(WiFiEvent_t event)
{
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        //set eth hostname here
        ETH.setHostname(HOSTNAME);
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
        ethConnected = true;
		setupNTP();
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
        ethConnected = false;
		ntpConnected = false;
        break;
    case ARDUINO_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        ethConnected = false;
        break;
    default:
        break;
    }
}

void NetworkHandler::setupWOLTargets(){
	std::string deviceMacs = DEVICE_MACS;
	deviceMacs.erase(std::remove(deviceMacs.begin(), deviceMacs.end(), '\r'),
			deviceMacs.end());

	std::istringstream deviceStream(deviceMacs);
	std::string deviceStr;
	while (std::getline(deviceStream, deviceStr, '\n')) {
		if (deviceStr.length() > 0) {
			std::string mac = deviceStr.substr(deviceStr.find('|') + 1, deviceStr.length());
			mac.erase( std::remove_if( mac.begin(), mac.end(), ::isspace ), mac.end() );
			wolDevice device(mac, deviceStr.substr(0,deviceStr.find('|')));
			NetworkHandler::wolDevices.push_back(device);
			Serial.print("Adding device: ");
			Serial.print(device.name.c_str());
			Serial.print(": ");
			Serial.print(device.mac.c_str());
			Serial.println("");
		}
	}
}

void NetworkHandler::setupWebServer() {
	webServer.on("/", HTTP_GET, onIndexGet);
	webServer.on("/", HTTP_POST, onIndexPost);
	webServer.on("/index.html", HTTP_GET, onIndexGet);
	webServer.on("/index.html", HTTP_POST, onIndexPost);

	webServer.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/javascript", INDEX_JS);
	});

	webServer.on("/main.css", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/css", MAIN_CSS);
	});

	webServer.onNotFound([](AsyncWebServerRequest *request) {
		request->send(404, "text/html", NOT_FOUND_HTML);
	});

	webServer.begin();
}

void NetworkHandler::setupOTA() {
	ArduinoOTA.setHostname(HOSTNAME);
	ArduinoOTA.setPassword(OTA_PASS);

	ArduinoOTA.onStart([]() {
		Serial.println("Start updating sketch.");
	});

	ArduinoOTA.onProgress([](uint progress, uint total) {
		Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});

	ArduinoOTA.onEnd([]() {
		Serial.println("\nUpdate Done.");
	});

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

void NetworkHandler::onIndexGet(AsyncWebServerRequest *request) {
	std::string response = prepareIndexResponse("",
			targetBroadcast.toString());
	request->send(200, "text/html", response.c_str());
}

void NetworkHandler::onIndexPost(AsyncWebServerRequest *request) {
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

		IPAddress originalBroadcastTarget = targetBroadcast;

		if (!WakeOnLanGenerator::isValidMac(device.c_str())) {
			std::string message = "error\">Received invalid device mac address \"";
			message += device.c_str();
			message += "\".";

			std::string response = prepareIndexResponse(device, target);
			response = std::regex_replace(response, std::regex("\\$message_type\" hidden>\\$message"), message.c_str());
			request->send(400, "text/html", response.c_str());
		} else if (!targetBroadcast.fromString(target)) {
			targetBroadcast = originalBroadcastTarget;
			std::string message = "error\">Received invalid target IP address \"";
			message += target.c_str();
			message += "\".";

			std::string response = prepareIndexResponse(device, target);
			response = std::regex_replace(response, std::regex("\\$message_type\" hidden>\\$message"), message.c_str());
			request->send(400, "text/html", response.c_str());
		} else {
			std::string n("Web Add");
			if (std::count(wolDevices.begin(), wolDevices.end(), wolDevice(device, n)) == 0) {
				std::string n("Web Add");
				wolDevices.push_back(wolDevice(device,n));
			}

			Serial.print("Waking up device ");
			Serial.print(device);
			Serial.println('.');

			AsyncUDPMessage wakePacket = WakeOnLanGenerator::generateWoLPacket(device.c_str());
			udp.sendTo(wakePacket, targetBroadcast, TARGET_PORT);

			std::string message = "success\">Woke up device ";
			message += device.c_str();
			message += '.';

			std::string response = prepareIndexResponse(device, target);
			response = std::regex_replace(response, std::regex("\\$message_type\" hidden>\\$message"), message.c_str());
			request->send(200, "text/html", response.c_str());
		}
	} else {
		onIndexGet(request);
	}
}

std::string NetworkHandler::prepareIndexResponse(const String device, const String target) {
	std::string response = INDEX_HTML;

	std::string devices = "\n";
	for (wolDevice device : wolDevices) {
		devices += "<option value=\"";
		devices += device.mac;
		devices += "\">";
		devices += device.mac;
		devices += " ";
		devices += device.name;
		devices += "</option>\n";
	}
	response = std::regex_replace(response, std::regex("\\$devices"),
			devices.c_str());

	std::vector<std::string> targetIPs;
	targetIPs.reserve(4);
	if (target.length() > 6) {
#ifdef ESP8266
		if (target == "255.255.255.255") {
			targetIPs.push_back("(IP unset)");
		} else
#endif
			targetIPs.push_back(std::string(target.c_str()));
	}
	std::string ip(targetBroadcast.toString().c_str());
	if (std::count(targetIPs.begin(), targetIPs.end(), ip) == 0) {
		targetIPs.push_back(ip);
	}
	if (DEFAULT_TARGET_BROADCAST == IPAddress((uint32_t) 0)) {
		IPAddress localBroadcast = ETH.broadcastIP();
		ip = std::string(localBroadcast.toString().c_str());
		if (std::count(targetIPs.begin(), targetIPs.end(), ip) == 0) {
			targetIPs.push_back(ip);
		}
	} else {
		ip = std::string(DEFAULT_TARGET_BROADCAST.toString().c_str());
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
	for (std::string target : targetIPs) {
#ifdef ESP8266
		if (target == "(IP unset)") {
			target = "255.255.255.255";
		}
#endif

		targets += "<option value=\"";
		targets += target;
		targets += "\">";
		targets += target;
		targets += "</option>\n";
	}
	response = std::regex_replace(response, std::regex("\\$targets"),
			targets.c_str());

	response = std::regex_replace(response, std::regex("\\$device"), device.c_str());
	response = std::regex_replace(response, std::regex("\\$target"), target.c_str());

	return response;
}

void NetworkHandler::sendWOL(){
	for (wolDevice device : wolDevices){
		//NetworkHandler::deviceHostnames.push_back(hostname);
		Serial.print(device.name.c_str());
		Serial.print(": ");
		Serial.print(device.mac.c_str());
		Serial.print(" ");
		Serial.print(ETH.broadcastIP());
		Serial.println("");
		AsyncUDPMessage wakePacket = WakeOnLanGenerator::generateWoLPacket(device.mac);
		udp.sendTo(wakePacket, ETH.broadcastIP(), TARGET_PORT);
		delay(50);
	}
}