// Parity gate for net.minecraft.network.protocol.game.ClientboundContainerSetSlotPacket.STREAM_CODEC
// over the EMPTY + NO-COMPONENT ItemStack domain.
//
// Wire form (ClientboundContainerSetSlotPacket.java:32-37):
//   writeContainerId(containerId)  -> VarInt  (FriendlyByteBuf.writeContainerId = VarInt.write)
//   writeVarInt(stateId)           -> VarInt
//   writeShort(slot)               -> 2-byte big-endian short
//   ItemStack.OPTIONAL_STREAM_CODEC.encode(itemStack):
//       empty stack        -> VarInt(0)
//       no-component stack -> VarInt(count) VarInt(itemId) VarInt(0) VarInt(0)
//   (itemId = Item.STREAM_CODEC = holderRegistry(ITEM) = plain VarInt(getId), resolved here
//    name -> id through mc::net::NetworkRegistries (minecraft:item), NOT hard-coded. Stacks with
//    DataComponents need the per-component patch codecs (a later wave) and are NOT in this gate.)
//
//   tools/run_groundtruth.ps1 -Tool PktContainerSetSlotParity -Out mcpp/build/pkt_container_set_slot.tsv
//   pkt_container_set_slot_parity --cases mcpp/build/pkt_container_set_slot.tsv [--asset <path>]
//
// Row: ENC \t <containerId> \t <stateId> \t <slot> \t <itemName or "-"> \t <count> \t <isEmpty 0/1> \t <readableBytes> \t <wireHex>

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
    std::string casesPath, assetPath = "mcpp/src/assets/network_registries.tsv";
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
        std::string tag, containerStr, stateStr, slotStr, name, countStr, emptyStr, readableStr, wireHex;
        std::getline(ss, tag, '\t');
        if (tag != "ENC") continue;
        std::getline(ss, containerStr, '\t');
        std::getline(ss, stateStr, '\t');
        std::getline(ss, slotStr, '\t');
        std::getline(ss, name, '\t');
        std::getline(ss, countStr, '\t');
        std::getline(ss, emptyStr, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;
        int containerId = std::stoi(containerStr);
        int stateId = std::stoi(stateStr);
        int slot = std::stoi(slotStr);
        int count = std::stoi(countStr);
        bool isEmpty = emptyStr == "1";
        size_t wantReadable = (size_t)std::stoul(readableStr);

        mc::net::PacketBuffer buf;
        // Header fields in codec order.
        buf.writeVarInt(containerId);                 // writeContainerId = VarInt.write
        buf.writeVarInt(stateId);                     // writeVarInt(stateId)
        buf.writeShort((int16_t)slot);                // writeShort(slot): 2-byte big-endian short
        // ItemStack.OPTIONAL_STREAM_CODEC over the no-component domain.
        if (isEmpty) {
            buf.writeVarInt(0);                        // empty stack
        } else {
            auto id = reg.id("minecraft:item", name);
            if (!id) {
                ++mism;
                if (shown++ < 25) std::cerr << "REGISTRY-MISS minecraft:item " << name << "\n";
                continue;
            }
            buf.writeVarInt(count);                    // ItemStack count
            buf.writeVarInt(*id);                      // Item.STREAM_CODEC holderRegistry -> plain VarInt(getId)
            buf.writeVarInt(0);                        // DataComponentPatch: added count = 0
            buf.writeVarInt(0);                        // DataComponentPatch: removed count = 0
        }
        std::string got = hex(buf.data());
        if (got != wireHex || buf.data().size() != wantReadable) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "MISMATCH cid=" << containerId << " sid=" << stateId << " slot=" << slot
                          << " item=" << name << " count=" << count << " empty=" << isEmpty
                          << "\n  got  " << got << " (" << buf.data().size() << "B)\n  want "
                          << wireHex << " (" << wantReadable << "B)\n";
        }
    }
    std::cout << "PktContainerSetSlot cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
