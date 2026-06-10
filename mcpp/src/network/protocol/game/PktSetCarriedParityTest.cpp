// Parity gate for ServerboundSetCarriedItemPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktSetCarriedParity.java ground truth).
//
// The packet body is exactly:
//   write : FriendlyByteBuf.writeShort(slot)   (low 16 bits, big-endian)
//   read  : FriendlyByteBuf.readShort()        (signed 16-bit, sign-extended to int)
// (net.minecraft.network.protocol.game.ServerboundSetCarriedItemPacket lines 18-24,
//  Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.)
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeShort truncates to the low 16 bits big-endian and readShort sign-extends,
// matching netty's ByteBuf.writeShort(int)/readShort() byte-for-byte.
//
//   pkt_set_carried_parity [--cases mcpp/build/pkt_set_carried.tsv]
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
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_set_carried.tsv";
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
            // ENC <slot_in> <hex>
            std::string slotStr, expHex;
            if (!std::getline(ss, slotStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t slot = (int32_t)std::stoll(slotStr);

            // write(): FriendlyByteBuf.writeShort(slot) -> low 16 bits big-endian.
            PacketBuffer enc;
            enc.writeShort((int16_t)slot);   // int -> int16_t == low 16 bits, exactly netty's writeShort(int)
            std::string got = hex(enc.data());
            if (got != expHex) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH slot=" << slot << "\n  got  " << got
                          << "\n  want " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hex> <slot_in> <slot_decoded>
            std::string inHex, slotStr, decStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, slotStr, '\t')
                || !std::getline(ss, decStr)) continue;
            ++cases;
            int32_t expSlot = (int32_t)std::stoll(decStr);

            // read(): FriendlyByteBuf.readShort() -> signed short sign-extended to int.
            std::vector<uint8_t> bytes;
            for (size_t i = 0; i + 1 < inHex.size(); i += 2)
                bytes.push_back((uint8_t)std::stoi(inHex.substr(i, 2), nullptr, 16));
            PacketBuffer dec(bytes);
            int32_t gotSlot = (int32_t)dec.readShort();   // int16_t -> int32_t sign-extends
            if (gotSlot != expSlot) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << " got=" << gotSlot
                          << " want=" << expSlot << "\n";
            }
        }
    }

    std::cout << "PktSetCarriedParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
