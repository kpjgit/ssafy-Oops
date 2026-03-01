# 라즈베리파이 스캐너 포팅 매뉴얼

`depth_cam/` 디렉터리의 RealSense 스캐너와 Python 오케스트레이터를 라즈베리파이에 배포하는 과정을 설명합니다. 대상 하드웨어는 RealSense D435/D455, ESP32 기반 턴테이블 모터, Raspberry Pi 4(4GB 이상, 64-bit OS) 조합을 기준으로 합니다.

## 1. 구성요소 개요
- `src/main.cpp`: RealSense 프레임 캡처 → 포인트클라우드 생성 → PLY 저장/업로드를 담당하는 `footscan` 실행 파일.
- `include/*.h`: 깊이 변환(`Depth`, `pointcloud`), 블루투스 모터 제어(`motor`), PLY 저장(`ply_writer`).
- `scanner_service.py`: FastAPI 기반 REST 서비스. `/scan` 요청을 받아 `footscan`을 실행하고, 결과를 EC2 변환 서버로 업로드합니다.
- `scan_jobs/`: 각 스캔 작업의 로그/결과 PLY를 저장하는 디렉터리 (서비스 실행 시 자동 생성).

## 2. 하드웨어/펌웨어 준비
1. RealSense 카메라를 USB 3.0 포트에 연결합니다.
2. ESP32 모터 컨트롤러(EncoderMotor 프로젝트 펌웨어)를 플래싱하고 SPP(Bluetooth Classic) 서버로 동작하도록 설정합니다.
3. `depth_cam/include/motor.h`의 `FIXED_MAC`, `FIXED_CHANNEL` 값을 ESP32의 실제 MAC/채널로 수정합니다.
4. 턴테이블 기준점과 카메라 위치를 측정해 `--radius_m`, `--cam_height_m`, `--pitch_deg` 기본값을 파악합니다.

## 3. OS 및 시스템 패키지 설치
```bash
sudo apt update
sudo apt upgrade -y
sudo apt install -y git build-essential cmake ninja-build pkg-config \
    libcurl4-openssl-dev libbluetooth-dev python3-venv python3-pip python3-dev \
    libatlas-base-dev libssl-dev
```

### 3.1 RealSense SDK 설치
1. Intel 공식 문서(https://github.com/IntelRealSense/librealsense/blob/master/doc/distribution_linux.md)를 따라 apt 저장소를 추가합니다.
2. 필수 패키지 설치
   ```bash
   sudo apt install -y librealsense2-dkms librealsense2-utils librealsense2-dev librealsense2-dbg
   ```
3. 카메라 권한을 위해 `rs-enumerate-devices`를 실행하여 장치가 인식되는지 확인합니다.

### 3.2 Bluetooth 권한
- `bluetooth` 서비스가 실행 중인지(`systemctl status bluetooth`) 확인하고, 스캐너에 사용할 Pi 계정이 `bluetooth`, `dialout` 그룹에 속해 있는지 점검합니다.

## 4. 프로젝트 배치
```bash
cd ~/oops
git clone <repo-url>
cd S13P31A107/depth_cam
```
- `scanner_service.py`는 동일 디렉터리에서 실행하는 것을 기준으로 작성되어 있습니다.

## 5. C++ 스캐너 빌드
```bash
cd ~/oops
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```
- 성공 시 `build/footscan` 바이너리가 생성됩니다.
- 빌드 실패 시 `realsense2` 또는 `libcurl` 링크 에러가 발생하면 `sudo ldconfig` 후 다시 시도합니다.

### 5.1 실행 테스트
```bash
./footscan --frames 12 --threshold_cm 30 --pitch_deg -10 --radius_m 0.192 \
          --cam_height_m 0.10 --out /tmp/test.ply
```
- ESP32 모터 전원이 켜져 있어야 하며, Bluetooth 연결이 자동으로 이루어지는지 로그(`motor] Connected...`)를 확인합니다.
- `--server_url http://<ec2-host>:8080/convert` 옵션을 주면 변환 서버로 파일을 업로드합니다.

## 6. Python 스캐너 서비스 구성
1. 가상환경 생성 및 의존성 설치
   ```bash
   cd ~/oops/S13P31A107/depth_cam
   python3 -m venv .venv
   source .venv/bin/activate
   pip install --upgrade pip
   pip install fastapi "uvicorn[standard]" requests pydantic
   ```
2. 환경 변수 설정 (예: `~/.bashrc` 또는 systemd 단위 파일)
   ```bash
   export CONVERT_SERVER_URL="http://<ec2-host>:8080/convert"
   export CONVERT_TIMEOUT_SEC="180"          # 선택: 기본 120초
   ```
3. 서비스 실행
   ```bash
   source .venv/bin/activate
   uvicorn scanner_service:app --host 0.0.0.0 --port 8000
   ```
4. 헬스체크: `curl http://<pi-ip>:8000/health`
5. 스캔 요청: `curl -X POST http://<pi-ip>:8000/scan -H 'Content-Type: application/json' -d '{"frames":12,"radius_m":0.192}'`
6. 작업 조회: `curl http://<pi-ip>:8000/jobs/<job_id>`

### 6.1 systemd 서비스 예시
`/etc/systemd/system/oops-scanner.service`
```
[Unit]
Description=Oops Foot Scanner API
After=network.target bluetooth.service

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/oops/S13P31A107/depth_cam
Environment="CONVERT_SERVER_URL=http://<ec2-host>:8080/convert"
Environment="CONVERT_TIMEOUT_SEC=180"
ExecStart=/home/pi/oops/S13P31A107/depth_cam/.venv/bin/uvicorn scanner_service:app --host 0.0.0.0 --port 8000
Restart=on-failure

[Install]
WantedBy=multi-user.target
```
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now oops-scanner
```

## 7. 디렉터리 및 로그
- `depth_cam/scan_jobs/<job_id>/`: 생성된 PLY, `footscan.log` 로그 저장 위치.
- `uvicorn` 표준 출력: `journalctl -u oops-scanner -f`로 모니터링.
- `footscan` 실행 실패 시 `scan_jobs/<job_id>/footscan.log`를 우선 확인합니다.

