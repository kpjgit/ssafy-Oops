# Oops Foot Scanner

Oops는 실시간 족형(발 모양) 스캐너와 모바일 앱을 결합해 “내 발에 맞는 신발” 경험을 제공하는 프로젝트입니다.  
라즈베리파이·RealSense를 이용해 3D 데이터를 캡처하고, EC2 변환 서버에서 GLB로 가공한 뒤 Flutter 앱에서 바로 확인할 수 있습니다.

```
┌──────────────┐     POST /scan         ┌──────────────────┐     POST /convert
│ Flutter App  │ ─────────────────────▶ │ Raspberry Pi      │ ───────────────▶ │ EC2 Mesh Server │
│ (oops_app)   │ ◀─── glb_url polling ─ │ scanner_service   │ ◀─── GLB URL ─── │ mesh_convert    │
└──────────────┘                        │ + footscan binary │                  │ + ply2gltf      │
                                        └──────────────────┘                  └─────────────────┘
```

## Repository Layout
| 경로 | 설명 |
| --- | --- |
| `oops_app/` | Flutter 3.x 앱. 발 측정 탭에서 3D GLB를 보여주고 라즈베리파이에 스캔/상태 조회 요청을 보냅니다. |
| `depth_cam/` | RealSense 기반 C++ 스캐너(`footscan`)와 FastAPI 오케스트레이터(`scanner_service.py`). |
| `backend/` | EC2에서 동작하는 GLB 변환 서버(`mesh_convert_service.py` + `ply2gltf.py`). |
| `EncoderMotor/` | ESP32 턴테이블 제어 펌웨어(Arduino IDE로 빌드). Bluetooth SPP로 Pi와 통신합니다. |
| `exec/` | 앱/라즈베리파이/EC2 포팅 매뉴얼 문서. |

## Quick Start

### 1. Flutter App (`oops_app`)
```bash
cd oops_app
flutter pub get
flutter run -d <device>
```
- `lib/screens/home/main_screen.dart` 상단의 `_scannerBase`를 실제 라즈베리파이 주소로 맞춥니다 (`--dart-define=SCANNER_BASE=...` 확장 권장).
- 앱은 `_FootMeasurementTab`에서 `/scan` 요청을 보내고 `/jobs/{id}`를 주기적으로 조회해 `glb_url`을 주입합니다.

### 2. ESP32 Turntable (`EncoderMotor`)
```bash
cd EncoderMotor/encoderMotorControl
# Arduino IDE에서 encoderMotorControl.ino 열기
# device_name / countsPerRev 조정 후 업로드
```
- Arduino IDE(ESP32 core 설치)에서 `encoderMotorControl.ino`를 열고 필요한 파라미터를 수정합니다.
- ESP32를 PC에 연결해 펌웨어를 업로드하고, Bluetooth SPP로 “device_name”이 노출되는지 확인합니다.
- 라즈베리파이에서 `bluetoothctl`로 페어링 후 `/dev/rfcomm`에 바인딩하면 `footscan`이 각도 명령을 전송할 수 있습니다.

### 3. Raspberry Pi Scanner (`depth_cam`)
```bash
cd depth_cam
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4          # footscan 바이너리 생성
cd ..
python3 -m venv .venv && source .venv/bin/activate
pip install fastapi "uvicorn[standard]" requests
export CONVERT_SERVER_URL="http://<ec2-host>:8080/convert"
uvicorn scanner_service:app --host 0.0.0.0 --port 8000
```
- RealSense SDK(libresalsense), ESP32 모터 펌웨어, Bluetooth MAC정보를 `include/motor.h`에 맞춰 주세요.
- `/scan` 호출 시 `footscan`이 실행되어 `scan_jobs/<job_id>`에 PLY와 `footscan.log`가 저장됩니다.

### 4. EC2 Mesh Convert (`backend`)
```bash
cd backend
python3 -m venv .venv && source .venv/bin/activate
pip install fastapi "uvicorn[standard]" numpy open3d trimesh
export CONVERT_PUBLIC_BASE_URL="https://scanner.example.com"   # 선택
uvicorn mesh_convert_service:app --host 0.0.0.0 --port 8080
```
- `/convert`는 `multipart/form-data`의 `file` 필드로 PLY를 받고 GLB를 생성하여 `/glbs/<job>.glb`로 노출합니다.
- `CORSMiddleware`가 적용되어 있어 Flutter WebView에서도 바로 접근할 수 있습니다.

## Operational Notes
- **환경 변수**
  - `CONVERT_SERVER_URL`: 라즈베리파이에서 EC2 변환 서버 `/convert` 주소를 지정 (예: `http://<ec2>:8080/convert`).
  - `CONVERT_TIMEOUT_SEC`: Pi → EC2 업로드 타임아웃(기본 120초).
  - `CONVERT_PUBLIC_BASE_URL`: 변환 서버가 외부에 알릴 GLB URL 베이스. 역프록시 뒤에 있을 때 활용합니다.
- **스토리지**: `depth_cam/scan_jobs`와 `backend/mesh_jobs/public_glbs`는 주기적으로 정리하거나 S3 등 외부 스토리지로 이전하세요.
- **보안**: 포트 8000/8080은 사내망·VPN에서만 열어두고, 실제 서비스 시 HTTPS 프록시를 권장합니다.

## Documentation
자세한 배포/운영 방법은 `exec/` 폴더의 매뉴얼을 참고하세요.
- `exec/app_porting_manual.md` – Flutter 앱 환경 구성/배포 절차
- `exec/esp32_porting_manual.md` – ESP32(EncoderMotor) 턴테이블 펌웨어 빌드/업로드 가이드
- `exec/raspberry_pi_porting_manual.md` – 라즈베리파이 스캐너 설치 가이드
- `exec/ec2_server_porting_manual.md` – EC2 변환 서버 설정 가이드

