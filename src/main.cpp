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

#include <NetworkHandler.h>
#include <main.h>

#ifdef USE_I2C_DISPLAY
#include <Button.h>
#include <Display.h>

#endif

hw_timer_t *timerDisplay = NULL;
hw_timer_t *timerWOLInterval = NULL;

volatile bool timerDisplayExpired = false;
volatile bool timerWOLIntervalExpired = false;
bool startupWOLSent = false;
#ifdef USE_I2C_DISPLAY
Button upButton(UP_BUTTON);
Button downButton(DOWN_BUTTON);
int currentPage = 0;
#endif

void IRAM_ATTR timerDisplayISR() { timerDisplayExpired = true; }

void IRAM_ATTR timerWOLIntervalISR() { timerWOLIntervalExpired = true; }

void setup() {
  Serial.begin(115200);
  NetworkHandler::Setup();

  timerDisplay = timerBegin(0, 80, true);  // Timer 0, clock divider 80
  timerAttachInterrupt(timerDisplay, &timerDisplayISR,
                       true);  // Attach the interrupt handling function
  timerAlarmWrite(timerDisplay, DISPLAY_INTERVAL * (uint64_t)1000000,
                  true);           // Interrupt every 1 second
  timerAlarmEnable(timerDisplay);  // Enable the alarm

  timerWOLInterval = timerBegin(1, 80, true);  // Timer 0, clock divider 80
  timerAttachInterrupt(timerWOLInterval, &timerWOLIntervalISR,
                       true);  // Attach the interrupt handling function
  timerAlarmWrite(timerWOLInterval, WOL_STARTUP * 60 * (uint64_t)1000000,
                  true);               // Interrupt after 60 minutes
  timerAlarmEnable(timerWOLInterval);  // Enable the alarm

#ifdef USE_I2C_DISPLAY
  // Display
  Wire.begin(I2C_SDA, I2C_SCL, I2C_SPEED);
  u8g2.setI2CAddress(SSD1315_ADDR);
  u8g2.begin();
  upButton.begin();
  downButton.begin();
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

  if (timerDisplayExpired) {
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
    timerDisplayExpired = false;
  }

  if (timerWOLIntervalExpired) {
    if (false == startupWOLSent) {
      // Reset Timer to new interval
      timerAlarmWrite(timerWOLInterval, WOL_INTERVAL * 60 * (uint64_t)1000000,
                      true);  // Interrupt after 5 minutes
      Serial.println("Sending frist WOL after startup wait.");
      startupWOLSent = true;
    } else {
      Serial.println("Sending WOL packets");
    }
    time(&wolEpoche);
    wolEpoche += WOL_INTERVAL * 60;
    NetworkHandler::SetNextWolTime(localtime(&wolEpoche));
    Serial.print("Next WOL: ");
    Serial.println(NetworkHandler::GetNextWolTime().c_str());
    NetworkHandler::SendWol();
    timerWOLIntervalExpired = false;
  }

#ifdef USE_I2C_DISPLAY
  if (downButton.pressed()) {
    // button is pressed
    Serial.println("Down button Pressed");
  }
  if (upButton.pressed()) {
    // button is pressed
    Serial.println("Up button Pressed");
  }

#endif
}
