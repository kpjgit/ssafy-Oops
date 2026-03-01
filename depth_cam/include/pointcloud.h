#pragma once
#include "Depth.h"
#include <vector>
#include <cmath>

// 좌표계 가정:
// - 원판 중심이 월드 원점 (0,0,0)
// - 원판 평면이 XY, 법선이 +Z
// - 카메라는 원판 가장자리 반지름 R에서 원점을 바라보며 회전
// - d.angle_deg = 테이블(카메라 위치)의 yaw(도)
// - d.cam_pitch_deg = 카메라 pitch(도; 아래쪽이 음수)
// - 월드 변환: p_world = Rz(yaw + π) * Rx(pitch) * p_cam + t(θ)
//   where t(θ) = [ R*cosθ, R*sinθ, cam_height ]

namespace oops {

constexpr float deg2rad(float d) { return d * float(M_PI) / 180.0f; }

// (u,v,Z[m]) -> 카메라 좌표계 (X,Y,Z) 변환
inline void pixel_to_xyz(int u, int v, float Z,
                         float fx, float fy, float cx, float cy,
                         float& X, float& Y, float& outZ)
{
    X = (u - cx) * Z / fx;
    Y = (v - cy) * Z / fy;
    outZ = Z;
}

// Z 범위 임계값 적용 + 카메라 pitch, 회전(yaw+π), 위치 t(θ) 반영하여 월드 XYZ 생성
// - d.depth_scale 를 사용해 raw(Z16) → meter 변환
// - 유효 Z 범위: [z_min_m, z_max_m]
// - radius_m: 원판 중심→카메라까지 반지름 (m)
// - cam_height_m: 원판 평면 기준 카메라 높이 (m)
inline void depth_to_pointcloud(const oops::Depth& d,
                                float fx, float fy, float cx, float cy,
                                float z_min_m, float z_max_m,
                                float radius_m, float cam_height_m,
                                std::vector<float>& xyz_out /* append */)
{
    const int W = d.width, H = d.height;

    const float pitch = deg2rad(d.cam_pitch_deg);
    const float yaw   = deg2rad(d.angle_deg);
    
    // 고정 회전: Rx(-90°)
    constexpr float HALF_PI = float(M_PI) / 2.0f;
    
    // 최종 Z축 회전: yaw + 90°
    const float cRz = std::cos(yaw + HALF_PI);  // <-- 이름 변경: cRz
    const float sRz = std::sin(yaw + HALF_PI);  // <-- 이름 변경: sRz
    
    // pitch 회전
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    
    // translation: 카메라 월드 위치
    const float tx = radius_m * std::cos(yaw);
    const float ty = radius_m * std::sin(yaw);
    const float tz = cam_height_m;
    
    for (int v = 0; v < H; ++v) {
        const size_t row = size_t(v) * size_t(W);
        for (int u = 0; u < W; ++u) {
            const uint16_t raw = d.data[row + size_t(u)];
            if (!raw) continue;
    
            const float Zm = raw * d.depth_scale;
            if (Zm < z_min_m || Zm > z_max_m) continue;
    
            float Xc, Yc, Zc;
            // cy는 'principal point y' 그대로 사용됩니다.
            pixel_to_xyz(u, v, Zm, fx, fy, cx, cy, Xc, Yc, Zc);
    
            // 1) Rx(-90°): (X, Y, Z) -> (X, Z, -Y)
            const float Xa = Xc;
            const float Ya = Zc;
            const float Za = -Yc;
    
            // 2) Rx(pitch)
            const float Xp = Xa;
            const float Yp =  cp * Ya - sp * Za;
            const float Zp =  sp * Ya + cp * Za;
    
            // 3) Rz(yaw + 90°)
            const float Xr =  cRz * Xp - sRz * Yp;  // <-- cRz/sRz 사용
            const float Yr =  sRz * Xp + cRz * Yp;
            const float Zr =  Zp;
    
            // translation
            xyz_out.push_back(Xr + tx);
            xyz_out.push_back(Yr + ty);
            xyz_out.push_back(Zr + tz);
        }
    }
}

} // namespace oops
