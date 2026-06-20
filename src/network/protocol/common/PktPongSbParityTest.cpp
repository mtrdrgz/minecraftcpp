// ServerboundPongPacket codec parity vs the REAL net.minecraft StreamCodec
// (tools/PktPongSbParity.java ground truth).
//
// ServerboundPongPacket.STREAM_CODEC body == FriendlyByteBuf.writeInt(id), a single
// 4-byte big-endian int. The C++ side encodes the same `id` via PacketBuffer.writeInt
// and must produce byte-for-byte identical bytes, report the same readableBytes count,
// and decode the Java bytes back to the identical id.
//
// TSV rows:  PONG_SB <id> <readableBytes> <hex> <decodedId>
//
//   pkt_pong_sb_parity [--cases mcpp/build/pkt_pong_sb.tsv]
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
    std::string casesPath = "build/pkt_pong_sb.tsv";
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
        std::string tag, idStr, lenStr, expHex, decStr;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "PONG_SB") continue;
        if (!std::getline(ss, idStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex, '\t') || !std::getline(ss, decStr)) continue;
        ++cases;

        int32_t id = (int32_t)std::stoll(idStr);
        size_t expLen = (size_t)std::stoull(lenStr);
        int32_t expDec = (int32_t)std::stoll(decStr);

        bool ok = true;

        // (a) ENCODE: writeInt(id) -> 4 big-endian bytes, byte-for-byte vs Java.
        PacketBuffer enc;
        enc.writeInt(id);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ok = false;
            std::cerr << "ENC-MISMATCH id=" << id << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ok = false;
            std::cerr << "LEN-MISMATCH id=" << id << " got " << enc.data().size()
                      << " want " << expLen << "\n";
        }

        // (b) DECODE the Java bytes back -> identical id (and matches Java's decoded id).
        PacketBuffer dec(unhex(expHex));
        int32_t roundTrip = dec.readInt();
        if (roundTrip != id || roundTrip != expDec) {
            ok = false;
            std::cerr << "DECODE-MISMATCH id=" << id << " got " << roundTrip
                      << " want " << id << " (javaDecoded=" << expDec << ")\n";
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktPongSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
