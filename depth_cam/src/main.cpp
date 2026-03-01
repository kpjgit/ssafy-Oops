#include <librealsense2/rs.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#include "Depth.h"
#include "motor.h"
#include "pointcloud.h"
#include "ply_writer.h"

// (선택) 서버 업로드: libcurl 사용
#include <curl/curl.h>

static void usage(const char* prog) {
    std::cerr <<
    "Usage: " << prog << " --frames N --threshold_cm T --pitch_deg P --radius_m R [--cam_height_m H] [--server_url URL] [--out ply_path]\n"
    "  --frames N        : 촬영 프레임 수 (테이블을 360/N deg씩 회전)\n"
    "  --threshold_cm T  : 깊이 임계값(cm). 사용 범위 [0..T] (필터 상한)\n"
    "  --pitch_deg P     : 카메라 pitch(정면 0, 아래 음수, 위 양수, deg)\n"
    "  --radius_m R      : 원판 중심 → 카메라까지의 반지름(미터)  ★필수\n"
    "  --cam_height_m H  : 원판 평면(원점) 기준 카메라 높이(미터). 기본 0.0\n"
    "  --server_url URL  : 업로드 서버 URL (예: http://host:port/upload). 생략 시 업로드 생략\n"
    "  --out ply_path    : 저장할 PLY 경로 (기본: scan.ply)\n";
}

static bool http_upload_file(const std::string& url, const std::string& filepath) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    CURLcode res;
    struct curl_httppost* formpost = nullptr;
    struct curl_httppost* lastptr  = nullptr;

    curl_formadd(&formpost, &lastptr,
                 CURLFORM_PTRNAME, "file",
                 CURLFORM_FILE, filepath.c_str(),
                 CURLFORM_END);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "footscan/1.0");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_formfree(formpost);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[upload] curl error: " << curl_easy_strerror(res) << "\n";
        return false;
    }
    std::cout << "[upload] http status: " << http_code << "\n";
    return http_code >= 200 && http_code < 300;
}

int main(int argc, char* argv[]) try
{
    // ---- 인자 파싱 ----
    int   frame_num     = 0;
    float threshold_cm  = 0.f;
    float cam_pitch_deg = 0.f;
    float radius_m      = -1.f;   // ★ 필수 → 음수면 미설정
    float cam_height_m  = 0.f;    // ★ 선택 → 기본 0.0
    std::string server_url;
    std::string out_path = "scan.ply";

    auto need = [&](int i, int more){
        if (i+more >= argc) { usage(argv[0]); std::exit(1); }
    };

    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        if (a == "--frames")          { need(i,1); frame_num     = std::atoi(argv[++i]); }
        else if (a == "--threshold_cm"){ need(i,1); threshold_cm  = std::atof(argv[++i]); }
        else if (a == "--pitch_deg")  { need(i,1); cam_pitch_deg  = std::atof(argv[++i]); }
        else if (a == "--radius_m")   { need(i,1); radius_m       = std::atof(argv[++i]); }
        else if (a == "--cam_height_m"){need(i,1); cam_height_m   = std::atof(argv[++i]); }
        else if (a == "--server_url") { need(i,1); server_url     = argv[++i]; }
        else if (a == "--out")        { need(i,1); out_path       = argv[++i]; }
        else { usage(argv[0]); return 1; }
    }

    // 필수 체크
    if (frame_num <= 0) { std::cerr << "ERROR: --frames must be > 0\n"; usage(argv[0]); return 1; }
    if (radius_m <= 0.f){ std::cerr << "ERROR: --radius_m must be > 0\n"; usage(argv[0]); return 1; }
    if (threshold_cm <= 0.f) {
        std::cerr << "WARN: --threshold_cm not set or <=0. Using default 40cm.\n";
        threshold_cm = 40.f;
    }

    const float z_min_m = 0.0f;                 // 필요하면 노이즈 컷(예: 0.03)
    const float z_max_m = threshold_cm / 100.f; // cm → m

    // ---- RealSense 파이프라인 ----
    rs2::pipeline pipe;
    rs2::config cfg;
    cfg.enable_stream(RS2_STREAM_DEPTH, 848, 480, RS2_FORMAT_Z16, 30);
    auto profile = pipe.start(cfg);

    // scale & intrinsics
    auto dev = profile.get_device();
    auto depth_sensor = dev.first<rs2::depth_sensor>();
    const float depth_scale = depth_sensor.get_depth_scale();
    std::cout << "[info] depth_scale: " << depth_scale << " m/LSB\n";

    auto vsp = profile.get_stream(RS2_STREAM_DEPTH).as<rs2::video_stream_profile>();
    auto intr = vsp.get_intrinsics();
    const float fx = intr.fx, fy = intr.fy, cx = intr.ppx, cy = intr.ppy;

    // ---- 스캔 루프 ----
    const float step_deg = 360.0f / frame_num;
    std::vector<float> xyz;  // 최종 누적 포인트
    xyz.reserve(size_t(848*480) * 3 * size_t(frame_num/4 + 1)); // 대략적 예약

    for (int k = 0; k < frame_num; ++k) {
        const float target_yaw = step_deg * k;

        // 1) 모터 회전 (플레이스홀더)
        motor::rotate_to_deg(step_deg);

        // 2) 프레임 획득
        rs2::frameset fs = pipe.wait_for_frames(5000);
        rs2::depth_frame d = fs.get_depth_frame();
        if (!d) { std::cerr << "[warn] no depth frame at step " << k << "\n"; continue; }

        // 3) 딥카피 → Depth
        oops::Depth snap;
        const double ts_ms = d.get_timestamp(); // 센서 timestamp(ms)
        oops::copyDepthData(snap, target_yaw, cam_pitch_deg, ts_ms, d, depth_scale);

        // 4) threshold + (X,Y,Z)로 변환 (월드 좌표: pitch, yaw(+π), translation 반영)
        depth_to_pointcloud(snap, fx, fy, cx, cy,
                            z_min_m, z_max_m,
                            radius_m,         // ★ 반지름 전달
                            cam_height_m,     // ★ 높이 전달(기본 0.0)
                            xyz);

        std::cout << "[step " << k+1 << "/" << frame_num << "] yaw=" << target_yaw
                  << " deg, points so far=" << (xyz.size()/3) << "\n";
    }

    pipe.stop();

    // ---- PLY 저장 ----
    if (!write_ply_binary_le(out_path, xyz)) {
        std::cerr << "ERROR: failed to write PLY to " << out_path << "\n";
        return 1;
    }
    std::cout << "[ok] wrote PLY: " << out_path
              << " (" << (xyz.size()/3) << " points)\n";

    // ---- 서버 업로드 (선택) ----
    if (!server_url.empty()) {
        if (!http_upload_file(server_url, out_path)) {
            std::cerr << "ERROR: upload failed\n";
            return 2;
        }
        std::cout << "[ok] uploaded to: " << server_url << "\n";
    }

    return 0;
}
catch (const rs2::error& e) {
    std::cerr << "RealSense error: " << e.what() << "\n"; return 1;
}
catch (const std::exception& e) {
    std::cerr << "Std error: " << e.what() << "\n"; return 1;
}
