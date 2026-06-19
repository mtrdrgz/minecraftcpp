// Parity gate for ClientboundSetHeldSlotPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktSetHeldSlotCbParity.java ground truth).
//
// net.minecraft.network.protocol.game.ClientboundSetHeldSlotPacket is a record
// with a single int field `slot`, encoded via:
//   STREAM_CODEC = StreamCodec.composite(ByteBufCodecs.VAR_INT, ...::slot, ...::new)
// ByteBufCodecs.VAR_INT is exactly VarInt.write/read (LEB128). So the packet body
// is exactly ONE VarInt(slot) — no packet-id prefix.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt / readVarInt match VarInt.write/read byte-for-byte.
//
//   pkt_set_held_slot_cb_parity [--cases mcpp/build/pkt_set_held_slot_cb.tsv]
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
    std::string casesPath = "mcpp/build/pkt_set_held_slot_cb.tsv";
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
            // ENC <slot_in> <readableBytes> <hex>
            std::string slotStr, readableStr, expHex;
            if (!std::getline(ss, slotStr, '\t') || !std::getline(ss, readableStr, '\t')
                || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t slot = (int32_t)std::stoll(slotStr);
            size_t expReadable = (size_t)std::stoull(readableStr);

            // write(): body is exactly VarInt(slot).
            PacketBuffer enc;
            enc.writeVarInt(slot);
            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expReadable) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH slot=" << slot
                          << "\n  got  " << got << " (" << enc.size() << "B)"
                          << "\n  want " << expHex << " (" << expReadable << "B)\n";
            }
        } else if (tag == "DEC") {
            // DEC <hex> <slot_in> <slot_decoded>
            std::string inHex, slotStr, decStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, slotStr, '\t')
                || !std::getline(ss, decStr)) continue;
            ++cases;
            int32_t expSlot = (int32_t)std::stoll(decStr);

            // read(): body is exactly VarInt(slot).
            PacketBuffer dec(unhex(inHex));
            int32_t gotSlot = dec.readVarInt();
            if (gotSlot != expSlot) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << " got=" << gotSlot
                          << " want=" << expSlot << "\n";
            }
        }
    }

    std::cout << "PktSetHeldSlotCbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
