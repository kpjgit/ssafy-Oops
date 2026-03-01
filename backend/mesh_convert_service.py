from __future__ import annotations

"""
FastAPI server that accepts PLY uploads, runs ply2gltf conversion, and
serves the resulting GLB files over HTTP.

Usage:
    uvicorn mesh_convert_service:app --host 0.0.0.0 --port 8080

Environment variables:
    CONVERT_PUBLIC_BASE_URL  Optional base URL (e.g. https://host:8080) used
                             when constructing glb_url responses. Defaults to
                             the incoming request base_url.
"""

import os
import uuid
from pathlib import Path

from fastapi import FastAPI, File, HTTPException, Request, UploadFile
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse
from fastapi.staticfiles import StaticFiles

from ply2gltf import process_ply_to_glb

BASE_DIR = Path(__file__).resolve().parent
JOB_DIR = BASE_DIR / "mesh_jobs"
PUBLIC_DIR = JOB_DIR / "public_glbs"
JOB_DIR.mkdir(parents=True, exist_ok=True)
PUBLIC_DIR.mkdir(parents=True, exist_ok=True)

PUBLIC_BASE_URL = os.environ.get("CONVERT_PUBLIC_BASE_URL")

app = FastAPI(title="Foot Mesh Convert Service")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
    allow_credentials=False,
)
app.mount("/glbs", StaticFiles(directory=PUBLIC_DIR), name="glbs")


def _build_public_url(filename: str, request: Request) -> str:
    base = PUBLIC_BASE_URL.rstrip("/") if PUBLIC_BASE_URL else str(request.base_url).rstrip("/")
    return f"{base}/glbs/{filename}"


@app.get("/health")
def health() -> dict[str, str]:
    return {"status": "ok"}


@app.post("/convert")
async def convert(request: Request, file: UploadFile = File(...)) -> JSONResponse:
    job_id = f"mesh-{uuid.uuid4().hex[:8]}"
    ply_path = JOB_DIR / f"{job_id}.ply"
    glb_filename = f"{job_id}.glb"
    glb_path = PUBLIC_DIR / glb_filename

    contents = await file.read()
    if not contents:
        raise HTTPException(400, "uploaded file is empty")
    ply_path.write_bytes(contents)

    try:
        process_ply_to_glb(ply_path=str(ply_path), out_glb_path=str(glb_path))
    except Exception as exc:  # pragma: no cover - conversion errors bubble to client
        ply_path.unlink(missing_ok=True)
        glb_path.unlink(missing_ok=True)
        raise HTTPException(400, f"conversion failed: {exc}") from exc

    response = {
        "job_id": job_id,
        "ply_path": str(ply_path),
        "glb_path": str(glb_path),
        "glb_url": _build_public_url(glb_filename, request),
    }
    return JSONResponse(response)


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(app, host="0.0.0.0", port=8080)
