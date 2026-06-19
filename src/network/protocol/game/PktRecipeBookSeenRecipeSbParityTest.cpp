// Parity gate for ServerboundRecipeBookSeenRecipePacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktRecipeBookSeenRecipeSbParity.java ground truth).
//
// The packet is a record(RecipeDisplayId recipe). Its STREAM_CODEC is
//   StreamCodec.composite(RecipeDisplayId.STREAM_CODEC, ::recipe, ::new)
// and RecipeDisplayId is a record(int index) with STREAM_CODEC
//   StreamCodec.composite(ByteBufCodecs.VAR_INT, RecipeDisplayId::index, RecipeDisplayId::new)
// (net.minecraft.network.protocol.game.ServerboundRecipeBookSeenRecipePacket lines 9-12,
//  net.minecraft.world.item.crafting.display.RecipeDisplayId lines 7-10).
// composite + ByteBufCodecs.VAR_INT: no packet-id prefix, just the body, so the whole wire
// payload is exactly ONE VarInt (LEB128, signed, no zig-zag): recipe.index().
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt / readVarInt are byte-for-byte / value-for-value the same as the real codec
// (no zig-zag -- negatives encode as 5 bytes). RecipeDisplayId.index has no validation, so
// any 32-bit value is legal.
//
//   pkt_recipe_book_seen_recipe_sb_parity [--cases mcpp/build/pkt_recipe_book_seen_recipe_sb.tsv]
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
    std::string casesPath = "mcpp/build/pkt_recipe_book_seen_recipe_sb.tsv";
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

        // ENC <index-dec> <readableBytes-dec> <hex>
        std::string idxStr, lenStr, expHex;
        if (!std::getline(ss, idxStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t index = (int32_t)std::stoll(idxStr);
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): a single writeVarInt(index) -- the entire wire payload.
        PacketBuffer enc;
        enc.writeVarInt(index);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH index=" << index
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH index=" << index
                      << " got=" << enc.data().size() << " want=" << expLen << "\n";
        }

        // read(): one readVarInt call must recover the exact signed int.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotIdx = dec.readVarInt();
        if (gotIdx != index) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << " got=" << gotIdx
                      << " want=" << index << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING index=" << index
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktRecipeBookSeenRecipeSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
