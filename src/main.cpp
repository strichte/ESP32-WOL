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

#include <FS.h>
#include <SD.h>
#define YAML_DISABLE_CJSON       // disable all cJSON functions
#define YAML_DISABLE_ARDUINOJSON // disable all ArduinoJson functions
#include <ArduinoYaml.h>  // Happy with plain YAML for out needs

#include <Button.h>
#include "Display.h"

#include "NetworkHandler.h"
#include "Timer.h"
#include "esp_sntp.h"

Timer timer_display(timer0);
Timer timer_wol(timer1);
I2CDisplay display(&timer_wol);

const char* config_file = "/config.yml";

void setup() {
  Serial.begin(115200);

  display.Setup();

  while(!NetworkHandler::Setup(config_file)) { delay(2000); }

  timer_display.Enable(DISPLAY_INTERVAL);
  timer_wol.Enable(NetworkHandler::Config().wol_startup * 60);
}

void loop() {
  static bool first_run = true;
  static time_t wol_epoche;
  NetworkHandler::Loop();

  if (first_run) {
    time(&wol_epoche);
    wol_epoche += NetworkHandler::Config().wol_startup * 60;
    NetworkHandler::SetNextWolTime(wol_epoche);
    first_run = false;
  }

  if (timer_display.IsExpired()) {
    display.DisplayCurrentPage();
    timer_display.Clear();
  }

  if (timer_wol.IsExpired()) {
    if (!NetworkHandler::FirstWolSent()) {
      // Reset Timer to new interval
      timer_wol.SetTimer(NetworkHandler::Config().wol_repeat * 60);
      timer_wol.Restart();
    }
    time(&wol_epoche);
    wol_epoche += NetworkHandler::Config().wol_repeat * 60;
    NetworkHandler::SetNextWolTime(wol_epoche);
    NetworkHandler::SendWol();
    timer_wol.Clear();
  }

  if (display.ButtonUpPressed()) {
    display.DisplayPreviousPage();
  }
  if (display.ButtonDownPressed()) {
    display.DisplayNextPage();
  }
  if (display.ButtonHashPressed()) {
  }

  if (display.ButtonStarPressed()) {
    time(&wol_epoche);
    wol_epoche += NetworkHandler::Config().wol_repeat * 60;
    NetworkHandler::SetNextWolTime(wol_epoche);
    timer_wol.Clear();
    timer_wol.SetTimer(NetworkHandler::Config().wol_repeat * 60);
    timer_wol.Restart();
    NetworkHandler::SendWol();
  }
}
