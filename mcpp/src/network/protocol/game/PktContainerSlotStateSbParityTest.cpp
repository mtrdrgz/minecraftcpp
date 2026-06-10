// Parity gate for ServerboundContainerSlotStateChangedPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktContainerSlotStateSbParity.java ground truth).
//
// The packet is a record(int slotId, int containerId, boolean newState). Its body is exactly:
//   write : output.writeVarInt(this.slotId);
//           output.writeContainerId(this.containerId);   // == VarInt.write (no zig-zag)
//           output.writeBoolean(this.newState);
//   read  : this(input.readVarInt(), input.readContainerId(), input.readBoolean());
// (net.minecraft.network.protocol.game.ServerboundContainerSlotStateChangedPacket lines 13-21;
//  FriendlyByteBuf.writeContainerId/readContainerId lines 671-685 are plain VarInt.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body, so the whole wire
// payload is VarInt(slotId) ++ VarInt(containerId) ++ byte(newState ? 1 : 0).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly: writeVarInt /
// readVarInt are byte-for-byte / value-for-value the same as the real codec (no zig-zag --
// negatives encode as 5 bytes), and writeBool/readBool match writeBoolean/readBoolean.
//
//   pkt_container_slot_state_sb_parity [--cases mcpp/build/pkt_container_slot_state_sb.tsv]
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
    std::string casesPath = "mcpp/build/pkt_container_slot_state_sb.tsv";
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

        // ENC <slotId-dec> <containerId-dec> <newState-dec(0|1)> <readableBytes-dec> <hex>
        std::string slotStr, cidStr, stateStr, lenStr, expHex;
        if (!std::getline(ss, slotStr, '\t') || !std::getline(ss, cidStr, '\t')
            || !std::getline(ss, stateStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t slotId = (int32_t)std::stoll(slotStr);
        int32_t containerId = (int32_t)std::stoll(cidStr);
        bool newState = (std::stoi(stateStr) != 0);
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): writeVarInt(slotId), writeVarInt(containerId), writeBool(newState).
        PacketBuffer enc;
        enc.writeVarInt(slotId);
        enc.writeVarInt(containerId);
        enc.writeBool(newState);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH slot=" << slotId << " cid=" << containerId
                      << " state=" << (newState ? 1 : 0)
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH slot=" << slotId << " cid=" << containerId
                      << " state=" << (newState ? 1 : 0)
                      << " got=" << enc.data().size() << " want=" << expLen << "\n";
        }

        // read(): readVarInt, readVarInt, readBool must recover the exact fields in order.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotSlot = dec.readVarInt();
        int32_t gotCid = dec.readVarInt();
        bool gotState = dec.readBool();
        if (gotSlot != slotId || gotCid != containerId || gotState != newState) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << " got=(" << gotSlot << ","
                      << gotCid << "," << (gotState ? 1 : 0) << ") want=(" << slotId << ","
                      << containerId << "," << (newState ? 1 : 0) << ")\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING slot=" << slotId << " cid=" << containerId
                      << " state=" << (newState ? 1 : 0)
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktContainerSlotStateSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
