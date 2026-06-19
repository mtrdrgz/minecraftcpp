// Parity test for mc::uuidutil (core/UUIDUtil.h) vs Java net.minecraft.core.UUIDUtil.
// Reads the TSV emitted by UuidUtilParity.java and compares value-for-value.
//
//   FROMARR  <i0> <i1> <i2> <i3> <msb> <lsb>
//   TOARR    <msb> <lsb> <i0> <i1> <i2> <i3>
//   LMTOARR  <msb> <lsb> <i0> <i1> <i2> <i3>
//   OFFLINE  <nameHexUtf8> <msb> <lsb>
#include "core/UUIDUtil.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Parse a signed 64-bit decimal (Java prints signed longs in decimal).
static int64_t pl(const std::string& s) { return std::stoll(s); }
// Parse a signed 32-bit decimal.
static int32_t pi(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Decode lowercase hex (raw UTF-8 bytes) back into a byte string.
static std::string from_hex(const std::string& h) {
    std::string out;
    out.reserve(h.size() / 2);
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    for (std::size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back(static_cast<char>((nib(h[i]) << 4) | nib(h[i + 1])));
    return out;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: UuidUtilParityTest --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "FROMARR") {
            // FROMARR <i0> <i1> <i2> <i3> <msb> <lsb>
            ++cases;
            std::array<int32_t, 4> a{ pi(p[1]), pi(p[2]), pi(p[3]), pi(p[4]) };
            int64_t emsb = pl(p[5]), elsb = pl(p[6]);
            mc::uuidutil::Uuid got = mc::uuidutil::uuidFromIntArray(a);
            if (got.mostSigBits != emsb || got.leastSigBits != elsb) {
                ++mism;
                std::cerr << "FROMARR mismatch in=[" << a[0] << "," << a[1] << ","
                          << a[2] << "," << a[3] << "] expected=(" << emsb << ","
                          << elsb << ") got=(" << got.mostSigBits << ","
                          << got.leastSigBits << ")\n";
            }
        } else if (tag == "TOARR" || tag == "LMTOARR") {
            // <TAG> <msb> <lsb> <i0> <i1> <i2> <i3>
            ++cases;
            int64_t msb = pl(p[1]), lsb = pl(p[2]);
            std::array<int32_t, 4> e{ pi(p[3]), pi(p[4]), pi(p[5]), pi(p[6]) };
            std::array<int32_t, 4> got =
                (tag == "TOARR")
                    ? mc::uuidutil::uuidToIntArray(mc::uuidutil::Uuid{ msb, lsb })
                    : mc::uuidutil::leastMostToIntArray(msb, lsb);
            if (got != e) {
                ++mism;
                std::cerr << tag << " mismatch msb=" << msb << " lsb=" << lsb
                          << " expected=[" << e[0] << "," << e[1] << "," << e[2]
                          << "," << e[3] << "] got=[" << got[0] << "," << got[1]
                          << "," << got[2] << "," << got[3] << "]\n";
            }
        } else if (tag == "OFFLINE") {
            // OFFLINE <nameHexUtf8> <msb> <lsb>
            ++cases;
            std::string name = from_hex(p[1]);
            int64_t emsb = pl(p[2]), elsb = pl(p[3]);
            mc::uuidutil::Uuid got = mc::uuidutil::createOfflinePlayerUUID(name);
            if (got.mostSigBits != emsb || got.leastSigBits != elsb) {
                ++mism;
                std::cerr << "OFFLINE mismatch nameHex=" << p[1] << " expected=("
                          << emsb << "," << elsb << ") got=(" << got.mostSigBits
                          << "," << got.leastSigBits << ")\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "UuidUtil cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
