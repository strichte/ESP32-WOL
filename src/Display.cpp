/*
 *
 * Display.cpp
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

#ifdef USE_I2C_DISPLAY
#include <Display.h>

void I2CDisplay::Setup() {
  Wire.begin(I2C_SDA, I2C_SCL, I2C_SPEED);
  display_.setI2CAddress(SSD1315_ADDR);
  display_.begin();
  display_.setFont(u8g2_font_siji_t_6x10);
}

void I2CDisplay::DrawHeader() {
  display_.drawStr(0, 8,
                   ("IP: " + ETH.localIP().toString())
                       .c_str());  // write something to the internal memory
  // display_.drawGlyph(118, 8, 0xe043);       // Network
  if (!NetworkHandler::FirstWolSent()) {
    display_.drawGlyph(122, 8, '1');  // NTP
  }
  display_.drawBox(0, 10, 128 / 100.0F * (*timer_wol_ptr_).PercentRemaining(),
                   4);  // progress bar
}

void I2CDisplay::UpdateStatusPage() {
  display_.clearBuffer();
  DrawHeader();
  display_.drawStr(
      0, 24,
      std::string("Next WOL : " + NetworkHandler::GetNextWolTime(date_only))
          .c_str());
  display_.drawStr(
      0, 32,
      std::string("           " + NetworkHandler::GetNextWolTime(time_only))
          .c_str());

  display_.drawStr(
      0, 40,
      std::string("Cur. Time: " + NetworkHandler::GetTime(date_only)).c_str());
  display_.drawStr(
      0, 48,
      std::string("           " + NetworkHandler::GetTime(time_only)).c_str());
  display_.drawStr(
      0, 56,
      std::string("Boot Time: " + NetworkHandler::GetUptime(date_only))
          .c_str());
  display_.drawStr(
      0, 64,
      std::string("           " + NetworkHandler::GetUptime(time_only))
          .c_str());
  current_page_ = status;
  display_.sendBuffer();
}

void I2CDisplay::UpdateNetworkPage() {
  display_.clearBuffer();
  DrawHeader();

  display_.drawStr(
      0, 24,
      std::string(std::string("GW:   ") + kGateway.toString().c_str()).c_str());
  display_.drawStr(
      0, 32,
      std::string(std::string("NM:   ") + kSubnet.toString().c_str()).c_str());
  display_.drawStr(
      0, 40,
      std::string(std::string("DNS:  ") + kDnsServer.toString().c_str())
          .c_str());
  display_.drawStr(0, 48,
                   std::string(std::string("NTP1: ") + kNtpServer1).c_str());
  display_.drawStr(0, 56,
                   std::string(std::string("NTP2: ") + kNtpServer2).c_str());
  display_.drawStr(0, 64, kTimeZone);
  current_page_ = network;
  display_.sendBuffer();
}

void I2CDisplay::UpdateDevicePage(){
  uint size=NetworkHandler::GetWolDevices().size(),
    start = 3 * current_device_page_,
    end = (start + 3) > size ? size : start + 3;
  DrawWolDevice(start, end);
}

bool I2CDisplay::PreviousWolDevicesPage() {
  bool had_previous = false;
  uint size = NetworkHandler::GetWolDevices().size();
  if (current_page_ != devices) {
    uint num_pages = std::ceil(size/3.0F),
      start = (num_pages - 1) * 3,
      end = size < (num_pages * 3) ? size : num_pages * 3;
    DrawWolDevice(start, end);
    current_device_page_ = num_pages - 1;
    had_previous = true;
  } else {
    // already showing the a devive page
    uint start = 3 * (current_device_page_ - 1);
    if (start < size) {
      // we have more than one device page
      uint end = (start + 3) > size ? size : start + 3;
      DrawWolDevice(start, end);
      current_device_page_--;
      had_previous = true;
    }
  }
  return had_previous;
}

bool I2CDisplay::NextWolDevicesPage() {
  bool had_next = false;
  uint size = NetworkHandler::GetWolDevices().size();
  if (current_page_ != devices) {
    DrawWolDevice(0, size>3? 3 : size);
    current_device_page_ = 0;
    had_next = true;
  } else {
    // already showing the 1st devive page
    uint start = 3 * (1 + current_device_page_);
    if (start < size) {
      // we have more than one device page
      uint end = start + 3;
      if (end > size) {
        end = size;
      }  // not a full page
      DrawWolDevice(start, end);
      current_device_page_++;
      had_next = true;
    }
  }
  return had_next;
}

void I2CDisplay::DisplayCurrentPage() {
  switch (current_page_) {
    case status:
      UpdateStatusPage();
      break;
    case devices:
      UpdateDevicePage();
      break;
    case network:
      UpdateNetworkPage();
  }
}

bool I2CDisplay::DisplayPreviousPage() {
  bool is_status_page = false;
  switch (current_page_) {
    case status:
      UpdateNetworkPage();
      break;
    case network:
      PreviousWolDevicesPage();
      break;
    case devices:
      if (!PreviousWolDevicesPage()) {
        UpdateStatusPage();
        is_status_page =true;
      }
      break;
  }
  return is_status_page;
}

bool I2CDisplay::DisplayNextPage() {
  bool is_status_page = false;
  switch (current_page_) {
    case status:
      NextWolDevicesPage();
      break;
    case devices:
      if (!NextWolDevicesPage()){UpdateNetworkPage();}
      break;
    default:
      UpdateStatusPage();
      is_status_page = true;
  }
  return is_status_page;
}

void I2CDisplay::DrawWolDevice(const uint &start, const uint &end) {
  display_.clearBuffer();
  DrawHeader();
  for (uint i=start, pos=0; i < end; i++, pos++) {
    display_.drawStr(0, 24 + 16 * pos,
                     std::string(NetworkHandler::GetWolDevices()[i].name + ":").c_str());
    display_.drawStr(0, 32 + 16 * pos,
                     std::string("   " + NetworkHandler::GetWolDevices()[i].mac).c_str());
  }
  display_.sendBuffer();
  current_page_ = devices;
}

#endif  // USE_I2C_DISPLAY

