#include <librealsense2/rs.hpp>
#include <iostream>
#include <fstream>
#include <cstdint>
#include <vector>

int main() try {
    rs2::pipeline pipe;
    rs2::config cfg;

    // D405 근접용 depth 스트림 설정 (필요하면 해상도/프레임레이트 조정 가능)
    cfg.enable_stream(RS2_STREAM_DEPTH, 848, 480, RS2_FORMAT_Z16, 30);

    auto profile = pipe.start(cfg);

    // depth scale: raw값 -> meter
    auto dev = profile.get_device();
    auto depth_sensor = dev.first<rs2::depth_sensor>();
    float depth_scale = depth_sensor.get_depth_scale();
    std::cout << "[info] depth_scale (m per unit): " << depth_scale << "\n";

    // intrinsics: fx, fy, cx, cy도 같이 뽑아둠 (포인트클라우드 만들 때 필요)
    auto vsp = profile.get_stream(RS2_STREAM_DEPTH).as<rs2::video_stream_profile>();
    auto intr = vsp.get_intrinsics();
    std::cout << "[info] intrinsics: w=" << intr.width << " h=" << intr.height
              << " fx=" << intr.fx << " fy=" << intr.fy
              << " cx=" << intr.ppx << " cy=" << intr.ppy << "\n";

    // 프레임 하나만 캡처해서 raw 저장
    rs2::frameset fs = pipe.wait_for_frames(5000);
    rs2::depth_frame d = fs.get_depth_frame();

    int w = d.get_width();
    int h = d.get_height();
    const uint16_t* raw = reinterpret_cast<const uint16_t*>(d.get_data());

    // 중앙 픽셀 거리
    uint16_t center_raw = raw[(h/2) * w + (w/2)];
    float center_m = center_raw * depth_scale;
    std::cout << "[info] center_raw=" << center_raw
              << "  => " << center_m << " m\n";

    // 1) 바이너리 그대로 덤프 (.bin)
    {
        std::ofstream f("depth_raw_z16.bin", std::ios::binary);
        f.write(reinterpret_cast<const char*>(raw), w * h * sizeof(uint16_t));
        std::cout << "[dump] wrote depth_raw_z16.bin (" << w << "x" << h
                  << ", " << (w*h*sizeof(uint16_t)) << " bytes)\n";
    }

    // 2) PGM(16bit grayscale) 저장 -> 뷰어에서 열어볼 때 유용
    {
        std::ofstream f("depth_raw_z16.pgm", std::ios::binary);
        f << "P5\n" << w << " " << h << "\n65535\n";
        f.write(reinterpret_cast<const char*>(raw), w * h * sizeof(uint16_t));
        std::cout << "[dump] wrote depth_raw_z16.pgm (16-bit grayscale)\n";
    }

    pipe.stop();
    return 0;
}
catch (const rs2::error& e) {
    std::cerr << "RealSense error: " << e.what() << "\n";
    return 1;
}
catch (const std::exception& e) {
    std::cerr << "Std error: " << e.what() << "\n";
    return 1;
}

