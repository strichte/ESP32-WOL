/*
 * main.cpp
 *
 * Created on: 23.11.2021
 *
 * Copyright (C) 2021 ToMe25.
 * This project is licensed under the MIT License.
 * The MIT license can be found in the project root and at
 * https://opensource.org/licenses/MIT.
 */

#include "main.h"

#ifdef USE_I2C_DISPLAY
#include <Button.h>
#include <Display.h>
#endif

#include "esp_sntp.h"

#include "NetworkHandler.h"

hw_timer_t *timer_display = NULL;
hw_timer_t *timer_wol_interval = NULL;

volatile bool timer_display_expired = false;
volatile bool timer_wol_interval_expired = false;
bool start_up_wol_sent = false;
#ifdef USE_I2C_DISPLAY
Button up_button(UP_BUTTON);
Button down_button(DOWN_BUTTON);
int current_page = 0;
#endif

// Setup timers for display refresh and sending WOL packages
void IRAM_ATTR timerDisplayISR() { timer_display_expired = true; }
void IRAM_ATTR timerWOLIntervalISR() { timer_wol_interval_expired = true; }

// Setup callback function for ntp sync notification
void CbSyncTime(struct timeval *tv)  {
  Serial.println("NTP time synched");
}


void setup() {
  Serial.begin(115200);
  sntp_set_time_sync_notification_cb(CbSyncTime);
  NetworkHandler::Setup();

  timer_display = timerBegin(0, 80, true);
  timerAttachInterrupt(timer_display, &timerDisplayISR, true);
  timerAlarmWrite(timer_display, DISPLAY_INTERVAL * (uint64_t)1000000, true);
  timerAlarmEnable(timer_display);

  timer_wol_interval = timerBegin(1, 80, true);
  timerAttachInterrupt(timer_wol_interval, &timerWOLIntervalISR, true);
  timerAlarmWrite(timer_wol_interval, WOL_STARTUP * 60 * (uint64_t)1000000,
                  true);
  timerAlarmEnable(timer_wol_interval);
  
#ifdef USE_I2C_DISPLAY
  Wire.begin(I2C_SDA, I2C_SCL, I2C_SPEED);
  u8g2.setI2CAddress(SSD1315_ADDR);
  u8g2.begin();
  up_button.begin();
  down_button.begin();
#endif
}

void loop() {
  static bool first_run = true;
  static time_t wolEpoche;
  NetworkHandler::Loop();

  if (first_run) {
    time(&wolEpoche);
    wolEpoche += WOL_STARTUP * 60;
    NetworkHandler::SetNextWolTime(localtime(&wolEpoche));
    first_run = false;
  }

  if (timer_display_expired) {
#ifdef USE_I2C_DISPLAY
    u8g2.clearBuffer();                   // clear the internal memory
    u8g2.setFont(u8g2_font_siji_t_6x10);  // choose a suitable font
    u8g2.drawGlyph(118, 8, 0xe043);       // Network
    u8g2.drawGlyph(108, 8, 0xe015);       // NTP
    u8g2.drawBox(0, 10, 128, 4);          // progress bar
    u8g2.drawStr(0, 8,
                 ("IP: " + ETH.localIP().toString())
                     .c_str());  // write something to the internal memory
    u8g2.drawStr(
        0, 24,
        std::string("Next WOL : " + NetworkHandler::GetNextWolTime(date_only))
            .c_str());
    u8g2.drawStr(
        0, 32,
        std::string("           " + NetworkHandler::GetNextWolTime(time_only))
            .c_str());
    u8g2.drawStr(0, 40,
                 std::string("Cur. Time: " + NetworkHandler::GetTime(date_only))
                     .c_str());
    u8g2.drawStr(0, 48,
                 std::string("           " + NetworkHandler::GetTime(time_only))
                     .c_str());
    u8g2.drawStr(
        0, 56,
        std::string("Boot Time: " + NetworkHandler::GetUptime(date_only))
            .c_str());
    u8g2.drawStr(
        0, 64,
        std::string("           " + NetworkHandler::GetUptime(time_only))
            .c_str());
    u8g2.sendBuffer();
#else
    Serial.print("\rLast Boot: ");
    Serial.print(NetworkHandler::getUptime().c_str());
#endif
    timer_display_expired = false;
  }

  if (timer_wol_interval_expired) {
    if (false == start_up_wol_sent) {
      // Reset Timer to new interval
      timerAlarmWrite(timer_wol_interval, WOL_INTERVAL * 60 * (uint64_t)1000000,
                      true);  // Interrupt after 5 minutes
      Serial.println("Sending frist WOL after startup wait.");
      start_up_wol_sent = true;
    } else {
      Serial.println("Sending WOL packets");
    }
    time(&wolEpoche);
    wolEpoche += WOL_INTERVAL * 60;
    NetworkHandler::SetNextWolTime(localtime(&wolEpoche));
    Serial.print("Next WOL: ");
    Serial.println(NetworkHandler::GetNextWolTime().c_str());
    NetworkHandler::SendWol();
    timer_wol_interval_expired = false;
  }

#ifdef USE_I2C_DISPLAY
  if (down_button.pressed()) {
    // button is pressed
    Serial.println("Down button Pressed");
  }
  if (up_button.pressed()) {
    // button is pressed
    Serial.println("Up button Pressed");
  }
#endif
}
