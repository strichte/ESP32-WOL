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

#include "NetworkHandler.h"
#include "Timer.h"
#include "esp_sntp.h"

Timer timer_display(timer0, DISPLAY_INTERVAL);
Timer timer_wol(timer1, WOL_STARTUP * 60);

#ifdef USE_I2C_DISPLAY
I2CDisplay display(&timer_wol);
#endif

// Testing
File myFile;
void printDirectory(File dir, int numTabs);

void setup() {
  Serial.begin(115200);

  NetworkHandler::Setup();
#ifdef USE_I2C_DISPLAY
  display.Setup();
  // display_pages.push_back(&I2CDisplay::UpdateStatusPage);
#endif  // USE_I2C_DISPLAY
  timer_display.Enable();
  timer_wol.Enable();

  // TESTING
  pinMode(SD_MISO_PIN, INPUT_PULLUP);
  SPI.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN);
  Serial.print("\nInitializing SD card...");
  // we'll use the initialization code from the utility libraries
  // since we're just testing if the card is working!

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("initialization failed. Things to check:");
    Serial.println("* is a card inserted?");
    Serial.println("* is your wiring correct?");
    Serial.println(
        "* did you change the chipSelect pin to match your shield or module?");
    while (1);
  } else {
    Serial.println("Wiring is correct and a card is present.");
  }

  // print the type of card
  Serial.println();
  Serial.print("Card type:         ");
  switch (SD.cardType()) {
    case CARD_SD:
      Serial.println("SD");
      break;
    case CARD_MMC:
      Serial.println("MMC");
      break;
    case CARD_SDHC:
      Serial.println("SDHC");
      break;
    case CARD_NONE:
      Serial.println("No card attached");
      break;
    default:
      Serial.println("Unknown");
  }

  // Print size of card
  uint32_t cardSize = SD.cardSize() / (1024 * 1024);
  String str = "SDCard Size: " + String(cardSize) + "MB";
  Serial.println(str);

  myFile = SD.open("/");
  printDirectory(myFile, 0);
  myFile.close();

  Serial.println("");
  // open a new file and immediately close it:
  Serial.println("Creating helloworld.txt...");
  myFile = SD.open("/helloworld.txt", FILE_WRITE);
  // Check to see if the file exists:
  if (SD.exists("/helloworld.txt")) {
    Serial.println("helloworld.txt exists.");
  } else {
    Serial.println("helloworld.txt doesn't exist.");
  }
  // delete the file:
  Serial.println("Removing helloworld.txt...");
  SD.remove("/helloworld.txt");
  if (SD.exists("/helloworld.txt")) {
    Serial.println("helloworld.txt exists.");
  } else {
    Serial.println("helloworld.txt doesn't exist.");
  }
  myFile.close();
  Serial.println("");

  // Open a file. Note that only one file can be open at a time,
  // so you have to close this one before opening another.
  myFile = SD.open("/test.txt", FILE_WRITE);
  // if the file opened okay, write to it.
  if (myFile) {
    Serial.print("Writing to test.txt...");
    myFile.println("testing 1, 2, 3.");

    // close the file:
    myFile.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error.
    Serial.println("error opening test.txt");
  }
  // Re-open the file for reading.
  myFile = SD.open("/test.txt");
  if (myFile) {
    Serial.println("test.txt:");
    // Read from the file until there's nothing else in it.
    while (myFile.available()) {
      Serial.write(myFile.read());
    }

    // Close the file.
    myFile.close();
  } else {
    // If the file didn't open, print an error.
    Serial.println("error opening test.txt");
  }
  myFile.close();
  SD.end();
  SPI.end();
  Serial.println("INFO: Setup complete");
}

void loop() {
  static bool first_run = true;
  static time_t wol_epoche;
  NetworkHandler::Loop();

  if (first_run) {
    time(&wol_epoche);
    wol_epoche += WOL_STARTUP * 60;
    NetworkHandler::SetNextWolTime(wol_epoche);
    Serial.print("first run wol_epoche=");
    Serial.println(wol_epoche);
    first_run = false;
  }

  if (timer_display.IsExpired()) {
#ifdef USE_I2C_DISPLAY
    display.DisplayCurrentPage();
#else   // USE_I2C_DISPLAY
    Serial.print("\rLast Boot: ");
    Serial.print(NetworkHandler::getUptime().c_str());
#endif  // USE_I2C_DISPLAY
    timer_display.Clear();
  }

  if (timer_wol.IsExpired()) {
    if (!NetworkHandler::FirstWolSent()) {
      // Reset Timer to new interval
      timer_wol.SetTimer(WOL_INTERVAL * 60);
      timer_wol.Restart();
      Serial.println("Sending first WOL after startup wait.");
    } else {
      Serial.println("Sending WOL");
    }
    time(&wol_epoche);
    wol_epoche += WOL_INTERVAL * 60;
    NetworkHandler::SetNextWolTime(wol_epoche);
    Serial.print("Next run wol_epoche=");
    Serial.println(wol_epoche);
    Serial.print("Next WOL: ");
    Serial.println(NetworkHandler::GetNextWolTime().c_str());
    NetworkHandler::SendWol();
    timer_wol.Clear();
  }

#ifdef USE_I2C_DISPLAY

  if (display.ButtonUpPressed()) {
    Serial.println("Up button Pressed");
    display.DisplayPreviousPage();
  }
  if (display.ButtonDownPressed()) {
    Serial.println("Down button Pressed");
    display.DisplayNextPage();
  }
  if (display.ButtonHashPressed()) {
    Serial.println("# button Pressed");
  }
  if (display.ButtonStarPressed()) {
    time(&wol_epoche);
    wol_epoche += WOL_INTERVAL * 60;
    NetworkHandler::SetNextWolTime(wol_epoche);
    Serial.print("Next run wol_epoche=");
    Serial.println(wol_epoche);
    Serial.print("Next WOL: ");
    Serial.println(NetworkHandler::GetNextWolTime().c_str());
    timer_wol.Clear();
    timer_wol.SetTimer(WOL_INTERVAL * 60);
    timer_wol.Restart();
    NetworkHandler::SendWol();
  }

#endif  // USE_I2C_DISPLAY
}

// TESTING
// printDirectory
void printDirectory(File dir, int numTabs) {
  while (true) {
    File entry = dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }
    for (uint8_t i = 0; i < numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs + 1);
    } else {
      // Files have sizes, directories do not.
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}