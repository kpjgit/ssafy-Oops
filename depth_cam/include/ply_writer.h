#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

// 매우 단순한 Binary Little-Endian PLY 포인트 클라우드 저장기
// 포맷: vertex only (x,y,z) float32
inline bool write_ply_binary_le(const std::string& path,
                                const std::vector<float>& xyz /* len = 3*N */)
{
    const size_t N = xyz.size() / 3;
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    // header
    f << "ply\nformat binary_little_endian 1.0\n";
    f << "element vertex " << N << "\n";
    f << "property float x\nproperty float y\nproperty float z\n";
    f << "end_header\n";

    // data
    f.write(reinterpret_cast<const char*>(xyz.data()),
            std::streamsize(xyz.size() * sizeof(float)));
    return true;
}

