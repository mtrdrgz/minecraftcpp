// Byte-exact parity gate for net.minecraft.network.protocol.common.ServerboundKeepAlivePacket.
//
// Ground truth: tools/PktKeepAliveSbParity.java (encoded through the REAL STREAM_CODEC).
// Row format:  ENC <id-decimal> <readableBytes> <hex>
//
// For each row the C++ ServerboundKeepAlivePacket::write must produce the IDENTICAL
// bytes (8 big-endian, the writeLong body), the same byte count, and read() must
// round-trip the id.
//
//   pkt_keepalive_sb_parity [--cases mcpp/build/pkt_keepalive_sb.tsv]
#include "ServerboundKeepAlivePacket.h"
#include "../../PacketBuffer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::PacketBuffer;
using mc::net::protocol::common::ServerboundKeepAlivePacket;

namespace {
std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> v;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return v;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_keepalive_sb.tsv";
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
        std::string tag, idStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t') || !std::getline(ss, idStr, '\t')
            || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
        if (tag != "ENC") continue;
        ++cases;

        int64_t id = (int64_t)std::stoll(idStr);
        size_t expN = (size_t)std::stoull(nStr);

        // Encode via the C++ packet write (== writeLong(id)).
        ServerboundKeepAlivePacket pkt{ (long long)id };
        PacketBuffer enc;
        pkt.write(enc);

        std::string got = hex(enc.data());
        bool ok = true;

        if (got != expHex) {
            ok = false;
            std::cerr << "ENC-MISMATCH id=" << id << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expN) {
            ok = false;
            std::cerr << "LEN-MISMATCH id=" << id << " got=" << enc.data().size()
                      << " want=" << expN << "\n";
        }

        // Round-trip decode the Java bytes through the C++ read() path.
        PacketBuffer dec(unhex(expHex));
        ServerboundKeepAlivePacket back = ServerboundKeepAlivePacket::read(dec);
        if ((int64_t)back.getId() != id) {
            ok = false;
            std::cerr << "DECODE-MISMATCH want id=" << id
                      << " got=" << (int64_t)back.getId() << "\n";
        }

        if (!ok) ++mismatches;
    }
    std::cout << "PktKeepAliveSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
