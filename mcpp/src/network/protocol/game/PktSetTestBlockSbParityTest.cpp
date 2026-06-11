// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundSetTestBlockPacket
// against the REAL STREAM_CODEC (tools/PktSetTestBlockSbParity.java ground truth).
//
// The packet is a record (position, mode, message) encoded by a StreamCodec.composite
// (ServerboundSetTestBlockPacket.java:12-20) in this exact field order:
//   1) BlockPos.STREAM_CODEC      -> writeBlockPos(pos) == writeLong(pos.asLong())  (8B BE)
//   2) TestBlockMode.STREAM_CODEC -> ByteBufCodecs.idMapper.encode == VarInt.write(mode.id)
//        (ByteBufCodecs.java:542-553). id == ordinal: START=0 LOG=1 FAIL=2 ACCEPT=3.
//   3) ByteBufCodecs.STRING_UTF8  -> Utf8String.write == VarInt(byteLen) + UTF-8 bytes.
// The stream codec is the packet body only (no packet-id / length prefix on the wire).
//
// BlockPos.asLong (BlockPos.java), PACKED_HORIZONTAL_LENGTH = 26, PACKED_Y_LENGTH = 12,
// X_OFFSET = 38, Z_OFFSET = 12, Y_OFFSET = 0:
//   node = ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF).
// BlockPos.of/getX/getY/getZ sign-extend each field on the way back.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
//   writeLong(asLong(x,y,z)) + writeVarInt(modeId) + writeString(message)
// is byte-for-byte the same as the real codec, and readLong()/readVarInt()/readString()
// round-trips the fields value-for-value. writeString == writeUtf == Utf8String.write
// (VarInt byte-length prefix + UTF-8 bytes). The message arrives as a UTF-8 HEX column.
//
//   pkt_set_test_block_sb_parity [--cases mcpp/build/pkt_set_test_block_sb.tsv]
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

// A UTF-8 HEX column -> the raw byte string the packet carries.
std::string hexToStr(const std::string& h) {
    std::vector<uint8_t> b = unhex(h);
    return std::string(b.begin(), b.end());
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
int32_t blockPosGetX(int64_t node) { return (int32_t)(node >> 38); }
int32_t blockPosGetY(int64_t node) { return (int32_t)((node << 52) >> 52); }
int32_t blockPosGetZ(int64_t node) { return (int32_t)((node << 26) >> 38); }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_set_test_block_sb.tsv";
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

        // ENC <name> <x> <y> <z> <modeId> <messageHex> <readableBytes> <hexBytes>
        std::string name, xS, yS, zS, mS, msgH, rbS, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, xS, '\t')
            || !std::getline(ss, yS, '\t') || !std::getline(ss, zS, '\t')
            || !std::getline(ss, mS, '\t') || !std::getline(ss, msgH, '\t')
            || !std::getline(ss, rbS, '\t') || !std::getline(ss, expHex))
            continue;
        ++cases;

        int32_t x = (int32_t)std::stoll(xS);
        int32_t y = (int32_t)std::stoll(yS);
        int32_t z = (int32_t)std::stoll(zS);
        int32_t modeId = (int32_t)std::stoll(mS);
        std::string message = hexToStr(msgH);
        size_t expReadable = (size_t)std::stoull(rbS);

        bool ok = true;

        // write(): writeBlockPos(pos)=writeLong(asLong) + writeVarInt(modeId) + writeUtf(message).
        PacketBuffer enc;
        try {
            enc.writeLong(blockPosAsLong(x, y, z));
            enc.writeVarInt(modeId);
            enc.writeString(message);
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

        // read(): decode the expected bytes back and require the fields round-trip.
        try {
            std::vector<uint8_t> bytes = unhex(expHex);
            PacketBuffer dec(bytes);
            int64_t gotNode = dec.readLong();   // readBlockPos == BlockPos.of(readLong)
            int32_t gotMode = dec.readVarInt();
            std::string gotMsg = dec.readString();

            int64_t expNode = blockPosAsLong(x, y, z);
            if (gotNode != expNode
                || blockPosGetX(gotNode) != blockPosGetX(expNode)
                || blockPosGetY(gotNode) != blockPosGetY(expNode)
                || blockPosGetZ(gotNode) != blockPosGetZ(expNode)) {
                std::cerr << "DEC-POS " << name << " node got=" << gotNode
                          << " want=" << expNode << "\n";
                ok = false;
            }
            if (gotMode != modeId) {
                std::cerr << "DEC-MODE " << name << " got=" << gotMode
                          << " want=" << modeId << "\n";
                ok = false;
            }
            if (gotMsg != message) {
                std::cerr << "DEC-MSG " << name << " mismatch\n";
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

    std::cout << "PktSetTestBlockSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
