// Byte-exact parity gate for ClientboundMountScreenOpenPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktMountScreenOpenParity.java ground truth).
//
// Source (26.1.2/src/.../ClientboundMountScreenOpenPacket.java):
//   write(buf): buf.writeContainerId(containerId);   // == VarInt.write (FriendlyByteBuf:679-685)
//               buf.writeVarInt(inventoryColumns);    // LEB128, no zig-zag
//               buf.writeInt(entityId);               // 4B big-endian (NOT VarInt)
//   read(buf) : readContainerId() -> containerId; readVarInt() -> columns; readInt() -> entityId;
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body. The whole wire
// payload is VARINT(containerId) ++ VARINT(inventoryColumns) ++ INT(entityId, 4B BE).
//
// writeContainerId is plain VarInt.write, and entityId uses writeInt (fixed 4-byte big-endian),
// NOT writeVarInt. We follow the real source verbatim. This reuses the certified PacketBuffer
// (the FriendlyByteBuf port) directly: writeVarInt / writeInt and readVarInt / readInt are
// byte-for-byte / value-for-value identical to the real codec.
//
//   pkt_mount_screen_open_parity [--cases mcpp/build/pkt_mount_screen_open.tsv]
//
// Row: ENC <containerId-dec> <inventoryColumns-dec> <entityId-dec> <readableBytes-dec> <hex>
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
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    out.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_mount_screen_open.tsv";
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

        // ENC <containerId-dec> <inventoryColumns-dec> <entityId-dec> <readableBytes-dec> <hex>
        std::string cidStr, colStr, eidStr, lenStr, expHex;
        if (!std::getline(ss, cidStr, '\t') || !std::getline(ss, colStr, '\t')
            || !std::getline(ss, eidStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t containerId = (int32_t)std::stoll(cidStr);
        int32_t columns     = (int32_t)std::stoll(colStr);
        int32_t entityId    = (int32_t)std::stoll(eidStr);
        size_t  expLen      = (size_t)std::stoull(lenStr);

        // (1) ENCODE: write the fields in the REAL wire order. writeContainerId == writeVarInt;
        // entityId via writeInt (fixed 4-byte BE).
        PacketBuffer enc;
        enc.writeVarInt(containerId);
        enc.writeVarInt(columns);
        enc.writeInt(entityId);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH cid=" << containerId << " col=" << columns
                      << " eid=" << entityId << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH cid=" << containerId << " col=" << columns
                      << " eid=" << entityId << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // (2) DECODE: read the Java bytes back and verify fields exact + ordered.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotCid = dec.readVarInt();
        int32_t gotCol = dec.readVarInt();
        int32_t gotEid = dec.readInt();

        if (gotCid != containerId) {
            ++mismatches;
            std::cerr << "DEC-CONTAINERID-MISMATCH hex=" << expHex << " got=" << gotCid
                      << " want=" << containerId << "\n";
        }
        if (gotCol != columns) {
            ++mismatches;
            std::cerr << "DEC-COLUMNS-MISMATCH hex=" << expHex << " got=" << gotCol
                      << " want=" << columns << "\n";
        }
        if (gotEid != entityId) {
            ++mismatches;
            std::cerr << "DEC-ENTITYID-MISMATCH hex=" << expHex << " got=" << gotEid
                      << " want=" << entityId << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING cid=" << containerId << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktMountScreenOpenParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
