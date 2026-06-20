// Parity gate for net.minecraft.network.protocol.game.ServerboundSetCreativeModeSlotPacket
// vs its REAL STREAM_CODEC (ServerboundSetCreativeModeSlotPacket.java:11-17), restricted to
// EMPTY + NO-COMPONENT ItemStacks. Ground truth: tools/PktSetCreativeSlotParity.java.
//
// STREAM_CODEC = StreamCodec.composite(
//     ByteBufCodecs.SHORT, slotNum,
//     ItemStack.validatedStreamCodec(ItemStack.OPTIONAL_UNTRUSTED_STREAM_CODEC), itemStack, ::new)
//
// Wire form (byte-for-byte), in codec order:
//   1) ByteBufCodecs.SHORT.encode (ByteBufCodecs.java:80-82) = writeShort(slotNum) -> 2 bytes BE.
//   2) ItemStack.validatedStreamCodec.encode (ItemStack.java:209-211) delegates to the inner
//      codec (validation runs only on DECODE) -> NO extra bytes. The inner codec is
//      OPTIONAL_UNTRUSTED_STREAM_CODEC = createOptionalStreamCodec(DataComponentPatch.DELIMITED_STREAM_CODEC).
//      Its encode (ItemStack.java:185-193):
//        empty stack       -> VarInt(0)
//        no-component stack -> VarInt(count) VarInt(itemId) VarInt(0) VarInt(0)
//      itemId = Item.STREAM_CODEC = holderRegistry(ITEM) = plain VarInt(BuiltInRegistries.ITEM.getId),
//      resolved here name -> id through mc::net::NetworkRegistries (minecraft:item, gated by
//      network_registries_parity), NOT hard-coded. DELIMITED == STREAM_CODEC for an empty patch
//      (DataComponentPatch.java:106-109: writeVarInt(0) writeVarInt(0)); the delimited length
//      prefix only wraps PRESENT components, so over the no-component domain the bytes are identical.
//
// Stacks carrying DataComponents need the per-component patch codecs (a later wave) and are NOT
// in this gate.
//
//   pkt_set_creative_slot_parity --cases mcpp/build/pkt_set_creative_slot.tsv [--asset <path>]
//
// Row: ENC \t <slotNum> \t <itemName or "-"> \t <count> \t <isEmpty 0/1> \t <readableBytes> \t <wireHex>

#include "../../PacketBuffer.h"
#include "../../NetworkRegistries.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s; s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath, assetPath = "src/assets/network_registries.tsv";
    for (int i = 1; i + 1 < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases") casesPath = argv[++i];
        else if (a == "--asset") assetPath = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: --cases <tsv> [--asset <path>]\n"; return 2; }

    mc::net::NetworkRegistries reg;
    if (!reg.loadFromFile(assetPath)) { std::cerr << "cannot open asset " << assetPath << "\n"; return 2; }

    std::ifstream f(casesPath);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long cases = 0, mism = 0; int shown = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::stringstream ss(line);
        std::string tag, slotStr, name, countStr, emptyStr, readableStr, wireHex;
        std::getline(ss, tag, '\t');
        if (tag != "ENC") continue;
        std::getline(ss, slotStr, '\t');
        std::getline(ss, name, '\t');
        std::getline(ss, countStr, '\t');
        std::getline(ss, emptyStr, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;
        int32_t slot = (int32_t)std::stoi(slotStr);
        int count = std::stoi(countStr);
        bool isEmpty = emptyStr == "1";
        size_t wantReadable = (size_t)std::stoul(readableStr);

        mc::net::PacketBuffer buf;
        // Field 1: ByteBufCodecs.SHORT -> writeShort(slotNum) (low 16 bits, big-endian).
        buf.writeShort((int16_t)slot);
        // Field 2: ItemStack (OPTIONAL_UNTRUSTED, no-component domain).
        if (isEmpty) {
            buf.writeVarInt(0);  // empty stack
        } else {
            auto id = reg.id("minecraft:item", name);
            if (!id) {
                ++mism;
                if (shown++ < 25) std::cerr << "REGISTRY-MISS minecraft:item " << name << "\n";
                continue;
            }
            buf.writeVarInt(count);   // ItemStack count
            buf.writeVarInt(*id);     // Item.STREAM_CODEC holderRegistry -> plain VarInt(getId)
            buf.writeVarInt(0);       // DataComponentPatch: added count = 0
            buf.writeVarInt(0);       // DataComponentPatch: removed count = 0
        }
        std::string got = hex(buf.data());
        if (got != wireHex || buf.data().size() != wantReadable) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "MISMATCH slot=" << slot << " " << name << " count=" << count
                          << " empty=" << isEmpty
                          << "\n  got  " << got << " (" << buf.data().size() << "B)\n  want "
                          << wireHex << " (" << wantReadable << "B)\n";
        }
    }
    std::cout << "PktSetCreativeSlotParity cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
