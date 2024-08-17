/*
 *
 * main.h
 *
 * Created on: 2024-08-16
 *
 *--------------------------------------------------------------
 * Copyright (c) 2024 SXR.
 * This project is licensed under the MIT License.
 * The MIT license can be found in the project root and at
 * https://opensource.org/licenses/MIT.
 *--------------------------------------------------------------
 */


#ifndef SRC_MAIN_H_
#define SRC_MAIN_H_

#include "utilities.h"
#include <FS.h>
#include <SPI.h>
#include <SD.h>

#ifdef ESP32
void setup();
void loop();
#endif

class main {
public:
};

#endif // SRC_MAIN_H_ 
