# EC2 메쉬 변환 서버 포팅 매뉴얼

본 문서는 `backend/mesh_convert_service.py`와 `ply2gltf.py`로 구성된 PLY→GLB 변환 서버를 AWS EC2(또는 동등한 x86_64 리눅스) 환경에 배포하는 절차를 설명합니다.

## 1. 아키텍처 개요
- FastAPI 앱(`/convert`, `/health`)이 업로드된 PLY를 저장하고 `ply2gltf.process_ply_to_glb` 파이프라인으로 GLB를 생성합니다.
- 변환된 GLB는 `backend/mesh_jobs/public_glbs/`에 저장, `/glbs/<job>.glb` 경로로 정적 서빙됩니다.
- 라즈베리파이 스캐너는 `CONVERT_SERVER_URL`에 `http://<ec2>:8080/convert`를 설정해 업로드/GLB URL을 획득합니다.

## 2. 시스템 요구사항
- OS: Ubuntu 22.04 LTS 권장.
- CPU/GPU: AVX 지원 x86_64 CPU(Open3D 사용), RAM 8GB 이상.
- Python 3.10 이상.
- 디스크: GLB/PLY 저장용으로 최소 20GB 여유 (cron으로 정리 권장).

## 3. 보안 그룹/네트워크
- 인바운드: `TCP 8080`(스캐너/API), `TCP 22`(관리). 가능하면 소스 IP를 사무실/VPC로 제한합니다.
- HTTPS가 필요하면 ALB 또는 Nginx(443→8080 프록시)를 두고 FastAPI는 내부에서만 노출합니다.

## 4. 패키지 및 런타임 설치
```bash
sudo apt update
sudo apt install -y git python3 python3-venv python3-dev build-essential \
    libgl1 libglib2.0-0 libxrender1 libxext6
```
- `libgl`/`libx*` 패키지는 헤드리스 환경에서도 Open3D가 동작하도록 해 줍니다.

## 5. 코드 배치 및 가상환경
```bash
cd /opt/oops
sudo git clone <repo-url>
sudo chown -R ubuntu:ubuntu S13P31A107
cd S13P31A107/backend
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install fastapi "uvicorn[standard]" numpy open3d trimesh scipy shapely
```
- `ply2gltf`는 `numpy`, `open3d`, `trimesh` 외에도 `scipy`, `shapely` 등의 의존성을 자동 설치하지만, 미리 명시하면 배포 시 안정적입니다.

## 6. 환경 변수 설정
- `CONVERT_PUBLIC_BASE_URL`: 클라이언트에 안내할 외부 URL(예: `https://scanner-api.example.com`). 미지정 시 요청 `base_url`을 그대로 사용합니다.
- 로그 디렉터리: 기본적으로 `backend/mesh_jobs` 아래를 사용합니다. 다른 EBS 마운트를 쓰려면 심볼릭 링크를 연결하거나 `JOB_DIR` 상수를 수정하세요.

## 7. 서비스 실행
### 7.1 개발 모드
```bash
source /opt/oops/S13P31A107/backend/.venv/bin/activate
uvicorn mesh_convert_service:app --host 0.0.0.0 --port 8080
```
- `/health` 체크로 기동 상태를 확인합니다.

### 7.2 systemd 서비스 예시
`/etc/systemd/system/oops-convert.service`
```
[Unit]
Description=Oops Foot Mesh Convert Service
After=network.target

[Service]
Type=simple
User=ubuntu
WorkingDirectory=/opt/oops/S13P31A107/backend
Environment="CONVERT_PUBLIC_BASE_URL=https://scanner-api.example.com"
ExecStart=/opt/oops/S13P31A107/backend/.venv/bin/uvicorn mesh_convert_service:app --host 0.0.0.0 --port 8080
Restart=on-failure

[Install]
WantedBy=multi-user.target
```
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now oops-convert
```
- 로그 확인: `journalctl -u oops-convert -f`

## 8. API 사양
### 8.1 `GET /health`
- 응답: `{ "status": "ok" }`

### 8.2 `POST /convert`
- Content-Type: `multipart/form-data`
- 필드: `file`(PLY 바이너리)
- 성공 응답 예시
```json
{
  "job_id": "mesh-2ab3c4d5",
  "ply_path": "/opt/oops/S13P31A107/backend/mesh_jobs/mesh-2ab3c4d5.ply",
  "glb_path": "/opt/oops/S13P31A107/backend/mesh_jobs/public_glbs/mesh-2ab3c4d5.glb",
  "glb_url": "https://scanner-api.example.com/glbs/mesh-2ab3c4d5.glb"
}
```
- 에러 응답: HTTP 400 + 메시지(`conversion failed: ...`).
- CORS: 모든 Origin 허용(`allow_origins=['*']`), 모바일 앱/라즈베리파이가 직접 호출 가능.


