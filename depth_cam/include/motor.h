#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>

namespace motor {

static const char* FIXED_MAC = "00:00:00:00:00:00";   // ★ ESP32-MAC 주소
static const uint8_t FIXED_CHANNEL = 1;               // ★ SPP 채널
static int bt_sock = -1;
static bdaddr_t bdaddr{};

// ---- 소켓 닫기 ----
inline void disconnect() {
    if (bt_sock >= 0) {
        close(bt_sock);
        bt_sock = -1;
    }
}

// ---- 연결 ----
inline bool connect(int timeout_ms = 5000) {
    if (bt_sock >= 0) return true; // 이미 연결됨

    // MAC → BDADDR 변환 (한번만)
    static bool addr_inited = false;
    if (!addr_inited) {
        if (str2ba(FIXED_MAC, &bdaddr) != 0) {
            std::cerr << "[motor] invalid FIXED_MAC\n";
            return false;
        }
        addr_inited = true;
    }

    // 소켓 생성
    bt_sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (bt_sock < 0) {
        std::cerr << "[motor] socket() failed: " << strerror(errno) << "\n";
        return false;
    }

    // 비블록으로 변경하여 타임아웃 제어
    int flags = fcntl(bt_sock, F_GETFL, 0);
    fcntl(bt_sock, F_SETFL, flags | O_NONBLOCK);

    sockaddr_rc addr {};
    addr.rc_family = AF_BLUETOOTH;
    addr.rc_channel = FIXED_CHANNEL;
    addr.rc_bdaddr = bdaddr;

    int rc = ::connect(bt_sock, (struct sockaddr*)&addr, sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) {
        std::cerr << "[motor] connect() fail: " << strerror(errno) << "\n";
        disconnect();
        return false;
    }

    // 연결 완료 대기
    pollfd pfd { bt_sock, POLLOUT, 0 };
    int pr = poll(&pfd, 1, timeout_ms);
    if (pr <= 0) {
        std::cerr << "[motor] connect() timeout\n";
        disconnect();
        return false;
    }

    // 에러 체크
    int so_error = 0;
    socklen_t len = sizeof(so_error);
    getsockopt(bt_sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error != 0) {
        std::cerr << "[motor] connect() error: " << strerror(so_error) << "\n";
        disconnect();
        return false;
    }

    // 블록 모드 복귀
    fcntl(bt_sock, F_SETFL, flags);

    std::cout << "[motor] Connected to ESP32 (" << FIXED_MAC << ")\n";
    return true;
}

// ---- 내부: 라인 읽기 (개행 기준) ----
inline int read_line_timeout(char* out, size_t maxlen, int timeout_ms) {
    if (bt_sock < 0) return -1;

    size_t used = 0;
    out[0] = '\0';

    while (used + 1 < maxlen) {
        pollfd pfd { bt_sock, POLLIN, 0 };
        int pr = poll(&pfd, 1, timeout_ms);
        if (pr <= 0) return -1;

        if (pfd.revents & POLLIN) {
            char buf[128];
            int n = ::read(bt_sock, buf, sizeof(buf));
            if (n <= 0) return -1;
            for (int i = 0; i < n && used + 1 < maxlen; i++) {
                out[used++] = buf[i];
                if (buf[i] == '\n') {
                    out[used] = '\0';
                    return used;
                }
            }
            out[used] = '\0';
        }
    }
    out[used] = '\0';
    return used;
}

// ---- 연결 보장 ----
inline bool ensure_connected() {
    if (bt_sock >= 0) return true;
    return connect();
}

// ---- 명령 전송 + OK 응답 받기 ----
inline bool rotate_to_deg(float angle_deg, int timeout_ms = 3000) {
    if (!ensure_connected()) return false;

    char msg[64];
    std::snprintf(msg, sizeof(msg), "%.2f\n", angle_deg);

    if (::write(bt_sock, msg, std::strlen(msg)) < 0) {
        std::cerr << "[motor] write fail: " << strerror(errno) << "\n";
        disconnect();
        return false;
    }

    char line[256];
    int n = read_line_timeout(line, sizeof(line), timeout_ms);
    if (n <= 0) {
        std::cout << "[motor] rotate " << angle_deg << " → NO RESPONSE\n";
        return false;
    }

    // trim
    std::string s(line);
    while (!s.empty() && (s.back()=='\n' || s.back()=='\r' || s.back()==' '))
        s.pop_back();

    if (s == "OK") {
        std::cout << "[motor] rotate " << angle_deg << " → OK\n";
        return true;
    }
    std::cout << "[motor] rotate " << angle_deg << " → RESP='" << s << "'\n";
    return false;
}

} // namespace motor
