// Byte-exact parity for
// net.minecraft.network.protocol.game.ServerboundRecipeBookChangeSettingsPacket
// against the REAL STREAM_CODEC (tools/PktRecipeBookSettingsSbParity.java GT).
//
// net.minecraft.network.protocol.game.ServerboundRecipeBookChangeSettingsPacket
// (26.1.2):
//   private final RecipeBookType bookType;
//   private final boolean isOpen;
//   private final boolean isFiltering;
//   STREAM_CODEC = Packet.codec(::write, ::new)
// write(FriendlyByteBuf), verbatim field order (ServerboundRecipeBookChangeSettingsPacket.java:29-33):
//     output.writeEnum(this.bookType);       // VarInt(ordinal)
//     output.writeBoolean(this.isOpen);      // 1 byte 0/1
//     output.writeBoolean(this.isFiltering); // 1 byte 0/1
// Read side (ServerboundRecipeBookChangeSettingsPacket.java:23-27):
//     readEnum(RecipeBookType.class) == getEnumConstants()[readVarInt()]
//     readBoolean(); readBoolean();
//
// FriendlyByteBuf.writeEnum(value) == writeVarInt(value.ordinal()) (FriendlyByteBuf.java:471-473);
// FriendlyByteBuf.readEnum(clazz)  == getEnumConstants()[readVarInt()] (FriendlyByteBuf.java:467-469).
// RecipeBookType ordinals (RecipeBookType.java:3-7):
//   CRAFTING=0, FURNACE=1, BLAST_FURNACE=2, SMOKER=3.
// Packet.codec -> no packet-id prefix, just the body.
//
// We reuse the certified mc::net::PacketBuffer (the FriendlyByteBuf port):
// writeVarInt(ordinal)==writeEnum, writeBool==writeBoolean. For each ENC case we
// (a) ENCODE the fields in codec order and require bytes match Java's hex exactly
// + readableBytes match, then (b) DECODE the Java bytes back through PacketBuffer
// and require every field round-trips identically.
//
//   pkt_recipe_book_settings_sb_parity [--cases mcpp/build/pkt_recipe_book_settings_sb.tsv]
#include "../../PacketBuffer.h"

#include <array>
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

// RecipeBookType constants in declaration order; index == ordinal
// (RecipeBookType.java:3-7).
const std::array<const char*, 4> kTypeNames = {
    "CRAFTING",       // ordinal 0
    "FURNACE",        // ordinal 1
    "BLAST_FURNACE",  // ordinal 2
    "SMOKER",         // ordinal 3
};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_recipe_book_settings_sb.tsv";
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

        if (tag == "ENUM") {
            // ENUM <ordinal> <name>
            std::string ordStr, name;
            if (!std::getline(ss, ordStr, '\t') || !std::getline(ss, name)) continue;
            ++cases;
            int ord = std::stoi(ordStr);
            if (ord < 0 || ord >= (int)kTypeNames.size() ||
                name != kTypeNames[(size_t)ord]) {
                ++mismatches;
                std::cerr << "ENUM-MISMATCH ord=" << ord << " got="
                          << (ord >= 0 && ord < (int)kTypeNames.size()
                                  ? kTypeNames[(size_t)ord] : "<oob>")
                          << " want=" << name << "\n";
            }
        } else if (tag == "ENC") {
            // ENC <name> <ord> <isOpen> <isFiltering> <readableBytes> <hex>
            std::string name, ordStr, openStr, filterStr, rbStr, expHex;
            if (!std::getline(ss, name, '\t') || !std::getline(ss, ordStr, '\t')
                || !std::getline(ss, openStr, '\t')
                || !std::getline(ss, filterStr, '\t')
                || !std::getline(ss, rbStr, '\t') || !std::getline(ss, expHex))
                continue;
            ++cases;
            int32_t ord  = (int32_t)std::stoll(ordStr);
            bool isOpen  = std::stoi(openStr) != 0;
            bool isFilt  = std::stoi(filterStr) != 0;
            int expReadable = std::stoi(rbStr);

            // write(): writeEnum(ordinal)=writeVarInt(ord), then two writeBoolean.
            PacketBuffer enc;
            enc.writeVarInt(ord);
            enc.writeBool(isOpen);
            enc.writeBool(isFilt);
            std::string got = hex(enc.data());
            if (got != expHex) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH name=" << name << " ord=" << ord
                          << " isOpen=" << isOpen << " isFiltering=" << isFilt
                          << "\n  got  " << got << "\n  want " << expHex << "\n";
            }
            if ((int)enc.data().size() != expReadable) {
                ++mismatches;
                std::cerr << "ENC-READABLE-MISMATCH name=" << name << " ord=" << ord
                          << " got=" << enc.data().size()
                          << " want=" << expReadable << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hex> <ordIn> <ordOut> <isOpen> <isFiltering>
            std::string inHex, ordInStr, ordOutStr, openStr, filterStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, ordInStr, '\t')
                || !std::getline(ss, ordOutStr, '\t')
                || !std::getline(ss, openStr, '\t')
                || !std::getline(ss, filterStr)) continue;
            ++cases;
            int32_t expOrd = (int32_t)std::stoll(ordOutStr);
            bool expOpen   = std::stoi(openStr) != 0;
            bool expFilt   = std::stoi(filterStr) != 0;

            // read(): readEnum -> getEnumConstants()[readVarInt()]; for in-range
            // ordinals that index is the identity, so decoded ordinal == read
            // VarInt. Then two readBoolean.
            PacketBuffer dec(unhex(inHex));
            int32_t gotOrd  = dec.readVarInt();
            bool gotOpen    = dec.readBool();
            bool gotFilt    = dec.readBool();
            if (gotOrd != expOrd) {
                ++mismatches;
                std::cerr << "DEC-ORD-MISMATCH hex=" << inHex << " got=" << gotOrd
                          << " want=" << expOrd << "\n";
            }
            if (gotOpen != expOpen) {
                ++mismatches;
                std::cerr << "DEC-OPEN-MISMATCH hex=" << inHex << " got=" << gotOpen
                          << " want=" << expOpen << "\n";
            }
            if (gotFilt != expFilt) {
                ++mismatches;
                std::cerr << "DEC-FILTER-MISMATCH hex=" << inHex << " got=" << gotFilt
                          << " want=" << expFilt << "\n";
            }
            if (dec.remaining() != 0) {
                ++mismatches;
                std::cerr << "DEC-TRAILING hex=" << inHex << " remaining="
                          << dec.remaining() << "\n";
            }
        }
    }

    std::cout << "PktRecipeBookSettingsSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
