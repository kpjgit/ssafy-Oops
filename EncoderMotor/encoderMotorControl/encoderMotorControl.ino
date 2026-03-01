#include <BluetoothSerial.h>

#include "EncoderMotor.hpp"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

const int PIN_PWM   = 18;  // DRV8871 IN1 (PWM)
const int PIN_DIR   = 19;  // DRV8871 IN2 (DIR)
const int PIN_ENC_A = 21;  // Encoder A
const int PIN_ENC_B = 22;  // Encoder B

String device_name = "ESP32-MOTOR"; 

BluetoothSerial SerialBT;

EncoderMotor motor(PIN_PWM, PIN_DIR, PIN_ENC_A, PIN_ENC_B,
/*countsPerRev=*/8253L*159/13,
/*pwmChannel=*/0, /*pwmFreqHz=*/20000, /*pwmResBits=*/10);

void setup() {
  Serial.begin(115200);
  motor.begin();
  motor.resetCount(0);

  SerialBT.begin(device_name);

  delay(1500);

  // Serial.println("CW 1 rev with linear decel from 6000...");
  // Serial.printf("count=%ld\n", motor.getCount());

  // motor.moveDegrees(360.0f, 512, 2000, 64);
}

void loop() {
  /*
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    float deg = line.toFloat();  // 숫자만 받음
    motor.moveDegrees(deg, 512, 2000, 64);  // 회전 수행
    Serial.println("OK");       // 완료 신호 전송
  }
  */

  if(SerialBT.available()){
    String line = SerialBT.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    float deg = line.toFloat();  // 숫자만 받음
    motor.moveDegrees(deg, 614, 2000, 64);  // 회전 수행
    SerialBT.println("OK");       // 완료 신호 전송

  }

}
