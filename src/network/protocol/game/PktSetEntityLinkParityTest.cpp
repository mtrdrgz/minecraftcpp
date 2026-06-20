// Parity gate for net.minecraft.network.protocol.game.ClientboundSetEntityLinkPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktSetEntityLinkParity.java GT).
//
// The packet body is exactly (ClientboundSetEntityLinkPacket.java:22-30):
//   write : FriendlyByteBuf.writeInt(this.sourceId)   // big-endian 4-byte int
//           FriendlyByteBuf.writeInt(this.destId)      // big-endian 4-byte int
//   read  : this.sourceId = input.readInt();
//           this.destId   = input.readInt();
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// Both fields are plain signed 32-bit ints, so this exercises the certified PacketBuffer
// (the FriendlyByteBuf port) directly: writeInt/readInt are big-endian 4-byte and are
// byte-for-byte identical to the real codec across the full signed 32-bit range.
//
//   pkt_set_entity_link_parity [--cases mcpp/build/pkt_set_entity_link.tsv]
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
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_set_entity_link.tsv";
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
        std::string tag;
        if (!std::getline(ss, tag, '\t')) continue;

        if (tag == "ENC") {
            // ENC <sourceId> <destId> <readableBytes> <hexBytes>
            std::string srcStr, dstStr, nStr, expHex;
            if (!std::getline(ss, srcStr, '\t') || !std::getline(ss, dstStr, '\t')
                || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t sourceId = (int32_t)std::stoll(srcStr);
            int32_t destId = (int32_t)std::stoll(dstStr);
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeInt(sourceId) then writeInt(destId), both big-endian 4-byte.
            PacketBuffer enc;
            enc.writeInt(sourceId);
            enc.writeInt(destId);

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH sourceId=" << sourceId << " destId=" << destId
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <sourceId_in> <destId_in> <sourceId_decoded> <destId_decoded>
            std::string inHex, srcStr, dstStr, decSrcStr, decDstStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, srcStr, '\t')
                || !std::getline(ss, dstStr, '\t') || !std::getline(ss, decSrcStr, '\t')
                || !std::getline(ss, decDstStr)) continue;
            ++cases;
            int32_t expSrc = (int32_t)std::stoll(decSrcStr);
            int32_t expDst = (int32_t)std::stoll(decDstStr);

            // read(): readInt() then readInt(), both big-endian 4-byte.
            PacketBuffer dec(unhex(inHex));
            int32_t gotSrc = dec.readInt();
            int32_t gotDst = dec.readInt();
            if (gotSrc != expSrc || gotDst != expDst) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " got src=" << gotSrc << " dst=" << gotDst
                          << " want src=" << expSrc << " dst=" << expDst << "\n";
            }
        }
    }

    std::cout << "PktSetEntityLinkParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
