// ClientboundPongResponsePacket codec parity vs the REAL net.minecraft StreamCodec
// (tools/PktPongResponseCbParity.java ground truth).
//
// `public record ClientboundPongResponsePacket(long time)` — its STREAM_CODEC body is
// exactly FriendlyByteBuf.writeLong(time), a single 8-byte big-endian long
// (read side: readLong()). The C++ side encodes the same `time` via
// PacketBuffer.writeLong and must produce byte-for-byte identical bytes, report the
// same readableBytes count, and decode the Java bytes back to the identical time.
//
// TSV rows:  ENC <time-decimal> <readableBytes> <hex>
//
//   pkt_pong_response_cb_parity [--cases mcpp/build/pkt_pong_response_cb.tsv]
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
    std::string casesPath = "mcpp/build/pkt_pong_response_cb.tsv";
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
        std::string tag, timeStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, timeStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        // Java prints longs as signed decimal; parse via stoll (covers Long.MIN/MAX).
        int64_t time = (int64_t)std::stoll(timeStr);
        size_t expLen = (size_t)std::stoull(lenStr);

        bool ok = true;

        // (a) ENCODE: writeLong(time) -> 8 big-endian bytes, byte-for-byte vs Java.
        PacketBuffer enc;
        enc.writeLong(time);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ok = false;
            std::cerr << "ENC-MISMATCH time=" << time << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ok = false;
            std::cerr << "LEN-MISMATCH time=" << time << " got " << enc.data().size()
                      << " want " << expLen << "\n";
        }

        // (b) DECODE the Java bytes back -> identical time.
        PacketBuffer dec(unhex(expHex));
        int64_t roundTrip = dec.readLong();
        if (roundTrip != time) {
            ok = false;
            std::cerr << "DECODE-MISMATCH time=" << time << " got " << roundTrip << "\n";
        }
        if (dec.remaining() != 0) {
            ok = false;
            std::cerr << "LEFTOVER-BYTES time=" << time << " remaining=" << dec.remaining() << "\n";
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktPongResponseCbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
