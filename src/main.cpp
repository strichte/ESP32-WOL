/*
 * main.cpp
 *
 * Created on: 23.11.2021
 *
 * Copyright (C) 2021 ToMe25.
 * This project is licensed under the MIT License.
 * The MIT license can be found in the project root and at https://opensource.org/licenses/MIT.
 */

#include <main.h>
#include <NetworkHandler.h>

hw_timer_t *timerDisplay = NULL;
hw_timer_t *timerWOLInterval = NULL;

volatile bool timerDisplayExpired = false;
volatile bool timerWOLIntervalExpired = false;
bool startupWOLSent = false;

void IRAM_ATTR timerDisplayISR() {
 	timerDisplayExpired = true;
}

void IRAM_ATTR timerWOLIntervalISR() {
 	timerWOLIntervalExpired = true;
}

void setup() {
	Serial.begin(115200);
	NetworkHandler::setup();

	timerDisplay = timerBegin(0, 80, true); // Timer 0, clock divider 80
  	timerAttachInterrupt(timerDisplay, &timerDisplayISR, true); // Attach the interrupt handling function
  	timerAlarmWrite(timerDisplay, DISPLAY_INTERVAL * 1000000, true); // Interrupt every 1 second
  	timerAlarmEnable(timerDisplay); // Enable the alarm

	timerWOLInterval = timerBegin(1, 80, true); // Timer 0, clock divider 80
  	timerAttachInterrupt(timerWOLInterval, &timerWOLIntervalISR, true); // Attach the interrupt handling function
  	timerAlarmWrite(timerWOLInterval, WOL_STARTUP * 60 * 1000000, true); // Interrupt after 60 minutes
  	timerAlarmEnable(timerWOLInterval); // Enable the alarm
}

void loop() {
	static bool first_run=true;
	NetworkHandler::loop();

	if(first_run){	
		Serial.print("Next WOL: ");
		Serial.println(NetworkHandler::nextWOLTime(startupWOLSent).c_str());
		first_run=false;
	}

	if(timerDisplayExpired)
  	{
		Serial.print("\rLast Boot: ");
		Serial.print(NetworkHandler::getUptime().c_str());
		timerDisplayExpired = false; 
  	}
	
	if(timerWOLIntervalExpired)
  	{
		if( false == startupWOLSent ){
			// Reset Timer to new interval
  			timerAlarmWrite(timerWOLInterval, WOL_INTERVAL * 60 * 1000000, true); // Interrupt after 5 minutes
	
			Serial.println("Sending frist WOL after startup wait.");
			startupWOLSent = true;
		}else{
			Serial.println("Sending WOL packets");
		}
		NetworkHandler::sendWOL();
   		Serial.print("Next WOL: ");
		Serial.println(NetworkHandler::nextWOLTime(startupWOLSent).c_str());
    	timerWOLIntervalExpired = false; 
  	}
}
