// Parity gate for ServerboundPickItemFromEntityPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktPickItemEntitySbParity.java ground truth).
//
// The packet is a record(int id, boolean includeData). Its STREAM_CODEC is
// StreamCodec.composite(ByteBufCodecs.VAR_INT, ...id, ByteBufCodecs.BOOL, ...includeData, ...).
// Body = writeVarInt(id) ++ writeBoolean(includeData), no packet-id prefix.
// (net.minecraft.network.protocol.game.ServerboundPickItemFromEntityPacket lines 9-16;
//  ByteBufCodecs.VAR_INT -> writeVarInt/readVarInt, ByteBufCodecs.BOOL -> writeBoolean/
//  readBoolean, a single 0x00/0x01 byte.)
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt/readVarInt are byte-for-byte / value-for-value the same as the real codec
// (no zig-zag -- negatives encode as 5 bytes); writeBool/readBool match writeBoolean/
// readBoolean (one byte, 1 for true / 0 for false on encode).
//
//   pkt_pick_item_entity_sb_parity [--cases mcpp/build/pkt_pick_item_entity_sb.tsv]
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
    std::string casesPath = "build/pkt_pick_item_entity_sb.tsv";
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

        // ENC <id-dec> <includeData-dec(0/1)> <readableBytes-dec> <hex>
        std::string idStr, incStr, lenStr, expHex;
        if (!std::getline(ss, idStr, '\t') || !std::getline(ss, incStr, '\t')
            || !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t id = (int32_t)std::stoll(idStr);
        bool includeData = (std::stoi(incStr) != 0);
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): writeVarInt(id) then writeBool(includeData), in that order.
        PacketBuffer enc;
        enc.writeVarInt(id);
        enc.writeBool(includeData);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH id=" << id << " inc=" << includeData
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH id=" << id << " inc=" << includeData
                      << " got=" << enc.data().size() << " want=" << expLen << "\n";
        }

        // read(): readVarInt then readBool must recover the exact fields in order.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotId = dec.readVarInt();
        bool gotInc = dec.readBool();
        if (gotId != id || gotInc != includeData) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << " got=(" << gotId << ","
                      << gotInc << ") want=(" << id << "," << includeData << ")\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING id=" << id << " inc=" << includeData
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktPickItemEntitySbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
