// Parity gate for ServerboundPongPacket's StreamCodec vs the REAL net.minecraft codec
// (tools/PktPongGameSbParity.java ground truth).
//
// (The assignment hint placed this in protocol.game; in 26.1.2 the real class lives in
//  protocol.common — net/minecraft/network/protocol/common/ServerboundPongPacket.java.
//  The test/target keep the assigned "game_sb" naming.)
//
// The packet has a single field `int id`. Its body is exactly:
//   write : FriendlyByteBuf.writeInt(this.id)
//   read  : input.readInt()
// (net.minecraft.network.protocol.common.ServerboundPongPacket.java lines 16-22.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body, so the whole
// wire payload is a single big-endian 4-byte signed int `id`.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeInt(id) is writeBE<uint32_t> (4-byte big-endian, no VarInt / no zig-zag) and
// readInt() is its inverse — byte-for-byte / value-for-value the same as the real codec.
//
//   pkt_pong_game_sb_parity [--cases mcpp/build/pkt_pong_game_sb.tsv]
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
    std::string casesPath = "build/pkt_pong_game_sb.tsv";
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
        if (tag != "ENC") continue;

        // ENC <id-dec> <readableBytes-dec> <hex>
        std::string idStr, lenStr, expHex;
        if (!std::getline(ss, idStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;
        // id is a signed 32-bit int; parse wide then narrow to int32_t.
        int32_t id = (int32_t)std::stoll(idStr);
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): FriendlyByteBuf.writeInt(id) -> 4-byte big-endian.
        PacketBuffer enc;
        enc.writeInt(id);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH id=" << id << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH id=" << id << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // read(): input.readInt() must recover the exact signed int.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotId = dec.readInt();
        if (gotId != id) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << " got=" << gotId
                      << " want=" << id << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING id=" << id << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktPongGameSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
