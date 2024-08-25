#ifndef SRC_TIMER_H_
#define SRC_TIMER_H_

/*
 *
 * Timer.h
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

#include <Arduino.h>

static volatile bool timer0_expired;
static volatile bool timer1_expired;
static volatile bool timer2_expired;
static volatile bool timer3_expired;

static void IRAM_ATTR Timer0Isr() { timer0_expired = true; }
static void IRAM_ATTR Timer1Isr() { timer1_expired = true; }
static void IRAM_ATTR Timer2Isr() { timer2_expired = true; }
static void IRAM_ATTR Timer3Isr() { timer3_expired = true; }

enum ESP32Timer_t {
  timer0=0,
  timer1=1,
  timer2=2,
  timer3=3
};

class Timer {
 public:
  Timer(ESP32Timer_t timer)
      : timer_num_(timer) {
    timer_ = timerBegin(timer, 80, true);
    switch (timer_num_) {
      case 0:
        timerAttachInterrupt(timer_, &Timer0Isr, true);
        timer0_expired = false;
        break;
      case 1:
        timerAttachInterrupt(timer_, &Timer1Isr, true);
        timer1_expired = false;
        break;
      case 2:
        timerAttachInterrupt(timer_, &Timer2Isr, true);
        timer2_expired = false;
        break;
      case 3:
        timerAttachInterrupt(timer_, &Timer3Isr, true);
        timer3_expired = false;
        break;
    }
  }
  void Enable(uint32_t duration) { 
      duration_=duration;
      timerAlarmWrite(timer_, duration_ * (uint64_t)1000000, true);
      timerAlarmEnable(timer_); 
  }

  void SetTimer(uint32_t duration) {
    duration_ = duration;
    timerAlarmWrite(timer_, duration_ * (uint64_t)1000000, true);
  }
  double ReadSeconds() { return timerReadSeconds(timer_); }
  uint64_t DurationSeconds() { return duration_; }
  uint64_t TimeLeftSeconds() { return DurationSeconds() - ReadSeconds(); }
  float PercentRemaining() {
    return 100.0F / (float)DurationSeconds() * TimeLeftSeconds();
  }
  bool IsExpired() {
    switch (timer_num_) {
      case 0:
        return timer0_expired;
      case 1:
        return timer1_expired;
      case 2:
        return timer2_expired;
      case 3:
        return timer3_expired;
    }
    return false;
  }

  void Clear() {
    switch (timer_num_) {
      case 0:
        timer0_expired = false;
      case 1:
        timer1_expired = false;
      case 2:
        timer2_expired = false;
      case 3:
        timer3_expired = false;
    }
  }

  void Restart() { timerRestart(timer_); }

 private:
  hw_timer_t *timer_;
  uint8_t timer_num_;
  uint32_t duration_;
};

#endif  // SRC_TIMER_H_