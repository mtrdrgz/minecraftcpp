// lp_vec3_parity — bit-exact gate for net.minecraft.network.LpVec3 (26.1.2).
//
// Reads the TSV emitted by mcpp/tools/LpVec3Parity.java and, for each row:
//   WRITE  <ix> <iy> <iz>(double bits)  <hex>(encoded bytes)
//          -> LpVec3::write(buf, Vec3) and compare the produced bytes to <hex>.
//   READ   <hex>(input bytes)  <ox> <oy> <oz>(double bits)
//          -> LpVec3::read(buf) and compare each component's raw bits.
//
// All comparisons are bit-for-bit (std::bit_cast on the doubles, exact byte
// string on the encoding). Prints "LpVec3 cases=N mismatches=M"; exit M==0?0:1.

#include "LpVec3.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

double bitsToDouble(const std::string& hex) {
    uint64_t bits = std::stoull(hex, nullptr, 16);
    return std::bit_cast<double>(bits);
}

uint64_t doubleToBits(double d) {
    return std::bit_cast<uint64_t>(d);
}

std::string bytesToHex(const std::vector<uint8_t>& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        out.push_back(digits[(b >> 4) & 0xF]);
        out.push_back(digits[b & 0xF]);
    }
    return out;
}

std::vector<uint8_t> hexToBytes(const std::string& hex) {
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        uint8_t hi = static_cast<uint8_t>(std::stoul(hex.substr(i, 1), nullptr, 16));
        uint8_t lo = static_cast<uint8_t>(std::stoul(hex.substr(i + 1, 1), nullptr, 16));
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> fields;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, delim)) {
        fields.push_back(cur);
    }
    return fields;
}

} // namespace

int main(int argc, char** argv) {
    std::string tsvPath;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            tsvPath = argv[++i];
        }
    }
    if (tsvPath.empty()) {
        std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
        return 2;
    }

    std::ifstream in(tsvPath);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", tsvPath.c_str());
        return 2;
    }

    int cases = 0;
    int mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        std::vector<std::string> f = split(line, '\t');
        if (f.empty()) {
            continue;
        }

        const std::string& tag = f[0];

        if (tag == "WRITE") {
            // WRITE <ix> <iy> <iz> <hex>
            if (f.size() < 5) {
                continue;
            }
            double x = bitsToDouble(f[1]);
            double y = bitsToDouble(f[2]);
            double z = bitsToDouble(f[3]);
            const std::string& expectedHex = f[4];

            mc::net::LpByteBuf buf;
            mc::net::LpVec3::write(buf, mc::Vec3{x, y, z});
            std::string gotHex = bytesToHex(buf.data);

            cases++;
            if (gotHex != expectedHex) {
                mismatches++;
                std::fprintf(stderr,
                             "WRITE mismatch: in=(%.17g,%.17g,%.17g) expected=%s got=%s\n",
                             x, y, z, expectedHex.c_str(), gotHex.c_str());
            }
        } else if (tag == "READ") {
            // READ <hex> <ox> <oy> <oz>
            if (f.size() < 5) {
                continue;
            }
            const std::string& inHex = f[1];
            uint64_t ex = std::stoull(f[2], nullptr, 16);
            uint64_t ey = std::stoull(f[3], nullptr, 16);
            uint64_t ez = std::stoull(f[4], nullptr, 16);

            mc::net::LpByteBuf buf;
            buf.data = hexToBytes(inHex);
            buf.pos = 0;
            mc::Vec3 out = mc::net::LpVec3::read(buf);

            uint64_t gx = doubleToBits(out.x);
            uint64_t gy = doubleToBits(out.y);
            uint64_t gz = doubleToBits(out.z);

            cases++;
            if (gx != ex || gy != ey || gz != ez) {
                mismatches++;
                std::fprintf(stderr,
                             "READ mismatch: bytes=%s\n  expected x=%016llx y=%016llx z=%016llx\n"
                             "  got      x=%016llx y=%016llx z=%016llx\n",
                             inHex.c_str(),
                             (unsigned long long)ex, (unsigned long long)ey, (unsigned long long)ez,
                             (unsigned long long)gx, (unsigned long long)gy, (unsigned long long)gz);
            }
        }
        // unknown tags ignored
    }

    std::printf("LpVec3 cases=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
