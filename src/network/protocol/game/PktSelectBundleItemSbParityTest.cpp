// Parity gate for ServerboundSelectBundleItemPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktSelectBundleItemSbParity.java ground truth).
//
// The packet is a record(int slotId, int selectedItemIndex). Its body is exactly:
//   write : output.writeVarInt(this.slotId); output.writeVarInt(this.selectedItemIndex);
//   read  : this(input.readVarInt(), input.readVarInt());
// (net.minecraft.network.protocol.game.ServerboundSelectBundleItemPacket lines 8-23.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body, so the whole
// wire payload is two VarInts (LEB128, signed, no zig-zag): slotId then selectedItemIndex.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt / readVarInt are byte-for-byte / value-for-value the same as the real codec
// (no zig-zag -- negatives encode as 5 bytes).
//
//   pkt_select_bundle_item_sb_parity [--cases mcpp/build/pkt_select_bundle_item_sb.tsv]
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
    std::string casesPath = "mcpp/build/pkt_select_bundle_item_sb.tsv";
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

        // ENC <slotId-dec> <selectedItemIndex-dec> <readableBytes-dec> <hex>
        std::string slotStr, selStr, lenStr, expHex;
        if (!std::getline(ss, slotStr, '\t') || !std::getline(ss, selStr, '\t')
            || !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t slotId = (int32_t)std::stoll(slotStr);
        int32_t selectedItemIndex = (int32_t)std::stoll(selStr);
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): writeVarInt(slotId) then writeVarInt(selectedItemIndex), in that order.
        PacketBuffer enc;
        enc.writeVarInt(slotId);
        enc.writeVarInt(selectedItemIndex);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH slot=" << slotId << " sel=" << selectedItemIndex
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH slot=" << slotId << " sel=" << selectedItemIndex
                      << " got=" << enc.data().size() << " want=" << expLen << "\n";
        }

        // read(): two readVarInt calls must recover the exact signed ints in order.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotSlot = dec.readVarInt();
        int32_t gotSel = dec.readVarInt();
        if (gotSlot != slotId || gotSel != selectedItemIndex) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << " got=(" << gotSlot << ","
                      << gotSel << ") want=(" << slotId << "," << selectedItemIndex << ")\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING slot=" << slotId << " sel=" << selectedItemIndex
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktSelectBundleItemSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
