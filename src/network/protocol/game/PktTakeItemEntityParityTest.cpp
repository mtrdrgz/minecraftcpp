// Parity gate for ClientboundTakeItemEntityPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktTakeItemEntityParity.java ground truth).
//
// The packet carries three ints (itemId, playerId, amount). Its body is exactly:
//   write : output.writeVarInt(this.itemId)
//           output.writeVarInt(this.playerId)
//           output.writeVarInt(this.amount)
//   read  : input.readVarInt(); input.readVarInt(); input.readVarInt();
// (net.minecraft.network.protocol.game.ClientboundTakeItemEntityPacket lines 22-32.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body, so the whole
// wire payload is exactly three VarInts (LEB128) of the signed ints, in that order.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt(x) / readVarInt() are byte-for-byte / value-for-value the same as the real
// codec (no zig-zag — negatives encode as 5 bytes).
//
//   pkt_take_item_entity_parity [--cases mcpp/build/pkt_take_item_entity.tsv]
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
    std::string casesPath = "build/pkt_take_item_entity.tsv";
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

        // ENC <itemId-dec> <playerId-dec> <amount-dec> <readableBytes-dec> <hex>
        std::string itemStr, playerStr, amountStr, lenStr, expHex;
        if (!std::getline(ss, itemStr, '\t') || !std::getline(ss, playerStr, '\t')
            || !std::getline(ss, amountStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t itemId   = (int32_t)std::stoll(itemStr);
        int32_t playerId = (int32_t)std::stoll(playerStr);
        int32_t amount   = (int32_t)std::stoll(amountStr);
        size_t  expLen   = (size_t)std::stoull(lenStr);

        // write(): three VarInts in order — itemId, playerId, amount.
        PacketBuffer enc;
        enc.writeVarInt(itemId);
        enc.writeVarInt(playerId);
        enc.writeVarInt(amount);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH item=" << itemId << " player=" << playerId
                      << " amount=" << amount << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH item=" << itemId << " player=" << playerId
                      << " amount=" << amount << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // read(): three readVarInt() must recover the exact signed ints, in order.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotItem   = dec.readVarInt();
        int32_t gotPlayer = dec.readVarInt();
        int32_t gotAmount = dec.readVarInt();
        if (gotItem != itemId || gotPlayer != playerId || gotAmount != amount) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex
                      << " got=(" << gotItem << "," << gotPlayer << "," << gotAmount << ")"
                      << " want=(" << itemId << "," << playerId << "," << amount << ")\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING item=" << itemId << " player=" << playerId
                      << " amount=" << amount << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktTakeItemEntityParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
