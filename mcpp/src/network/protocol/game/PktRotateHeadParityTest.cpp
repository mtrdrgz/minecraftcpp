// Parity gate for net.minecraft.network.protocol.game.ClientboundRotateHeadPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktRotateHeadParity.java ground
// truth).
//
// The packet body is exactly (ClientboundRotateHeadPacket.java:29-32):
//   write : output.writeVarInt(this.entityId);
//           output.writeByte(this.yHeadRot);   // a single signed byte (packed rotation)
//   read  : this.entityId = input.readVarInt();
//           this.yHeadRot = input.readByte();  // SIGNED byte (-128..127)
// Packet.codec -> StreamCodec.ofMember (Packet.java:22-24): body only, no id/length prefix.
//
// Both fields are plain numbers; no registry/ItemStack/Component/Holder/NBT. This reuses
// the certified PacketBuffer (the FriendlyByteBuf port) directly:
//   writeVarInt(entityId) + writeByte(yHeadRot & 0xff)
// is byte-for-byte the same as the real codec, and readVarInt()/readByte() round-trips
// the fields value-for-value (the byte is read back signed).
//
//   pkt_rotate_head_parity [--cases mcpp/build/pkt_rotate_head.tsv]
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
    std::string casesPath = "mcpp/build/pkt_rotate_head.tsv";
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

        // ENC <name> <entityId> <yHeadRot> <readableBytes> <hexBytes>
        std::string name, idS, rotS, rbS, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, idS, '\t')
            || !std::getline(ss, rotS, '\t') || !std::getline(ss, rbS, '\t')
            || !std::getline(ss, expHex))
            continue;
        ++cases;

        int32_t entityId = (int32_t)std::stoll(idS);
        int32_t yHeadRot = (int32_t)std::stoll(rotS);   // -128..127 (a Java byte)
        size_t expReadable = (size_t)std::stoull(rbS);

        // write(): writeVarInt(entityId) + writeByte(yHeadRot & 0xff).
        // FriendlyByteBuf.writeByte(int) writes the low 8 bits; yHeadRot is already a
        // byte so its low 8 bits == (yHeadRot & 0xff).
        PacketBuffer enc;
        enc.writeVarInt(entityId);
        enc.writeByte((uint8_t)(yHeadRot & 0xff));
        std::string got = hex(enc.data());

        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expReadable) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH " << name << " got=" << enc.data().size()
                      << " want=" << expReadable << "\n";
        }

        // read(): decode the expected bytes back and require the fields round-trip.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotId = dec.readVarInt();
        // readByte() returns the raw byte; read() yields a SIGNED Java byte, so
        // sign-extend the low 8 bits to compare against the signed yHeadRot.
        int32_t gotRot = (int32_t)(int8_t)dec.readByte();

        if (gotId != entityId) {
            ++mismatches;
            std::cerr << "DEC-ID " << name << " got=" << gotId << " want=" << entityId << "\n";
        }
        // The wire carries the low 8 bits; the signed value that round-trips is the
        // sign-extension of those 8 bits, which for a Java byte field equals yHeadRot.
        int32_t expRot = (int32_t)(int8_t)(yHeadRot & 0xff);
        if (gotRot != expRot) {
            ++mismatches;
            std::cerr << "DEC-ROT " << name << " got=" << gotRot << " want=" << expRot << "\n";
        }
    }

    std::cout << "PktRotateHeadParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
