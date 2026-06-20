// Bit-exact parity gate for net.minecraft.network.VarLong:
//   * getByteSize(long)  -> int (1..10)
//   * write(...)         -> LEB128-style byte sequence
//
// Ground truth: tools/VarLongSizeParity.java (the REAL decompiled VarLong).
// TSV rows: SIZE  <value(decimal long)>  <byteSize(decimal)>  <encoded hex>
//
//   varlong_size_parity --cases mcpp/build/varlong_size.tsv
#include "../../../network/VarLong.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/varlong_size.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tag, valStr, sizeStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "SIZE") continue;
        if (!std::getline(ss, valStr, '\t') || !std::getline(ss, sizeStr, '\t')) continue;
        std::getline(ss, expHex); // may be empty only if value==0 produces 1 byte "00"; never empty here

        ++cases;

        int64_t value = (int64_t)std::stoll(valStr);
        int expSize = std::stoi(sizeStr);

        int gotSize = mc::net::varlong::getByteSize(value);
        if (gotSize != expSize) {
            ++mismatches;
            std::cerr << "SIZE-MISMATCH value=" << valStr
                      << " got=" << gotSize << " want=" << expSize << "\n";
        }

        std::vector<uint8_t> enc;
        mc::net::varlong::write(enc, value);
        std::string gotHex = hex(enc);
        if (gotHex != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH value=" << valStr
                      << "\n  got  " << gotHex
                      << "\n  want " << expHex << "\n";
        }

        // Cross-check: the encoded length MUST equal getByteSize (Java invariant).
        if ((int)enc.size() != expSize) {
            ++mismatches;
            std::cerr << "LEN-INVARIANT value=" << valStr
                      << " enc.size()=" << enc.size() << " size=" << expSize << "\n";
        }
    }

    std::cout << "VarLongSizeParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
