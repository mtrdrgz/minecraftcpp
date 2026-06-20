// Parity gate for net.minecraft.network.protocol.game.ClientboundMoveEntityPacket.Rot's
// StreamCodec vs the REAL net.minecraft codec (tools/PktMoveEntityRotParity.java ground
// truth).
//
// ClientboundMoveEntityPacket is abstract; we target the Rot subtype. Its body is exactly
// (ClientboundMoveEntityPacket.java:178-183, the .Rot static class):
//   write : output.writeVarInt(this.entityId);
//           output.writeByte(this.yRot);        // signed byte (packed yaw)
//           output.writeByte(this.xRot);        // signed byte (packed pitch)
//           output.writeBoolean(this.onGround); // single byte 0/1
//   read  : entityId = readVarInt(); yRot = readByte(); xRot = readByte();
//           onGround = readBoolean();           // both bytes are SIGNED (-128..127)
// Packet.codec -> StreamCodec.ofMember (Packet.java): body only, no id/length prefix.
//
// All four fields are plain numbers/bool; no registry/ItemStack/Component/Holder/NBT.
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
//   writeVarInt(entityId)+writeByte(yRot&0xff)+writeByte(xRot&0xff)+writeBool(onGround)
// is byte-for-byte the same as the real codec, and readVarInt()/readByte()/readBool()
// round-trips the fields value-for-value (the bytes are read back signed).
//
//   pkt_move_entity_rot_parity [--cases mcpp/build/pkt_move_entity_rot.tsv]
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
    std::string casesPath = "build/pkt_move_entity_rot.tsv";
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

        // ENC <name> <entityId> <yRot> <xRot> <onGround> <readableBytes> <hexBytes>
        std::string name, idS, yS, xS, gS, rbS, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, idS, '\t')
            || !std::getline(ss, yS, '\t') || !std::getline(ss, xS, '\t')
            || !std::getline(ss, gS, '\t') || !std::getline(ss, rbS, '\t')
            || !std::getline(ss, expHex))
            continue;
        ++cases;

        int32_t entityId = (int32_t)std::stoll(idS);
        int32_t yRot = (int32_t)std::stoll(yS);   // -128..127 (a Java byte)
        int32_t xRot = (int32_t)std::stoll(xS);   // -128..127 (a Java byte)
        bool onGround = std::stoi(gS) != 0;
        size_t expReadable = (size_t)std::stoull(rbS);

        // write(): writeVarInt + writeByte(yRot) + writeByte(xRot) + writeBool(onGround).
        // FriendlyByteBuf.writeByte(int) writes the low 8 bits; yRot/xRot are already
        // bytes so their low 8 bits == (val & 0xff).
        PacketBuffer enc;
        enc.writeVarInt(entityId);
        enc.writeByte((uint8_t)(yRot & 0xff));
        enc.writeByte((uint8_t)(xRot & 0xff));
        enc.writeBool(onGround);
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
        // sign-extend the low 8 bits to compare against the signed yRot/xRot.
        int32_t gotY = (int32_t)(int8_t)dec.readByte();
        int32_t gotX = (int32_t)(int8_t)dec.readByte();
        bool gotG = dec.readBool();

        if (gotId != entityId) {
            ++mismatches;
            std::cerr << "DEC-ID " << name << " got=" << gotId << " want=" << entityId << "\n";
        }
        // The wire carries the low 8 bits; the signed value that round-trips is the
        // sign-extension of those 8 bits, which for a Java byte field equals the field.
        int32_t expY = (int32_t)(int8_t)(yRot & 0xff);
        int32_t expX = (int32_t)(int8_t)(xRot & 0xff);
        if (gotY != expY) {
            ++mismatches;
            std::cerr << "DEC-YROT " << name << " got=" << gotY << " want=" << expY << "\n";
        }
        if (gotX != expX) {
            ++mismatches;
            std::cerr << "DEC-XROT " << name << " got=" << gotX << " want=" << expX << "\n";
        }
        if (gotG != onGround) {
            ++mismatches;
            std::cerr << "DEC-GROUND " << name << " got=" << gotG << " want=" << onGround << "\n";
        }
    }

    std::cout << "PktMoveEntityRotParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
