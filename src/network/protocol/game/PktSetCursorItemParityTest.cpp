// Parity gate for ClientboundSetCursorItemPacket.STREAM_CODEC vs the REAL net.minecraft
// codec (tools/PktSetCursorItemParity.java ground truth).
//
// ClientboundSetCursorItemPacket (ClientboundSetCursorItemPacket.java:9-12):
//   record(ItemStack contents); STREAM_CODEC = StreamCodec.composite(
//     ItemStack.OPTIONAL_STREAM_CODEC, ::contents, ::new)
// One field — the cursor stack — encoded via ItemStack.OPTIONAL_STREAM_CODEC. Wire form over
// the NO-COMPONENT domain (ItemStack.java:185-193, DataComponentPatch.java:106-109):
//   empty stack         -> VarInt(0)
//   no-component stack   -> VarInt(count) VarInt(itemId) VarInt(0) VarInt(0)
// itemId = Item.STREAM_CODEC = holderRegistry(ITEM) = plain VarInt(BuiltInRegistries.ITEM.getId),
// resolved here name -> id through mc::net::NetworkRegistries (minecraft:item, gated by
// network_registries_parity), NOT hard-coded. Stacks carrying DataComponents need the per-
// component patch codecs (a later wave) and are NOT in this gate.
//
//   tools/run_groundtruth.ps1 -Tool PktSetCursorItemParity -Out mcpp/build/pkt_set_cursor_item.tsv
//   pkt_set_cursor_item_parity --cases mcpp/build/pkt_set_cursor_item.tsv [--asset <path>]
//
// Row: ENC \t <itemName or "-"> \t <count> \t <isEmpty 0/1> \t <readableBytes> \t <wireHex>

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
        std::string tag, name, countStr, emptyStr, readableStr, wireHex;
        std::getline(ss, tag, '\t');
        if (tag != "ENC") continue;
        std::getline(ss, name, '\t');
        std::getline(ss, countStr, '\t');
        std::getline(ss, emptyStr, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;
        int count = std::stoi(countStr);
        bool isEmpty = emptyStr == "1";
        size_t wantReadable = (size_t)std::stoul(readableStr);

        // Encode the single `contents` field exactly as ItemStack.OPTIONAL_STREAM_CODEC does.
        mc::net::PacketBuffer buf;
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
                std::cerr << "MISMATCH " << name << " count=" << count << " empty=" << isEmpty
                          << "\n  got  " << got << " (" << buf.data().size() << "B)\n  want "
                          << wireHex << " (" << wantReadable << "B)\n";
        }
    }
    std::cout << "PktSetCursorItem cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
