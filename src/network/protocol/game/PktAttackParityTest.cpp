// Parity gate for ServerboundAttackPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktAttackParity.java ground truth).
//
// The packet is `public record ServerboundAttackPacket(int entityId)` (the attacked
// entity's network id). Its STREAM_CODEC is
//   StreamCodec.composite(ByteBufCodecs.VAR_INT, ServerboundAttackPacket::entityId,
//                         ServerboundAttackPacket::new)
// (net.minecraft.network.protocol.game.ServerboundAttackPacket lines 9-22).
// ByteBufCodecs.VAR_INT is exactly VarInt.write / VarInt.read (LEB128, signed, NO zig-zag);
// a composite over one field writes only that field, and there is no packet-id prefix, so
// the entire wire payload is a single VarInt: entityId.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt / readVarInt are byte-for-byte / value-for-value the same as the real codec
// (no zig-zag -- negatives encode as 5 bytes).
//
//   pkt_attack_parity [--cases mcpp/build/pkt_attack.tsv]
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
    std::string casesPath = "build/pkt_attack.tsv";
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

        // ENC <entityId-dec> <readableBytes-dec> <hex>
        std::string idStr, lenStr, expHex;
        if (!std::getline(ss, idStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t entityId = (int32_t)std::stoll(idStr);
        size_t expLen = (size_t)std::stoull(lenStr);

        // encode(): VAR_INT writes a single VarInt of entityId.
        PacketBuffer enc;
        enc.writeVarInt(entityId);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH entityId=" << entityId
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH entityId=" << entityId
                      << " got=" << enc.data().size() << " want=" << expLen << "\n";
        }

        // decode(): one readVarInt call must recover the exact signed int.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotId = dec.readVarInt();
        if (gotId != entityId) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << " got=" << gotId
                      << " want=" << entityId << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING entityId=" << entityId
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktAttackParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
