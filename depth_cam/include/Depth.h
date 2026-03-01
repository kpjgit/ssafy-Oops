#pragma once
#include <cstdint>
#include <vector>
#include <cstring>

namespace oops {

struct Depth {
    // 스캔된 시점의 회전각(테이블 yaw), 단위: degree
    float angle_deg = 0.0f;

    // 카메라 pitch (정면 0, 아래 음수, 위 양수), 단위: degree
    float cam_pitch_deg = 0.0f;

    // 타임스탬프(ms)
    double timestamp_ms = 0.0;

    int width = 0;
    int height = 0;

    // raw depth buffer (Z16), row-major, len = w*h
    std::vector<uint16_t> data;

    // raw * depth_scale = meter
    float depth_scale = 0.0f;
};

// frame -> Depth 복사
template <class DepthFrameT>
inline void copyDepthData(Depth& out,
                          float angle_deg,
                          float cam_pitch_deg,
                          double timestamp_ms,
                          const DepthFrameT& frame,
                          float depth_scale)
{
    const int w = frame.get_width();
    const int h = frame.get_height();

    out.angle_deg = angle_deg;
    out.cam_pitch_deg = cam_pitch_deg;
    out.timestamp_ms = timestamp_ms;
    out.width = w;  out.height = h;
    out.depth_scale = depth_scale;

    out.data.resize(size_t(w) * size_t(h));
    const uint16_t* src = reinterpret_cast<const uint16_t*>(frame.get_data());
    std::memcpy(out.data.data(), src, size_t(w) * size_t(h) * sizeof(uint16_t));
}

} // namespace oops

