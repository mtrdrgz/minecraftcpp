// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundPlaceRecipePacket
// against the REAL STREAM_CODEC (tools/PktPlaceRecipeSbParity.java ground truth).
//
// The packet is a record encoded by StreamCodec.composite of three primitive codecs,
// in field order (ServerboundPlaceRecipePacket.java:11-19):
//   ByteBufCodecs.CONTAINER_ID   -> containerId  (FriendlyByteBuf.writeContainerId
//                                                  == VarInt.write, a LEB128 VarInt)
//   RecipeDisplayId.STREAM_CODEC -> recipe.index (composite over ByteBufCodecs.VAR_INT)
//   ByteBufCodecs.BOOL           -> useMaxItems  (single byte 0/1)
// So the wire body is exactly:
//   writeVarInt(containerId) + writeVarInt(recipeIndex) + writeBoolean(useMaxItems)
// composite codecs carry no packet-id or length prefix.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
//   writeVarInt(containerId) + writeVarInt(recipeIndex) + writeBool(useMaxItems)
// is byte-for-byte the same as the real codec, and readVarInt()/readVarInt()/readBool()
// round-trips the fields value-for-value.
//
//   pkt_place_recipe_sb_parity [--cases mcpp/build/pkt_place_recipe_sb.tsv]
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
    std::string casesPath = "mcpp/build/pkt_place_recipe_sb.tsv";
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

        // ENC <name> <containerId> <recipeIndex> <useMaxItems> <readableBytes> <hexBytes>
        std::string name, cidS, ridS, mS, rbS, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, cidS, '\t')
            || !std::getline(ss, ridS, '\t') || !std::getline(ss, mS, '\t')
            || !std::getline(ss, rbS, '\t') || !std::getline(ss, expHex))
            continue;
        ++cases;

        int32_t containerId = (int32_t)std::stoll(cidS);
        int32_t recipeIndex = (int32_t)std::stoll(ridS);
        bool useMaxItems = std::stoi(mS) != 0;
        size_t expReadable = (size_t)std::stoull(rbS);

        bool ok = true;

        // encode(): writeVarInt(containerId) + writeVarInt(recipeIndex) + writeBool(useMaxItems).
        PacketBuffer enc;
        try {
            enc.writeVarInt(containerId);
            enc.writeVarInt(recipeIndex);
            enc.writeBool(useMaxItems);
        } catch (const std::exception& e) {
            std::cerr << "ENC-EXCEPTION " << name << ": " << e.what() << "\n";
            ok = false;
        }

        if (ok) {
            std::string got = hex(enc.data());
            if (got != expHex) {
                std::cerr << "ENC-MISMATCH " << name << "\n  got  " << got
                          << "\n  want " << expHex << "\n";
                ok = false;
            }
            if (enc.data().size() != expReadable) {
                std::cerr << "LEN-MISMATCH " << name << " got=" << enc.data().size()
                          << " want=" << expReadable << "\n";
                ok = false;
            }
        }

        // decode(): decode the expected bytes back and require the fields round-trip.
        try {
            std::vector<uint8_t> bytes = unhex(expHex);
            PacketBuffer dec(bytes);
            int32_t gotCid = dec.readVarInt();
            int32_t gotRid = dec.readVarInt();
            bool gotMax = dec.readBool();

            if (gotCid != containerId) {
                std::cerr << "DEC-CONTAINER " << name << " got=" << gotCid
                          << " want=" << containerId << "\n";
                ok = false;
            }
            if (gotRid != recipeIndex) {
                std::cerr << "DEC-RECIPE " << name << " got=" << gotRid
                          << " want=" << recipeIndex << "\n";
                ok = false;
            }
            if (gotMax != useMaxItems) {
                std::cerr << "DEC-MAXITEMS " << name << " got=" << gotMax
                          << " want=" << useMaxItems << "\n";
                ok = false;
            }
            if (dec.remaining() != 0) {
                std::cerr << "DEC-TRAILING " << name << " remaining "
                          << dec.remaining() << "\n";
                ok = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "DEC-EXCEPTION " << name << ": " << e.what() << "\n";
            ok = false;
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktPlaceRecipeSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
