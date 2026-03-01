#include "EncoderMotor.hpp"
#include <math.h>

// 남은 카운트에 비례한 선형 감속
static inline uint32_t scaleDutyByRemain(uint32_t baseDuty, long rem, long decelWin, uint32_t minDuty, uint32_t maxDuty) {
  if (decelWin <= 0 || rem >= decelWin) return baseDuty;
  if (rem <= 0) return 0;
  uint32_t d = (uint32_t)((float)baseDuty * ((float)rem / (float)decelWin));
  if (d < minDuty) d = minDuty;
  if (d > maxDuty) d = maxDuty;
  return d;
}

void EncoderMotor::begin() {
  // 방향 핀
  pinMode(_pinDIR, OUTPUT);
  digitalWrite(_pinDIR, LOW); // 기본 CW

  // LEDC 핀 기반 API (v3.x): 채널 자동 할당
  ledcAttach(_pinPWM, _pwmFreq, _pwmResBits);
  ledcWrite(_pinPWM, 0);

  // 엔코더 입력
  pinMode(_pinEncA, INPUT_PULLUP);
  pinMode(_pinEncB, INPUT_PULLUP);

  int msb = digitalRead(_pinEncA);
  int lsb = digitalRead(_pinEncB);
  _lastEncoded = ((msb & 1) << 1) | (lsb & 1);

  // 인스턴스별 인터럽트 등록
  attachInterruptArg(digitalPinToInterrupt(_pinEncA), _isrThunk, this, CHANGE);
  attachInterruptArg(digitalPinToInterrupt(_pinEncB), _isrThunk, this, CHANGE);
}

void EncoderMotor::setDutyRaw(uint32_t duty) {
  uint32_t md = maxDuty();
  if (duty > md) duty = md;
  ledcWrite(_pinPWM, duty);
}

void EncoderMotor::setDutyPercent(uint8_t percent) {
  if (percent > 100) percent = 100;
  uint32_t duty = (uint32_t)((maxDuty() * (uint32_t)percent) / 100u);
  setDutyRaw(duty);
}

float EncoderMotor::getDegrees() const {
  if (_countsPerRev == 0) return 0.0f;  // 0 division 방지
  long modCount = _count % _countsPerRev;
  if (modCount < 0) modCount += _countsPerRev;  // 음수 보정 (CCW 회전 시)
  return (float)modCount * (360.0f / (float)_countsPerRev);
}


void IRAM_ATTR EncoderMotor::isrHandle() {
  // 주신 쿼드러처 테이블 그대로
  int MSB = digitalRead(_pinEncA);
  int LSB = digitalRead(_pinEncB);
  int encoded = (MSB << 1) | LSB;
  int sum = (_lastEncoded << 2) | encoded;

  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) _count++;
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) _count--;

  _lastEncoded = encoded;
}

void EncoderMotor::moveCountsCW(long counts,
                                uint32_t duty,
                                long decelWindowCounts,
                                uint32_t minDuty) {
  if (counts <= 0) return;

  long start  = _count;
  long target = start + counts;

  setDirCW(true);     // CW (DIR=LOW)
  setDutyRaw(duty);

  while (_count < target) {
    if (decelWindowCounts > 0) {
      long remain = target - _count;
      uint32_t d2 = scaleDutyByRemain(duty, remain, decelWindowCounts, minDuty, maxDuty());
      ledcWrite(_pinPWM, d2);
    }
    delay(0); // yield
  }

  // 코스트 정지 (IN1=PWM=0, IN2=LOW)
  setDutyRaw(0);
  digitalWrite(_pinDIR, LOW);
}

void EncoderMotor::moveRevolutionsCW(float rev,
                                     uint32_t duty,
                                     long decelWindowCounts,
                                     uint32_t minDuty) {
  if (rev <= 0.f) return;
  long counts = (long)(_countsPerRev * rev + 0.5f);
  moveCountsCW(counts, duty, decelWindowCounts, minDuty);
}

long EncoderMotor::degreesToCounts(float deg) const {
  // 1 rev = _countsPerRev counts, 360° 기준
  double counts = (double)_countsPerRev * ((double)deg / 360.0);
  // 가장 가까운 정수로 반올림
  long ci = (long)llround(counts);
  return ci;
}

void EncoderMotor::moveDegrees(float deg,
                               uint32_t duty,
                               long decelWindowCounts,
                               uint32_t minDuty)
{
  // 0이면 할 일 없음
  if (deg == 0.0f) return;

  // 회전 방향/목표 카운트 계산
  const bool cw = (deg > 0.0f);
  long delta = degreesToCounts(deg);
  if (delta == 0) return; // 변환 후 0이면 종료

  long start  = _count;
  long target = start + delta; // cw면 증가, ccw면 감소(delta<0)

  // 방향/듀티 설정
  setDirCW(cw);          // CW이면 DIR=LOW, CCW이면 DIR=HIGH
  setDutyRaw(duty);

  // 메인 루프: 목표에 도달할 때까지
  while (true) {
    long remain = labs(target - _count);

    // 감속 창이 지정되어 있으면 선형 감속
    if (decelWindowCounts > 0) {
      uint32_t d2 = scaleDutyByRemain(
          duty, remain, decelWindowCounts, minDuty, maxDuty());
      ledcWrite(_pinPWM, d2);
    }

    // 도달 판정: 방향별로 비교 연산자만 다름
    if (cw) {
      if (_count >= target) break;
    } else {
      if (_count <= target) break;
    }

    delay(0); // yield
  }

  // 코스트 정지 (IN1=0, IN2=LOW로 통일)
  setDutyRaw(0);
  digitalWrite(_pinDIR, LOW);
}

