import argparse
import io
import json
import tempfile
from typing import Optional, Tuple

import numpy as np
import open3d as o3d
import trimesh

# -------------------------
# Core processing pipeline
# -------------------------

def _ensure_outward_winding(mesh: o3d.geometry.TriangleMesh) -> o3d.geometry.TriangleMesh:
    """
    Open3D의 Poisson 결과는 법선 방향이 뒤집혀 나오는 경우가 있어 GLB에서
    백페이스만 보이는 현상이 생길 수 있다. 메쉬 중심을 기준으로 삼각형
    노말과 바라보는 방향이 반대라면 winding을 뒤집어 준다.
    """
    if len(mesh.triangles) == 0:
        return mesh

    mesh.compute_triangle_normals()
    vertices = np.asarray(mesh.vertices)
    triangles = np.asarray(mesh.triangles)
    tri_centers = vertices[triangles].mean(axis=1)
    normals = np.asarray(mesh.triangle_normals)

    # 중심에서 각 삼각형 중심을 바라보는 벡터와 노말의 평균 내적 부호를 본다.
    view_vectors = tri_centers - mesh.get_center()
    alignment = np.mean(np.einsum("ij,ij->i", normals, view_vectors))

    if alignment < 0:
        flipped = triangles[:, [0, 2, 1]]
        mesh.triangles = o3d.utility.Vector3iVector(flipped)
        mesh.vertex_normals = o3d.utility.Vector3dVector(-np.asarray(mesh.vertex_normals))
        mesh.triangle_normals = o3d.utility.Vector3dVector(-normals)

    return mesh


def process_ply_to_glb(
    ply_path: str,
    out_glb_path: Optional[str] = None,
    voxel_size: float = 0.002,          # 3mm 다운샘플 (필요시 0으로 꺼도 됨)
    stat_nb_neighbors: int = 20,
    stat_std_ratio: float = 3.5,
    ransac_remove_table: bool = True,
    table_z_cut_eps: Optional[float] = None,  # 예: 0.01 -> Z in [-1cm, +1cm] 제거. None이면 미사용
    dbscan_eps: float = 0.01,            # 0.1cm 이내를 같은 클러스터로
    dbscan_min_points: int = 40,
    poisson_depth: int = 12,              # 8~10 권장 (깊을수록 디테일↑, 시간↑)
    simplify_target_tris: Optional[int] = 100_000,  # 메쉬 디메시피케이션 타겟
    smooth_iters: int = 5,
    density_trim_percent: Optional[float] = 5.0,
    rotate_euler_deg: Optional[Tuple[float, float, float]] = (-90.0, 0.0, 0.0),
) -> bytes:
    """
    입력: PLY 경로
    출력: GLB(바이너리) bytes (또는 파일로 저장)

    파이프라인:
      PLY -> (다운샘플) -> (노이즈 제거) -> (테이블 제거) -> (최대 클러스터=발) ->
      법선 -> Poisson -> 저밀도 버텍스 제거 -> (스무딩/디메시피케이션) -> GLB
    """
    # 1) Load
    pcd = o3d.io.read_point_cloud(ply_path)
    if pcd.is_empty():
        raise ValueError("Empty point cloud")

    # 2) Downsample (optional)
    if voxel_size and voxel_size > 0:
        pcd = pcd.voxel_down_sample(voxel_size=voxel_size)

    # 3) Denoise: Statistical Outlier Removal
    pcd, ind = pcd.remove_statistical_outlier(
        nb_neighbors=stat_nb_neighbors, std_ratio=stat_std_ratio
    )

    # (optional) 빠른 Z-컷 기반 테이블 제거: 좌표계에서 테이블이 Z=0 근처에 놓여있다면 활성화
    if table_z_cut_eps is not None and table_z_cut_eps > 0:
        pts = np.asarray(pcd.points)
        mask = np.abs(pts[:, 2]) > table_z_cut_eps  # |Z| > eps 만 유지
        pcd = pcd.select_by_index(np.where(mask)[0])

    # 4) RANSAC 평면 분리로 테이블 제거 (일반적)
    if ransac_remove_table:
        plane_model, inliers = pcd.segment_plane(
            distance_threshold=0.004,  # 4mm 허용
            ransac_n=3,
            num_iterations=1000
        )
        # plane_model: ax + by + cz + d = 0
        table_cloud = pcd.select_by_index(inliers)
        object_cloud = pcd.select_by_index(inliers, invert=True)
        pcd = object_cloud  # 테이블 제외

    # 5) DBSCAN으로 최대 클러스터(foot)만 유지
    if len(pcd.points) == 0:
        raise ValueError("No points left after table removal")

    labels = np.array(pcd.cluster_dbscan(eps=dbscan_eps, min_points=dbscan_min_points, print_progress=False))
    if labels.size == 0 or labels.max() < 0:
        # 클러스터가 하나로도 안 묶여 있으면 그대로 진행
        foot_pcd = pcd
    else:
        largest = int(np.bincount(labels[labels >= 0]).argmax())
        idx = np.where(labels == largest)[0]
        foot_pcd = pcd.select_by_index(idx)

    # 6) 법선 추정
    foot_pcd.estimate_normals(
        search_param=o3d.geometry.KDTreeSearchParamHybrid(radius=0.02, max_nn=30)
    )
    foot_pcd.orient_normals_consistent_tangent_plane(30)

    # 7) 메쉬 재구성 (Poisson)
    mesh, densities = o3d.geometry.TriangleMesh.create_from_point_cloud_poisson(
        foot_pcd, depth=poisson_depth
    )
    # 저밀도 버텍스 제거 (아웃라이어)
    densities = np.asarray(densities)
    if density_trim_percent is not None and 0.0 < density_trim_percent < 100.0:
        threshold = np.percentile(densities, density_trim_percent)
        keep = densities >= threshold
        mesh = mesh.select_by_index(np.where(keep)[0])

    # 8) 메쉬 클린업: 작게 떨어져 있는 컴포넌트 제거
    mesh.remove_unreferenced_vertices()
    mesh.remove_degenerate_triangles()
    mesh.remove_duplicated_triangles()
    mesh.remove_duplicated_vertices()
    mesh.remove_non_manifold_edges()

    # 작은 컴포넌트 제거
    cc = mesh.cluster_connected_triangles()
    tri_labels = np.asarray(cc[0])
    if tri_labels.size > 0:
        largest = int(np.bincount(tri_labels).argmax())
        keep_mask = (tri_labels == largest)
        # remove_* 는 "지울 것"을 받으므로 반전 마스크를 넘긴다
        mesh.remove_triangles_by_mask(~keep_mask)
        mesh.remove_unreferenced_vertices()

    # 스무딩 & 디메시피케이션(옵션)
    if smooth_iters and smooth_iters > 0:
        mesh = mesh.filter_smooth_simple(number_of_iterations=smooth_iters)
    if simplify_target_tris and simplify_target_tris > 0 and len(mesh.triangles) > simplify_target_tris:
        mesh = mesh.simplify_quadric_decimation(target_number_of_triangles=simplify_target_tris)

    mesh.compute_vertex_normals()
    if rotate_euler_deg is not None and any(abs(angle) > 1e-6 for angle in rotate_euler_deg):
        rot = mesh.get_rotation_matrix_from_xyz(tuple(np.deg2rad(rotate_euler_deg)))
        mesh.rotate(rot, center=mesh.get_center())
    mesh = _ensure_outward_winding(mesh)

    # 9) Open3D -> Trimesh 변환 후 GLB로 export
    V = np.asarray(mesh.vertices)
    F = np.asarray(mesh.triangles)
    tmesh = trimesh.Trimesh(vertices=V, faces=F, process=False)
    # 좌표 단위는 meter 유지. 필요 시 scene scale 적용 가능.
    glb_bytes = tmesh.export(file_type='glb')  # returns bytes

    if out_glb_path:
        with open(out_glb_path, "wb") as f:
            f.write(glb_bytes)

    return glb_bytes


# -------------------------
# CLI
# -------------------------

def run_cli():
    ap = argparse.ArgumentParser(description="PLY -> cleaned mesh -> GLB")
    ap.add_argument("--input", required=True, help="Input PLY path")
    ap.add_argument("--out", default="foot.glb", help="Output GLB path")
    ap.add_argument("--voxel", type=float, default=0.002, help="Voxel size (m), 0 to disable")
    ap.add_argument("--stat_nb", type=int, default=20, help="Statistical outlier nb_neighbors")
    ap.add_argument("--stat_std", type=float, default=3.5, help="Statistical outlier std_ratio")
    ap.add_argument("--table_z_eps", type=float, default=None, help="Fast Z-cut for table around Z=0 (m). None to skip")
    ap.add_argument("--no_ransac", action="store_true", help="Disable RANSAC plane removal")
    ap.add_argument("--dbscan_eps", type=float, default=0.01, help="DBSCAN eps (m)")
    ap.add_argument("--dbscan_min", type=int, default=40, help="DBSCAN min_points")
    ap.add_argument("--poisson_depth", type=int, default=12, help="Poisson depth (8~10)")
    ap.add_argument("--simplify_tris", type=int, default=100000, help="Target triangles after simplification; 0=skip")
    ap.add_argument("--smooth_iters", type=int, default=5, help="Smoothing iterations")
    ap.add_argument("--density_trim", type=float, default=5.0, help="Percent of low-density vertices to drop (0-100, default 5)")
    ap.add_argument(
        "--rotate",
        type=float,
        nargs=3,
        metavar=("RX", "RY", "RZ"),
        default=(-90.0, 0.0, 0.0),
        help="Euler rotation degrees applied before export (x y z)",
    )
    ap.add_argument("--serve", action="store_true", help="Run as HTTP server instead of CLI convert")
    args = ap.parse_args()

    if args.serve:
        run_server()
        return

    glb = process_ply_to_glb(
        ply_path=args.input,
        out_glb_path=args.out,
        voxel_size=args.voxel if args.voxel > 0 else 0.0,
        stat_nb_neighbors=args.stat_nb,
        stat_std_ratio=args.stat_std,
        ransac_remove_table=not args.no_ransac,
        table_z_cut_eps=args.table_z_eps,
        dbscan_eps=args.dbscan_eps,
        dbscan_min_points=args.dbscan_min,
        poisson_depth=args.poisson_depth,
        simplify_target_tris=(None if args.simplify_tris <= 0 else args.simplify_tris),
        smooth_iters=args.smooth_iters,
        density_trim_percent=args.density_trim,
        rotate_euler_deg=tuple(args.rotate) if args.rotate is not None else None,
    )
    print(f"[OK] wrote GLB: {args.out} ({len(glb)} bytes)")


# -------------------------
# HTTP server (FastAPI)
# -------------------------

def run_server():
    from fastapi import FastAPI, UploadFile, File, Form
    from fastapi.responses import Response, JSONResponse
    import uvicorn

    app = FastAPI(title="FootScan Mesh Server")

    @app.post("/process")
    async def process_endpoint(
        file: UploadFile = File(...),
        voxel: float = Form(0.003),
        stat_nb: int = Form(20),
        stat_std: float = Form(2.0),
        table_z_eps: float = Form(None),
        no_ransac: bool = Form(False),
        dbscan_eps: float = Form(0.01),
        dbscan_min: int = Form(50),
        poisson_depth: int = Form(9),
        simplify_tris: int = Form(100000),
        smooth_iters: int = Form(5),
        density_trim: float = Form(5.0),
        rotate_x: float = Form(0.0),
        rotate_y: float = Form(0.0),
        rotate_z: float = Form(0.0),
    ):
        try:
            # 임시 파일로 저장 후 처리 (Open3D는 파일 경로로 로딩하는 게 안전)
            with tempfile.NamedTemporaryFile(suffix=".ply", delete=False) as tmp:
                content = await file.read()
                tmp.write(content)
                tmp.flush()
                ply_path = tmp.name

            glb_bytes = process_ply_to_glb(
                ply_path=ply_path,
                out_glb_path=None,
                voxel_size=voxel if voxel > 0 else 0.0,
                stat_nb_neighbors=stat_nb,
                stat_std_ratio=stat_std,
                ransac_remove_table=(not no_ransac),
                table_z_cut_eps=(None if table_z_eps in (None, "", "null") else float(table_z_eps)),
                dbscan_eps=dbscan_eps,
                dbscan_min_points=dbscan_min,
                poisson_depth=poisson_depth,
                simplify_target_tris=(None if simplify_tris <= 0 else simplify_tris),
                smooth_iters=smooth_iters,
                density_trim_percent=density_trim,
                rotate_euler_deg=(rotate_x, rotate_y, rotate_z),
            )

            return Response(content=glb_bytes, media_type="model/gltf-binary")  # GLB
        except Exception as e:
            return JSONResponse({"error": str(e)}, status_code=400)

    uvicorn.run(app, host="0.0.0.0", port=8000)


if __name__ == "__main__":
    run_cli()
