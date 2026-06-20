// Parity gate for net.minecraft.network.protocol.game.ServerboundMovePlayerPacket.Rot's
// StreamCodec vs the REAL net.minecraft codec (tools/PktMovePlayerRotParity.java GT).
//
// ServerboundMovePlayerPacket.Rot.write(FriendlyByteBuf) is, IN THIS EXACT ORDER
// (ServerboundMovePlayerPacket.java:204-208):
//   output.writeFloat(this.yRot);   // big-endian IEEE-754 4 bytes
//   output.writeFloat(this.xRot);   // big-endian IEEE-754 4 bytes
//   output.writeByte(packFlags(this.onGround, this.horizontalCollision)); // 1 byte
// where packFlags (ServerboundMovePlayerPacket.java:22-33):
//   flags = (onGround ? 1 : 0) | (horizontalCollision ? 2 : 0).
// On-wire body: float(yRot) ++ float(xRot) ++ byte(flags) = 9 bytes, no packet-id prefix.
//
// All fields are plain primitives, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeFloat/readFloat (big-endian IEEE-754 bit-cast) and
// writeByte/readByte are byte-for-byte identical to the real codec. The two booleans
// (onGround, horizontalCollision) are packed into one byte exactly as packFlags does.
// yRot/xRot are carried through the TSV as their raw int bits (%08x) so NaN/Inf/-0.0 are
// exercised exactly without float-parse rounding.
//
//   pkt_move_player_rot_parity [--cases mcpp/build/pkt_move_player_rot.tsv]
#include "../../PacketBuffer.h"

#include <cstdint>
#include <cstring>
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

// Reinterpret raw 32-bit IEEE-754 bits as a float (matches Float.intBitsToFloat).
float bitsToFloat(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

// Reinterpret a float as its raw 32-bit IEEE-754 bits (matches Float.floatToRawIntBits).
uint32_t floatToBits(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}

// packFlags: (onGround ? 1 : 0) | (horizontalCollision ? 2 : 0).
uint8_t packFlags(bool onGround, bool horizontalCollision) {
    int flags = 0;
    if (onGround) flags |= 1;
    if (horizontalCollision) flags |= 2;
    return (uint8_t)flags;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_move_player_rot.tsv";
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

        // ENC <yRotBits> <xRotBits> <onGround> <horizontalCollision>
        //     <readableBytes> <hexBytes>
        std::string yStr, xStr, ogStr, hcStr, nStr, expHex;
        if (!std::getline(ss, yStr, '\t') || !std::getline(ss, xStr, '\t')
            || !std::getline(ss, ogStr, '\t') || !std::getline(ss, hcStr, '\t')
            || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;
        float yRot = bitsToFloat((uint32_t)std::stoul(yStr, nullptr, 16));
        float xRot = bitsToFloat((uint32_t)std::stoul(xStr, nullptr, 16));
        bool onGround = std::stoi(ogStr) != 0;
        bool horizontalCollision = std::stoi(hcStr) != 0;
        size_t expN = (size_t)std::stoull(nStr);

        // write(): writeFloat(yRot) ++ writeFloat(xRot) ++ writeByte(packFlags(...))
        //          -- EXACT codec order.
        PacketBuffer enc;
        enc.writeFloat(yRot);
        enc.writeFloat(xRot);
        enc.writeByte(packFlags(onGround, horizontalCollision));

        std::string got = hex(enc.data());
        if (got != expHex || enc.size() != expN) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH yBits=" << yStr << " xBits=" << xStr
                      << " onGround=" << ogStr << " horizontalCollision=" << hcStr
                      << "\n  got  n=" << enc.size() << " " << got
                      << "\n  want n=" << expN << " " << expHex << "\n";
            continue;
        }

        // read(): decode the SAME expected bytes back through PacketBuffer; require the
        // fields round-trip (readFloat ++ readFloat ++ readByte -> unpack the two bits).
        PacketBuffer dec(unhex(expHex));
        uint32_t gotYBits = floatToBits(dec.readFloat());
        uint32_t gotXBits = floatToBits(dec.readFloat());
        uint8_t flags = dec.readByte();
        bool gotOnGround = (flags & 1) != 0;
        bool gotHorizontalCollision = (flags & 2) != 0;
        uint32_t expYBits = (uint32_t)std::stoul(yStr, nullptr, 16);
        uint32_t expXBits = (uint32_t)std::stoul(xStr, nullptr, 16);
        if (gotYBits != expYBits || gotXBits != expXBits
            || gotOnGround != onGround || gotHorizontalCollision != horizontalCollision) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << std::hex
                      << " got yBits=" << gotYBits << " xBits=" << gotXBits << std::dec
                      << " onGround=" << gotOnGround << " hc=" << gotHorizontalCollision
                      << " want yBits=" << yStr << " xBits=" << xStr
                      << " onGround=" << ogStr << " hc=" << hcStr << "\n";
        }
    }

    std::cout << "PktMovePlayerRotParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
