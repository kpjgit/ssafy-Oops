#pragma once

#include <Arduino.h>

class EncoderMotor{
  private:
    uint32_t _pinPWM, _pinDIR, _pinEncA, _pinEncB;
    long     _countsPerRev;     // 1회전당 엔코더 카운트 수
    uint8_t  _pwmCh;            // pwm 채널 (v3에선 미사용, 보존)
    uint32_t _pwmFreq;          // pwm 주파수
    uint8_t  _pwmResBits;       // pwm 해상도

    volatile long _count = 0;   // 엔코더 누적 카운트 값
    volatile int  _lastEncoded = 0; // 엔코더 이전 상태 (방향 판단용)

    void IRAM_ATTR isrHandle();

    // ISR 트램펄린 (attachInterruptArg 용) - 클래스명 맞춤
    static void IRAM_ATTR _isrThunk(void* arg) {
      static_cast<EncoderMotor*>(arg)->isrHandle();
    }

  public:
    EncoderMotor(uint8_t pinPWM, uint8_t pinDIR,
                 uint8_t pinEncA, uint8_t pinEncB,
                 long countsPerRev = 8253,
                 uint8_t pwmChannel = 0,
                 uint32_t pwmFreqHz = 20000,
                 uint8_t pwmResBits = 10)
    : _pinPWM(pinPWM), _pinDIR(pinDIR),
      _pinEncA(pinEncA), _pinEncB(pinEncB),
      _countsPerRev(countsPerRev),
      _pwmCh(pwmChannel), _pwmFreq(pwmFreqHz), _pwmResBits(pwmResBits) {}

    void begin();

    inline void setDirCW(bool cw) { digitalWrite(_pinDIR, cw ? LOW : HIGH); } // LOW=CW
    void setDutyRaw(uint32_t duty);
    void setDutyPercent(uint8_t percent);  // 0 ~ 100
    inline uint32_t maxDuty() const { return (1u << _pwmResBits) - 1; }
    inline void stop() { setDutyRaw(0); }
    
    // 상태
    inline long getCount() const { return _count; }
    inline void resetCount(long v = 0) { _count = v; }
    inline long getCountsPerRev() const { return _countsPerRev; }
    float getDegrees() const; // 각도 반환

    // 블로킹 회전(CW 고정). decelWindowCounts>0이면 남은 카운트에 따라 선형 감속
    void moveRevolutionsCW(float rev,
                           uint32_t duty = 512,
                           long decelWindowCounts = 0,
                           uint32_t minDuty = 64);

    void moveCountsCW(long counts,
                      uint32_t duty = 512,
                      long decelWindowCounts = 0,
                      uint32_t minDuty = 64);

        // 각도(°) → 카운트 변환 (부호 유지, 반올림)
    long degreesToCounts(float deg) const;

    // 상대 각도 회전: +deg = CW, -deg = CCW
    void moveDegrees(float deg,
                    uint32_t duty = 512,
                    long decelWindowCounts = 0,
                    uint32_t minDuty = 64);
};
