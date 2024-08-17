#ifndef SRC_DISPLAY_H_
#define SRC_DISPLAY_H_

/*
 *
 * Display.h
 *
 * Created on: 2024-08-15
 *
 *--------------------------------------------------------------
 * Copyright (c) 2024 SXR.
 * This project is licensed under the MIT License.
 * The MIT license can be found in the project root and at
 * https://opensource.org/licenses/MIT.
 *--------------------------------------------------------------
 */

/*
Class for 128x64 OLED I2C display with 4 buttons from
https://www.aliexpress.com/item/1005006322621064.html?spm=a2g0o.order_list.order_list_main.58.5bbf1802eNcJxk
*/

#include <vector>

#include <Button.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "NetworkHandler.h"
#include "Timer.h"

#define I2C_SDA 16
#define I2C_SCL 32
#define SSD1315_ADDR 0x78
#define I2C_SPEED 400E3
#define BUTTON_UP 36
#define BUTTON_DOWN 39
#define BUTTON_HASH 34
#define BUTTON_STAR 35

// Macro to invoke member function from pointer
#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))

class I2CDisplay {
 public:
  I2CDisplay(Timer *const t)
      : button_up_(BUTTON_UP),
        button_down_(BUTTON_DOWN),
        button_hash_(BUTTON_HASH),
        button_star_(BUTTON_STAR),
        timer_wol_ptr_(t),
        display_(U8G2_R0),
        current_page_(status),
        current_device_page_(0) {
    button_up_.begin();
    button_down_.begin();
    button_hash_.begin();
    button_star_.begin();
  }
  void Setup();
  void DrawHeader();
  void UpdateStatusPage();
  void UpdateNetworkPage();
  void UpdateDevicePage();
  bool PreviousWolDevicesPage();
  bool NextWolDevicesPage();
  void DisplayCurrentPage();
  bool DisplayPreviousPage();
  bool DisplayNextPage();
  bool ButtonUpPressed() { return button_up_.pressed(); }
  bool ButtonDownPressed() { return button_down_.pressed(); }
  bool ButtonHashPressed() { return button_hash_.pressed(); }
  bool ButtonStarPressed() { return button_star_.pressed(); }

 private:
  void DrawWolDevice(const uint &start, const uint &end);

  Button button_up_;
  Button button_down_;
  Button button_hash_;
  Button button_star_;
  Timer *const timer_wol_ptr_;
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C display_;
  enum Pages{
    status,
    devices,
    network
  };
  Pages current_page_;
  uint current_device_page_;
};

// I2CDisplay class Member function pointer type
//typedef void (I2CDisplay::*I2CDisplayMemFn)();

#endif /* SRC_DISPLAY_H_ */