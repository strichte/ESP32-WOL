#ifndef SRC_TIMER_H_
#define SRC_TIMER_H_

#include <Arduino.h>

volatile bool timer0_expired = false;
volatile bool timer1_expired = false;
volatile bool timer2_expired = false;
volatile bool timer3_expired = false;

void IRAM_ATTR Timer0Isr() { timer0_expired = true; }
void IRAM_ATTR Timer1Isr() { timer1_expired = true; }
void IRAM_ATTR Timer2Isr() { timer2_expired = true; }
void IRAM_ATTR Timer3Isr() { timer3_expired = true; }

class Timer {
 public:
  Timer(uint8_t timer, uint64_t duration)
      : timer_num_(timer), duration_(duration) {
    timer_ = timerBegin(timer, 80, true);
    switch (timer_num_) {
      case 0:
        timerAttachInterrupt(timer_, &Timer0Isr, true);
        break;
      case 1:
        timerAttachInterrupt(timer_, &Timer1Isr, true);
        break;
      case 2:
        timerAttachInterrupt(timer_, &Timer2Isr, true);
        break;
      case 3:
        timerAttachInterrupt(timer_, &Timer3Isr, true);
        break;
    }
    timerAlarmWrite(timer_, duration * (uint64_t)1000000, true);
  }
  void Enable() { timerAlarmEnable(timer_); }
  void SetTimer(uint8_t duration) {
    timerAlarmWrite(timer_, duration * (uint64_t)1000000, true);
  }
  double ReadSeconds() { return timerReadSeconds(timer_); }
  uint64_t DurationSeconds() { return duration_; }
  uint64_t TimeLeftSeconds() { return DurationSeconds() - ReadSeconds(); }
  float PercentRemaining() { return 100.0F / (float) DurationSeconds() * TimeLeftSeconds(); }
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

  void Reset() {
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

 private:
  hw_timer_t *timer_;
  uint8_t timer_num_;
  uint64_t duration_;
};

#endif  // SRC_TIMER_H_