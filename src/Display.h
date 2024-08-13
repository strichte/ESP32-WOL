
#ifndef SRC_DISPLAY_H_
#define SRC_DISPLAY_H_

#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#define I2C_SDA     14
#define I2C_SCL     15
#define SSD1315_ADDR 0x78
#define I2C_SPEED 400E3

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);  // High speed I2C

#endif /* SRC_DISPLAY_H_ */