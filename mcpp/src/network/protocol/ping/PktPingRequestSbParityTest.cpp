// Byte-exact parity gate for net.minecraft.network.protocol.ping.ServerboundPingRequestPacket.
//
// Ground truth: tools/PktPingRequestSbParity.java (encoded through the REAL STREAM_CODEC).
// The packet body is exactly `output.writeLong(this.time)` (8 big-endian bytes), so the
// C++ side exercises the certified mc::net::PacketBuffer directly — writeLong(time) must
// reproduce the Java wire bytes byte-for-byte, with the same byte count, and readLong()
// must round-trip the value. No packet class header is needed.
//
// Row format:  ENC <time-decimal> <readableBytes> <hex>
//
//   pkt_ping_request_sb_parity [--cases mcpp/build/pkt_ping_request_sb.tsv]
#include "../../PacketBuffer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::PacketBuffer;

namespace {
std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> v;
    v.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return v;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_ping_request_sb.tsv";
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
        std::string tag, timeStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t') || !std::getline(ss, timeStr, '\t')
            || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
        if (tag != "ENC") continue;
        ++cases;

        int64_t time = (int64_t)std::stoll(timeStr);
        size_t expN = (size_t)std::stoull(nStr);

        // (1) ENCODE: write(ByteBuf) == writeLong(time) -> 8 big-endian bytes.
        PacketBuffer enc;
        enc.writeLong(time);

        std::string got = hex(enc.data());
        bool ok = true;

        if (got != expHex) {
            ok = false;
            std::cerr << "ENC-MISMATCH time=" << time << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expN) {
            ok = false;
            std::cerr << "LEN-MISMATCH time=" << time << " got=" << enc.data().size()
                      << " want=" << expN << "\n";
        }

        // (2) DECODE: read the Java bytes back through the C++ readLong() path.
        PacketBuffer dec(unhex(expHex));
        int64_t back = dec.readLong();
        if (back != time) {
            ok = false;
            std::cerr << "DECODE-MISMATCH want time=" << time << " got=" << back << "\n";
        }
        if (dec.remaining() != 0) {
            ok = false;
            std::cerr << "TRAILING-BYTES time=" << time << " remaining=" << dec.remaining() << "\n";
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktPingRequestSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
