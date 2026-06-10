// Parity gate for net.minecraft.network.protocol.game.ClientboundOpenSignEditorPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktOpenSignEditorParity.java
// ground truth).
//
// The packet body is exactly (ClientboundOpenSignEditorPacket.java:26-29):
//   write : output.writeBlockPos(this.pos);        == output.writeLong(pos.asLong())  (FriendlyByteBuf.java:398-400)
//           output.writeBoolean(this.isFrontText); // single byte 0/1
//   read  : this.pos         = input.readBlockPos();   == BlockPos.of(input.readLong())  (FriendlyByteBuf.java:389-391)
//           this.isFrontText = input.readBoolean();
// Packet.codec -> StreamCodec.ofMember (Packet.java): body only, no id/length prefix.
//
// BlockPos.asLong (BlockPos.java), with PACKED_HORIZONTAL_LENGTH = 26,
// PACKED_Y_LENGTH = 12, X_OFFSET = 38, Z_OFFSET = 12:
//   node = ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF).
// BlockPos.of/getX/getY/getZ sign-extend each field on the way back.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
//   writeLong(asLong(x,y,z)) + writeBool(isFrontText)
// is byte-for-byte the same as the real codec, and readLong()/readBool()
// round-trips the fields value-for-value.
//
//   pkt_open_sign_editor_parity [--cases mcpp/build/pkt_open_sign_editor.tsv]
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

// BlockPos.asLong(x,y,z) — BlockPos.java. 26/12/26 packing.
// X_OFFSET = 38, Z_OFFSET = 12, Y_OFFSET = 0. Masks: X/Z = 0x3FFFFFF (26 bits),
// Y = 0xFFF (12 bits).
int64_t blockPosAsLong(int32_t x, int32_t y, int32_t z) {
    uint64_t node = 0;
    node |= ((uint64_t)((uint32_t)x & 0x3FFFFFFu)) << 38;
    node |= ((uint64_t)((uint32_t)y & 0xFFFu)) << 0;
    node |= ((uint64_t)((uint32_t)z & 0x3FFFFFFu)) << 12;
    return (int64_t)node;
}

// BlockPos.getX/getY/getZ (BlockPos.java) — sign-extend each packed field.
//   getX = (int)(node >> X_OFFSET)               with 26-bit sign extension
//   getY = (int)(node << 52 >> 52)               (64 - PACKED_Y_LENGTH = 52)
//   getZ = (int)(node << 26 >> 38)               (Z field, 26-bit sign extension)
int32_t blockPosGetX(int64_t node) {
    return (int32_t)(node >> 38);  // top 26 bits, arithmetic shift sign-extends
}
int32_t blockPosGetY(int64_t node) {
    return (int32_t)((node << 52) >> 52);
}
int32_t blockPosGetZ(int64_t node) {
    return (int32_t)((node << 26) >> 38);
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_open_sign_editor.tsv";
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

        // ENC <name> <x> <y> <z> <isFrontText> <readableBytes> <hexBytes>
        std::string name, xS, yS, zS, fS, rbS, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, xS, '\t')
            || !std::getline(ss, yS, '\t') || !std::getline(ss, zS, '\t')
            || !std::getline(ss, fS, '\t') || !std::getline(ss, rbS, '\t')
            || !std::getline(ss, expHex))
            continue;
        ++cases;

        int32_t x = (int32_t)std::stoll(xS);
        int32_t y = (int32_t)std::stoll(yS);
        int32_t z = (int32_t)std::stoll(zS);
        bool isFrontText = std::stoi(fS) != 0;
        size_t expReadable = (size_t)std::stoull(rbS);

        // write(): writeBlockPos(pos)=writeLong(asLong) + writeBoolean(isFrontText).
        PacketBuffer enc;
        enc.writeLong(blockPosAsLong(x, y, z));
        enc.writeBool(isFrontText);
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
        int64_t gotNode = dec.readLong();   // readBlockPos == BlockPos.of(readLong)
        bool gotFront = dec.readBool();

        int32_t gotX = blockPosGetX(gotNode);
        int32_t gotY = blockPosGetY(gotNode);
        int32_t gotZ = blockPosGetZ(gotNode);

        // Expected sign-extended coords (what BlockPos.of yields back).
        int64_t expNode = blockPosAsLong(x, y, z);
        int32_t expX = blockPosGetX(expNode);
        int32_t expY = blockPosGetY(expNode);
        int32_t expZ = blockPosGetZ(expNode);

        if (gotNode != expNode || gotX != expX || gotY != expY || gotZ != expZ) {
            ++mismatches;
            std::cerr << "DEC-POS " << name << " node got=" << gotNode << " want=" << expNode
                      << " (x " << gotX << "/" << expX << " y " << gotY << "/" << expY
                      << " z " << gotZ << "/" << expZ << ")\n";
        }
        if (gotFront != isFrontText) {
            ++mismatches;
            std::cerr << "DEC-FRONT " << name << " got=" << gotFront
                      << " want=" << isFrontText << "\n";
        }
    }

    std::cout << "PktOpenSignEditorParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
