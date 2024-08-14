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
#include "Display.h"
#endif

#include "esp_sntp.h"

#include "NetworkHandler.h"
#include "Timer.h"

Timer timer_display(0, DISPLAY_INTERVAL);
Timer timer_wol(1, WOL_STARTUP * 60);

bool start_up_wol_sent = false;
#ifdef USE_I2C_DISPLAY
Button up_button(UP_BUTTON);
Button down_button(DOWN_BUTTON);
int current_page = 0;
#endif


void setup() {
  Serial.begin(115200);

  NetworkHandler::Setup();
  timer_display.Enable();
  timer_wol.Enable();

#ifdef USE_I2C_DISPLAY
  Wire.begin(I2C_SDA, I2C_SCL, I2C_SPEED);
  u8g2.setI2CAddress(SSD1315_ADDR);
  u8g2.begin();
  up_button.begin();
  down_button.begin();
#endif //USE_I2C_DISPLAY
}

void loop() {
  static bool first_run = true;
  static time_t wol_epoche;
  NetworkHandler::Loop();

  if (first_run) {
    // delay(5000);
    time(&wol_epoche);
    wol_epoche += WOL_STARTUP * 60;
    // NetworkHandler::SetNextWolTime(localtime(&wol_epoche));
    NetworkHandler::SetNextWolTime(wol_epoche);
    Serial.print("first run wol_epoche=");
    Serial.println(wol_epoche);
    first_run = false;
  }

  if ( timer_display.IsExpired() ) {
#ifdef USE_I2C_DISPLAY
    u8g2.clearBuffer();                   // clear the internal memory
    u8g2.setFont(u8g2_font_siji_t_6x10);  // choose a suitable font
    u8g2.drawGlyph(118, 8, 0xe043);       // Network
    u8g2.drawGlyph(108, 8, 0xe015);       // NTP
    u8g2.drawStr(0, 8,
                 ("IP: " + ETH.localIP().toString())
                     .c_str());  // write something to the internal memory

    NextWolTime next_wol_time = NetworkHandler::GetNextWolTime(date_only);
    u8g2.drawStr(0, 24, std::string("Next WOL : " + next_wol_time.str).c_str());
    next_wol_time = NetworkHandler::GetNextWolTime(time_only);
    u8g2.drawStr(0, 32, std::string("           " + next_wol_time.str).c_str());
    u8g2.drawBox(0, 10, 128 / 100.0F * timer_wol.PercentRemaining(), 4);  // progress bar

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
#else // USE_I2C_DISPLAY
    Serial.print("\rLast Boot: ");
    Serial.print(NetworkHandler::getUptime().c_str());
#endif // USE_I2C_DISPLAY
    timer_display.Reset();
  }

  if ( timer_wol.IsExpired() ) {
    if (false == start_up_wol_sent) {
      // Reset Timer to new interval
      timer_wol.SetTimer(WOL_INTERVAL * 60);
      Serial.println("Sending first WOL after startup wait.");
      start_up_wol_sent = true;
    } else {
      Serial.println("Sending WOL");
    }
    time(&wol_epoche);
    wol_epoche += WOL_INTERVAL * 60;
    NetworkHandler::SetNextWolTime(wol_epoche);
    Serial.print("Next run wol_epoche=");
    Serial.println(wol_epoche);
    Serial.print("Next WOL: ");
    Serial.println(NetworkHandler::GetNextWolTime().str.c_str());
    NetworkHandler::SendWol();
    timer_wol.Reset();
  }

#ifdef USE_I2C_DISPLAY
  if (down_button.pressed()) {
    // button is pressed
    Serial.println("Down button Pressed");
  }
  if (up_button.pressed()) {
    // button is pressed
    Serial.println("Up button Pressed");

    time(&wol_epoche);
    wol_epoche += WOL_INTERVAL * 60;
    NetworkHandler::SetNextWolTime(wol_epoche);
    Serial.print("Next run wol_epoche=");
    Serial.println(wol_epoche);
    Serial.print("Next WOL: ");
    Serial.println(NetworkHandler::GetNextWolTime().str.c_str());
    NetworkHandler::SendWol();
    timer_wol.Reset();
  }
#endif // USE_I2C_DISPLAY
}
