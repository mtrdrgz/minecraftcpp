// Parity gate for net.minecraft.network.protocol.game.ServerboundPlayerActionPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktPlayerActionSbParity.java
// ground truth).
//
// The packet body is exactly (ServerboundPlayerActionPacket.java:37-42):
//   write : output.writeEnum(this.action);                    == writeVarInt(action.ordinal())  (FriendlyByteBuf.java:471-473)
//           output.writeBlockPos(this.pos);                   == writeLong(pos.asLong())         (FriendlyByteBuf.java:398-400)
//           output.writeByte(this.direction.get3DDataValue());// 1 byte, low 8 bits (0..5)
//           output.writeVarInt(this.sequence);                // VarInt
//   read  : this.action    = input.readEnum(Action.class);    // VarInt ordinal -> Action.values()[i]
//           this.pos       = input.readBlockPos();             == BlockPos.of(input.readLong())  (FriendlyByteBuf.java:389-391)
//           this.direction = Direction.from3DDataValue(input.readUnsignedByte()); // 0..255 (wraps %6)
//           this.sequence  = input.readVarInt();               // VarInt
// Packet.codec -> StreamCodec.ofMember (Packet.java): body only, no id/length prefix.
//
// Action enum ordinals: START_DESTROY_BLOCK=0 .. STAB=7 (ServerboundPlayerActionPacket.java:69-78).
// Direction.get3DDataValue: DOWN=0,UP=1,NORTH=2,SOUTH=3,WEST=4,EAST=5 (Direction.java:33-38).
//
// BlockPos.asLong (BlockPos.java:107-116), with PACKED_HORIZONTAL_LENGTH = 26,
// PACKED_Y_LENGTH = 12, X_OFFSET = 38, Z_OFFSET = 12:
//   node = ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF).
// BlockPos.of/getX/getY/getZ sign-extend each field on the way back.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
//   writeVarInt(actionOrdinal) + writeLong(asLong(x,y,z)) + writeByte(dir3d) + writeVarInt(sequence)
// is byte-for-byte the same as the real codec, and readVarInt()/readLong()/readByte()/readVarInt()
// round-trips the fields value-for-value.
//
//   pkt_player_action_sb_parity [--cases mcpp/build/pkt_player_action_sb.tsv]
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

// BlockPos.asLong(x,y,z) — BlockPos.java:111-116. 26/12/26 packing.
// X_OFFSET = 38, Z_OFFSET = 12, Y_OFFSET = 0. Masks: X/Z = 0x3FFFFFF (26 bits),
// Y = 0xFFF (12 bits).
int64_t blockPosAsLong(int32_t x, int32_t y, int32_t z) {
    uint64_t node = 0;
    node |= ((uint64_t)((uint32_t)x & 0x3FFFFFFu)) << 38;
    node |= ((uint64_t)((uint32_t)y & 0xFFFu)) << 0;
    node |= ((uint64_t)((uint32_t)z & 0x3FFFFFFu)) << 12;
    return (int64_t)node;
}

// BlockPos.getX/getY/getZ — sign-extend each packed field.
int32_t blockPosGetX(int64_t node) { return (int32_t)(node >> 38); }
int32_t blockPosGetY(int64_t node) { return (int32_t)((node << 52) >> 52); }
int32_t blockPosGetZ(int64_t node) { return (int32_t)((node << 26) >> 38); }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_player_action_sb.tsv";
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

        // ENC <name> <actionOrdinal> <x> <y> <z> <dir3d> <sequence> <readableBytes> <hexBytes>
        std::string name, aS, xS, yS, zS, dS, seqS, rbS, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, aS, '\t')
            || !std::getline(ss, xS, '\t') || !std::getline(ss, yS, '\t')
            || !std::getline(ss, zS, '\t') || !std::getline(ss, dS, '\t')
            || !std::getline(ss, seqS, '\t') || !std::getline(ss, rbS, '\t')
            || !std::getline(ss, expHex))
            continue;
        ++cases;

        int32_t actOrd = (int32_t)std::stoll(aS);
        int32_t x = (int32_t)std::stoll(xS);
        int32_t y = (int32_t)std::stoll(yS);
        int32_t z = (int32_t)std::stoll(zS);
        int32_t dir3d = (int32_t)std::stoll(dS);
        int32_t sequence = (int32_t)std::stoll(seqS);
        size_t expReadable = (size_t)std::stoull(rbS);

        // write(): writeEnum(action)=writeVarInt(ordinal) + writeBlockPos(pos)=writeLong(asLong)
        //          + writeByte(dir3d) + writeVarInt(sequence).
        PacketBuffer enc;
        enc.writeVarInt(actOrd);
        enc.writeLong(blockPosAsLong(x, y, z));
        enc.writeByte((uint8_t)(dir3d & 0xff));  // FriendlyByteBuf.writeByte(int) -> low 8 bits
        enc.writeVarInt(sequence);
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
        int32_t gotAction = dec.readVarInt();      // readEnum -> Action.values()[ordinal]
        int64_t gotNode = dec.readLong();          // readBlockPos == BlockPos.of(readLong)
        int32_t gotDir = dec.readByte();           // readUnsignedByte -> 0..255
        int32_t gotSeq = dec.readVarInt();

        int32_t gotX = blockPosGetX(gotNode);
        int32_t gotY = blockPosGetY(gotNode);
        int32_t gotZ = blockPosGetZ(gotNode);

        int64_t expNode = blockPosAsLong(x, y, z);
        int32_t expX = blockPosGetX(expNode);
        int32_t expY = blockPosGetY(expNode);
        int32_t expZ = blockPosGetZ(expNode);

        if (gotAction != actOrd) {
            ++mismatches;
            std::cerr << "DEC-ACTION " << name << " got=" << gotAction << " want=" << actOrd << "\n";
        }
        if (gotNode != expNode || gotX != expX || gotY != expY || gotZ != expZ) {
            ++mismatches;
            std::cerr << "DEC-POS " << name << " node got=" << gotNode << " want=" << expNode
                      << " (x " << gotX << "/" << expX << " y " << gotY << "/" << expY
                      << " z " << gotZ << "/" << expZ << ")\n";
        }
        // All test dir3d are 0..5, so from3DDataValue is identity (no wrap).
        if (gotDir != dir3d) {
            ++mismatches;
            std::cerr << "DEC-DIR " << name << " got=" << gotDir << " want=" << dir3d << "\n";
        }
        if (gotSeq != sequence) {
            ++mismatches;
            std::cerr << "DEC-SEQ " << name << " got=" << gotSeq << " want=" << sequence << "\n";
        }
    }

    std::cout << "PktPlayerActionSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
