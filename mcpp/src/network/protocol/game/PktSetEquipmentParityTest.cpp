// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundSetEquipmentPacket
// vs the REAL ClientboundSetEquipmentPacket.STREAM_CODEC (tools/PktSetEquipmentParity.java).
//
// 26.1.2 wire body (ClientboundSetEquipmentPacket.write, lines 39-51 — hand-written, NOT a
// composite codec):
//   writeVarInt(entity)
//   for i in [0, size):
//     slotByte = slot.ordinal() | (i != size-1 ? 0x80 : 0)   // CONTINUE_MASK = -128 = 0x80
//     writeByte(slotByte)
//     ItemStack.OPTIONAL_STREAM_CODEC.encode(stack)
//   So the LAST slot's byte has the high bit CLEAR (terminates the do-while decode loop);
//   every earlier slot has it SET. The slot index on the wire is EquipmentSlot.ordinal()
//   (MAINHAND=0 OFFHAND=1 FEET=2 LEGS=3 CHEST=4 HEAD=5 BODY=6 SADDLE=7), NOT EquipmentSlot.id.
//
// ItemStack.OPTIONAL_STREAM_CODEC, NO-COMPONENT domain (ItemStack.java:170-194,
// DataComponentPatch.java:106-109), already certified by itemstack_stream_parity:
//   empty stack        -> writeVarInt(0)
//   no-component stack  -> writeVarInt(count) writeVarInt(itemId) writeVarInt(0) writeVarInt(0)
//   itemId = Item.STREAM_CODEC = holderRegistry(ITEM) -> plain VarInt(getId), resolved here
//   name -> id through mc::net::NetworkRegistries (minecraft:item), NOT hard-coded. Stacks
//   carrying DataComponents are out of scope (a later wave).
//
//   pkt_set_equipment_parity [--cases mcpp/build/pkt_set_equipment.tsv] [--asset <path>]
//
// Row: ENC <entityId> <slotCount> [ <slotOrdinal> <itemName|"-"> <count> <isEmpty> ]* <readableBytes> <hexBytes>
#include "../../PacketBuffer.h"
#include "../../NetworkRegistries.h"

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
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_set_equipment.tsv";
    std::string assetPath = "mcpp/src/assets/network_registries.tsv";
    for (int i = 1; i + 1 < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases") casesPath = argv[++i];
        else if (a == "--asset") assetPath = argv[++i];
    }

    mc::net::NetworkRegistries reg;
    if (!reg.loadFromFile(assetPath)) { std::cerr << "cannot open asset " << assetPath << "\n"; return 2; }

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long cases = 0, mism = 0; int shown = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // Split the whole row on tabs into fields.
        std::vector<std::string> fld;
        {
            std::stringstream ss(line);
            std::string cell;
            while (std::getline(ss, cell, '\t')) fld.push_back(cell);
        }
        if (fld.empty() || fld[0] != "ENC") continue;
        // Minimum row: ENC entity slotCount [4 per slot]* readable hex.
        if (fld.size() < 5) continue;
        ++cases;

        int32_t entityId = (int32_t)std::stoll(fld[1]);
        int     slotCount = std::stoi(fld[2]);
        // After ENC(0) entity(1) slotCount(2): slotCount*4 slot fields, then readable + hex.
        size_t expectFields = 3 + (size_t)slotCount * 4 + 2;
        if (fld.size() != expectFields) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "ROW-SHAPE entity=" << entityId << " slotCount=" << slotCount
                          << " fields=" << fld.size() << " want=" << expectFields << "\n";
            continue;
        }
        size_t readable = (size_t)std::stoul(fld[fld.size() - 2]);
        const std::string& wireHex = fld[fld.size() - 1];

        PacketBuffer buf;
        buf.writeVarInt(entityId);

        bool registryMiss = false;
        for (int s = 0; s < slotCount; ++s) {
            size_t base = 3 + (size_t)s * 4;
            int  ordinal = std::stoi(fld[base + 0]);
            const std::string& name = fld[base + 1];
            int  count = std::stoi(fld[base + 2]);
            bool isEmpty = fld[base + 3] == "1";

            // slot byte = ordinal | (continue ? 0x80 : 0); last slot -> high bit clear.
            bool cont = (s != slotCount - 1);
            buf.writeByte((uint8_t)((ordinal & 0x7f) | (cont ? 0x80 : 0x00)));

            // ItemStack.OPTIONAL_STREAM_CODEC, no-component framing.
            if (isEmpty) {
                buf.writeVarInt(0);
            } else {
                auto id = reg.id("minecraft:item", name);
                if (!id) {
                    registryMiss = true;
                    if (shown++ < 25) std::cerr << "REGISTRY-MISS minecraft:item " << name << "\n";
                    break;
                }
                buf.writeVarInt(count);  // ItemStack count
                buf.writeVarInt(*id);    // Item.STREAM_CODEC -> plain VarInt(getId)
                buf.writeVarInt(0);      // DataComponentPatch added count
                buf.writeVarInt(0);      // DataComponentPatch removed count
            }
        }
        if (registryMiss) { ++mism; continue; }

        std::string got = hex(buf.data());
        if (got != wireHex || buf.data().size() != readable) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "MISMATCH entity=" << entityId << " slotCount=" << slotCount
                          << "\n  got  " << got << " (" << buf.data().size() << "B)\n  want "
                          << wireHex << " (" << readable << "B)\n";
        }
    }

    std::cout << "PktSetEquipmentParity cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
