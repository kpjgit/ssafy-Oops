# scanner_service.py
#!/usr/bin/env python3
# uvicorn scanner_service:app --host 0.0.0.0 --port 8000
"""
Raspberry Pi foot-scan orchestrator.
- POST /scan  : run ./footscan once and (optionally) upload PLY via main.cpp
- GET  /jobs  : list job summaries
- GET  /jobs/{job_id} : inspect single job

Dependencies: pip install fastapi uvicorn pydantic
"""

import json
import os
import subprocess
import threading
import uuid
from dataclasses import dataclass, asdict, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, Optional

import requests
from fastapi import FastAPI, HTTPException
from pydantic import BaseModel, Field

# --- Paths/config --------------------------------------------------------
BASE_DIR = Path(__file__).resolve().parent
FOOTSCAN_BIN = (BASE_DIR / "build/footscan").resolve()
WORK_DIR = (BASE_DIR / "scan_jobs").resolve()
WORK_DIR.mkdir(parents=True, exist_ok=True)
CONVERT_SERVER_URL = os.environ.get("CONVERT_SERVER_URL")
CONVERT_TIMEOUT = float(os.environ.get("CONVERT_TIMEOUT_SEC", "120"))


def iso_now() -> str:
    return datetime.now(timezone.utc).isoformat()


class ScanRequest(BaseModel):
    frames: int = Field(12, gt=0)
    threshold_cm: float = Field(30.0, gt=0)
    pitch_deg: float = -10.0
    radius_m: float = Field(0.192, gt=0)
    cam_height_m: float = 0.10
    server_url: Optional[str] = Field(
        default=None, description="Optional HTTP upload endpoint consumed by src/main.cpp"
    )


@dataclass
class JobRecord:
    id: str
    state: str
    request: Dict[str, Any]
    created_at: str = field(default_factory=iso_now)
    updated_at: str = field(default_factory=iso_now)
    result: Dict[str, Any] = field(default_factory=dict)
    error: Optional[str] = None
    logs: Dict[str, str] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


jobs: Dict[str, JobRecord] = {}
app = FastAPI(title="FootScan Local Service")
state_lock = threading.Lock()
current_job_id: Optional[str] = None


@app.get("/health")
def health() -> Dict[str, str]:
    return {"status": "ok", "ts": iso_now()}


@app.get("/jobs")
def list_jobs() -> Dict[str, Any]:
    return {"jobs": [j.to_dict() for j in jobs.values()]}


@app.get("/jobs/{job_id}")
def get_job(job_id: str) -> Dict[str, Any]:
    job = jobs.get(job_id)
    if not job:
        raise HTTPException(404, "unknown job")
    return job.to_dict()


@app.post("/scan")
async def enqueue_scan(req: ScanRequest) -> Dict[str, str]:
    if not FOOTSCAN_BIN.exists():
        raise HTTPException(500, f"footscan binary not found: {FOOTSCAN_BIN}")

    with state_lock:
        busy = current_job_id is not None
    if busy:
        raise HTTPException(409, "scanner is busy; try again later")

    job_id = f"scan-{uuid.uuid4().hex[:8]}"
    job = JobRecord(id=job_id, state="queued", request=json.loads(req.json()))
    jobs[job_id] = job

    thread = threading.Thread(target=run_job_async, args=(job_id, req), daemon=True)
    thread.start()
    return {"job_id": job_id}


def run_job_async(job_id: str, req: ScanRequest) -> None:
    global current_job_id
    with state_lock:
        current_job_id = job_id
    try:
        run_job(job_id, req)
    finally:
        with state_lock:
            current_job_id = None


def run_job(job_id: str, req: ScanRequest) -> None:
    job = jobs[job_id]
    try:
        update_job(job, state="running")
        job_dir = WORK_DIR / job_id
        job_dir.mkdir(parents=True, exist_ok=True)

        timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S")
        ply_path = job_dir / f"{job_id}_{timestamp}.ply"

        scan_cmd = [
            str(FOOTSCAN_BIN),
            "--frames",
            str(req.frames),
            "--threshold_cm",
            str(req.threshold_cm),
            "--pitch_deg",
            str(req.pitch_deg),
            "--radius_m",
            str(req.radius_m),
            "--cam_height_m",
            str(req.cam_height_m),
            "--out",
            str(ply_path),
        ]
        if req.server_url:
            scan_cmd += ["--server_url", req.server_url]

        run_and_log(scan_cmd, job_dir / "footscan.log")
        job.logs["footscan"] = str(job_dir / "footscan.log")

        job.result = {"ply_path": str(ply_path)}
        if CONVERT_SERVER_URL:
            convert_result = send_to_convert_server(ply_path)
            job.result.update(
                {
                    "convert_job_id": convert_result.get("job_id"),
                    "glb_url": convert_result.get("glb_url"),
                    "glb_path": convert_result.get("glb_path"),
                }
            )
        update_job(job, state="succeeded")
    except Exception as exc:
        job.error = str(exc)
        update_job(job, state="failed")


def run_and_log(cmd: list[str], log_path: Path) -> None:
    with log_path.open("w", encoding="utf-8") as log_file:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        assert proc.stdout
        for line in proc.stdout:
            log_file.write(line)
            log_file.flush()
        ret = proc.wait()
    if ret != 0:
        raise RuntimeError(f"command failed ({ret}): {' '.join(cmd)}")


def update_job(job: JobRecord, **changes: Any) -> None:
    for k, v in changes.items():
        setattr(job, k, v)
    job.updated_at = iso_now()


def send_to_convert_server(ply_path: Path) -> Dict[str, Any]:
    if not CONVERT_SERVER_URL:
        raise RuntimeError("convert server URL is not configured")

    with ply_path.open("rb") as ply_file:
        files = {"file": (ply_path.name, ply_file, "application/octet-stream")}
        response = requests.post(CONVERT_SERVER_URL, files=files, timeout=CONVERT_TIMEOUT)
    if response.status_code != 200:
        raise RuntimeError(f"convert server error ({response.status_code}): {response.text}")
    try:
        return response.json()
    except json.JSONDecodeError as exc:  # pragma: no cover
        raise RuntimeError("convert server returned invalid JSON") from exc
