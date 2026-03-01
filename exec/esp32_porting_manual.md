# ESP32 모터 컨트롤러 포팅 매뉴얼

EncoderMotor 프로젝트는 ESP32 + DRV8871(또는 동급 H-bridge) 조합으로 턴테이블을 제어하며, 라즈베리파이 스캐너와 Bluetooth SPP로 통신합니다. 아래 절차는 Arduino IDE에서 펌웨어를 빌드·업로드하고 현장 장비와 연동하는 방법을 정리한 것입니다.

## 1. 하드웨어 개요
| 구성 | 설명 |
| --- | --- |
| ESP32 DevKitC / WROOM 계열 | Bluetooth Classic(SPP) 지원 필수 |
| DRV8871 보드 | IN1=PWM, IN2=DIR 연결 |
| 증분형 엔코더 모터 | A/B 채널을 ESP32 GPIO 21/22에 연결 |
| 전원 | 모터용 외부 전원, ESP32는 5V USB 또는 5V Regulator |

### 1.1 기본 핀 매핑 (`encoderMotorControl.ino`)
| 기능 | ESP32 핀 | 비고 |
| --- | --- | --- |
| PWM (DRV8871 IN1) | GPIO 18 | `PIN_PWM` |
| DIR  (DRV8871 IN2) | GPIO 19 | `PIN_DIR` |
| Encoder A | GPIO 21 | `PIN_ENC_A` |
| Encoder B | GPIO 22 | `PIN_ENC_B` |

필요에 따라 `.ino` 파일 상단의 PIN 상수를 변경해도 되지만, ESP32의 PWM 채널/인터럽트 사용 가능 핀을 확인해야 합니다.

## 2. 개발 환경 준비 (Arduino IDE)
1. Arduino IDE 2.x 설치.
2. **ESP32 Board Manager** 등록  
   - *Preferences* → *Additional Boards Manager URLs*에 `https://espressif.github.io/arduino-esp32/package_esp32_index.json` 추가.
3. **보드 설치**  
   - *Tools → Board → Boards Manager* 에서 “ESP32 by Espressif” 설치.
4. **라이브러리**  
   - `BluetoothSerial`은 ESP32 코어에 포함되어 별도 설치가 필요 없습니다.
5. **포트/보드 설정**  
   - Board: `ESP32 Dev Module` (실제 사용 모듈에 맞게 선택)  
   - Flash Mode: `QIO`, Partition Scheme: `Default 4MB with spiffs`(기본값 사용)  
   - Upload Speed: 921600 (안정성 문제 시 115200)  
   - Tools → Port에서 ESP32 장치를 선택합니다.

## 3. 프로젝트 열기 및 설정
1. `EncoderMotor/encoderMotorControl/encoderMotorControl.ino`를 Arduino IDE로 열면 `.cpp/.hpp`가 자동으로 함께 로드됩니다.
2. 필요한 매개변수를 수정
   - `device_name`: Bluetooth 페어링 시 노출되는 명칭.
   - `countsPerRev`: 모터+감속기 조합에 맞춰 `EncoderMotor` 생성자 인자를 조정.
   - `motor.moveDegrees` 호출 시 Duty/감속 파라미터를 현장 상황에 맞게 조절.
3. ESP32에 업로드(CTRL+U). 업로드 중에는 모터 전원을 분리하거나 브레이크를 잡아 두는 것이 안전합니다.

## 4. 동작 방식
- ESP32는 Bluetooth SPP를 통해 라즈베리파이(`scanner_service`)와 직렬 문자열을 주고받습니다.
- 입력: 개행 문자로 끝나는 각도(°) 문자열. 예: `"360\n"`.
- 동작: `moveDegrees(deg, duty=614, decelWindow=2000, minDuty=64)` 호출로 시계방향 회전.
- 응답: 완료 후 `"OK\n"` 문자열 송신.

전원 투입 직후 `SerialBT.begin(device_name)`로 SPP 서비스가 노출되므로, 라즈베리파이에서 `bluetoothctl`로 페어링 후 `/dev/rfcomm*`에 바인딩하면 됩니다.

## 5. 페어링 및 테스트
1. ESP32 전원을 켠 뒤, 라즈베리파이/노트북에서 `bluetoothctl`을 사용해 검색 → 페어링 → 신뢰(Trust) 설정.
2. (선택) `rfcomm connect`로 직렬 포트를 생성해 수동 테스트:
   ```bash
   sudo rfcomm connect /dev/rfcomm0 <ESP32_MAC> 1
   echo "90" | sudo tee /dev/rfcomm0
   ```
3. Arduino IDE의 Serial Monitor 또는 Bluetooth SPP(모바일 앱 등)로 접속해 `30`, `-30`과 같은 값을 전송하면 턴테이블이 회전하며, 완료 시 `OK`가 출력돼야 합니다.

## 6. 라즈베리파이 연동
- `depth_cam/include/motor.h`에 ESP32의 MAC/채널을 반영합니다.
- `footscan` 실행 시 ESP32가 자동으로 연결되지 않으면, OS 레벨에서 `rfcomm`를 영구 매핑하거나 `scanner_service` 실행 전 스크립트로 연결을 재시도합니다.

## 7. 문제 해결
| 증상 | 확인 항목 |
| --- | --- |
| 업로드 실패 (`Failed to connect to ESP32`) | 부트/EN 버튼을 누른 채 업로드 시도, 데이터 케이블 확인 |
| Bluetooth에 기기가 보이지 않음 | `device_name` 중복 여부, ESP32 리셋, 전원 공급 확인 |
| 모터가 과도하게 빠르게 회전하거나 멈춤 | `countsPerRev`, Duty, 감속 파라미터 재조정 |
| 엔코더 카운트가 0으로 고정 | A/B 선결선/접지 확인, `attachInterrupt` 핀 변경 필요 여부 체크 |

