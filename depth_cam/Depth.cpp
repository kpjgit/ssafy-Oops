#pragma once

#include <cstdint>
#include <vector>

namespace oops {

    struct Depth {
        // 스캔된 시점의 회전각 (예: 엔코더에서 읽은 회전 각도, 단위: degree)
        float angle_deg;

        // 캡처 타임스탬프 (예: 카메라 제공 ms 혹은 시스템 시간 us 등)
        double timestamp_ms;

        // 이미지 크기
        int width;
        int height;

        // raw depth buffer (Z16 그대로). length == width * height
        // row-major: index = y * width + x
        std::vector<uint16_t> data;

        // 이 프레임에 대응하는 scale (raw * depth_scale = meters)
        float depth_scale;

        Depth() : angle_deg(0.0f),
                  timestamp_ms(0.0),
                  width(0),
                  height(0),
                  depth_scale(0.0f)
        {}
    };

    // frame -> Depth 로 복사하는 함수
    // 주의: frame이 살아있는 동안 동기적으로 즉시 호출해서 data를 안전하게 복사해야 함
    inline void copyDepthData(Depth& out,
                              float angle_deg,
                              double timestamp_ms,
                              const rs2::depth_frame& frame,
                              float depth_scale)
    {
        int w = frame.get_width();
        int h = frame.get_height();

        out.angle_deg = angle_deg;
        out.timestamp_ms = timestamp_ms;
        out.width = w;
        out.height = h;
        out.depth_scale = depth_scale;

        // 대상 버퍼 크기 확보
        out.data.resize(w * h);

        // frame.get_data() -> uint16_t*로 캐스팅한 뒤 memcpy
        const uint16_t* src = reinterpret_cast<const uint16_t*>(frame.get_data());
        std::memcpy(out.data.data(),
                    src,
                    static_cast<size_t>(w) * static_cast<size_t>(h) * sizeof(uint16_t));
    }

} // namespace oops
