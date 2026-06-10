// Parity gate for net.minecraft.network.protocol.game.ServerboundMovePlayerPacket.PosRot's
// StreamCodec vs the REAL net.minecraft codec (tools/PktMovePlayerPosRotParity.java GT).
//
// PosRot (ServerboundMovePlayerPacket.java:144-184) carries: double x, double y, double z,
// float yRot, float xRot, plus two booleans (onGround, horizontalCollision) packed into ONE
// byte. Its STREAM_CODEC is Packet.codec(PosRot::write, PosRot::read) == StreamCodec.ofMember
// (Packet.java:22-24), so there is NO packet-id prefix and the on-wire body is EXACTLY what
// PosRot.write(FriendlyByteBuf) emits, IN THIS EXACT ORDER (ServerboundMovePlayerPacket.java
// :171-178):
//   output.writeDouble(this.x);     // big-endian IEEE-754 8 bytes  (writeDouble)
//   output.writeDouble(this.y);     // big-endian IEEE-754 8 bytes  (writeDouble)
//   output.writeDouble(this.z);     // big-endian IEEE-754 8 bytes  (writeDouble)
//   output.writeFloat(this.yRot);   // big-endian IEEE-754 4 bytes  (writeFloat)
//   output.writeFloat(this.xRot);   // big-endian IEEE-754 4 bytes  (writeFloat)
//   output.writeByte(packFlags(onGround, horizontalCollision)); // 1 byte: bit0=onGround, bit1=horizColl
// where packFlags (ServerboundMovePlayerPacket.java:22-33) = (onGround?1:0)|(horizontalCollision?2:0).
// So the body is exactly: double(x)++double(y)++double(z)++float(yRot)++float(xRot)++byte(flags),
// 8+8+8+4+4+1 = 25 bytes.
//
// All fields are plain primitives, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeDouble/readDouble + writeFloat/readFloat (big-endian
// IEEE-754 bit-cast) and writeByte/readByte are byte-for-byte identical to the real codec.
// x/y/z are carried through the TSV as raw long bits (%016x) and yRot/xRot as raw int bits
// (%08x) so NaN/Inf/-0.0 are exercised exactly without float-parse rounding. The flags byte is
// reconstructed C++-side from onGround/horizontalCollision exactly as packFlags does.
//
//   pkt_move_player_posrot_parity [--cases mcpp/build/pkt_move_player_posrot.tsv]
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

// Reinterpret raw 64-bit IEEE-754 bits as a double (matches Double.longBitsToDouble).
double bitsToDouble(uint64_t bits) {
    double v;
    std::memcpy(&v, &bits, 8);
    return v;
}
// Reinterpret a double as its raw 64-bit IEEE-754 bits (Double.doubleToRawLongBits).
uint64_t doubleToBits(double v) {
    uint64_t bits;
    std::memcpy(&bits, &v, 8);
    return bits;
}
// Reinterpret raw 32-bit IEEE-754 bits as a float (matches Float.intBitsToFloat).
float bitsToFloat(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
// Reinterpret a float as its raw 32-bit IEEE-754 bits (Float.floatToRawIntBits).
uint32_t floatToBits(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}

// packFlags(onGround, horizontalCollision): bit0=onGround, bit1=horizontalCollision
// (ServerboundMovePlayerPacket.java:22-33).
uint8_t packFlags(bool onGround, bool horizontalCollision) {
    uint8_t flags = 0;
    if (onGround) flags |= 1;
    if (horizontalCollision) flags |= 2;
    return flags;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_move_player_posrot.tsv";
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

        if (tag == "ENC") {
            // ENC <xBits> <yBits> <zBits> <yRotBits> <xRotBits> <onGround> <horizColl>
            //     <readableBytes> <hexBytes>
            std::string xStr, yStr, zStr, yRotStr, xRotStr, onStr, horizStr, nStr, expHex;
            if (!std::getline(ss, xStr, '\t') || !std::getline(ss, yStr, '\t')
                || !std::getline(ss, zStr, '\t') || !std::getline(ss, yRotStr, '\t')
                || !std::getline(ss, xRotStr, '\t') || !std::getline(ss, onStr, '\t')
                || !std::getline(ss, horizStr, '\t') || !std::getline(ss, nStr, '\t')
                || !std::getline(ss, expHex)) continue;
            ++cases;
            double x = bitsToDouble((uint64_t)std::stoull(xStr, nullptr, 16));
            double y = bitsToDouble((uint64_t)std::stoull(yStr, nullptr, 16));
            double z = bitsToDouble((uint64_t)std::stoull(zStr, nullptr, 16));
            float yRot = bitsToFloat((uint32_t)std::stoul(yRotStr, nullptr, 16));
            float xRot = bitsToFloat((uint32_t)std::stoul(xRotStr, nullptr, 16));
            bool onGround = std::stoi(onStr) != 0;
            bool horizColl = std::stoi(horizStr) != 0;
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeDouble(x) ++ writeDouble(y) ++ writeDouble(z) ++ writeFloat(yRot)
            //          ++ writeFloat(xRot) ++ writeByte(packFlags(onGround,horizColl)) -- EXACT.
            PacketBuffer enc;
            enc.writeDouble(x);
            enc.writeDouble(y);
            enc.writeDouble(z);
            enc.writeFloat(yRot);
            enc.writeFloat(xRot);
            enc.writeByte(packFlags(onGround, horizColl));

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH xBits=" << xStr << " yBits=" << yStr
                          << " zBits=" << zStr << " yRotBits=" << yRotStr
                          << " xRotBits=" << xRotStr << " onGround=" << onStr
                          << " horizColl=" << horizStr
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <xBits_in> <yBits_in> <zBits_in> <yRotBits_in> <xRotBits_in>
            //     <onGround_in> <horizColl_in> <xBits_dec> <yBits_dec> <zBits_dec>
            //     <yRotBits_dec> <xRotBits_dec> <onGround_dec> <horizColl_dec>
            std::string inHex,
                xIn, yIn, zIn, yRotIn, xRotIn, onIn, horizIn,
                xDec, yDec, zDec, yRotDec, xRotDec, onDec, horizDec;
            if (!std::getline(ss, inHex, '\t')
                || !std::getline(ss, xIn, '\t') || !std::getline(ss, yIn, '\t')
                || !std::getline(ss, zIn, '\t') || !std::getline(ss, yRotIn, '\t')
                || !std::getline(ss, xRotIn, '\t') || !std::getline(ss, onIn, '\t')
                || !std::getline(ss, horizIn, '\t')
                || !std::getline(ss, xDec, '\t') || !std::getline(ss, yDec, '\t')
                || !std::getline(ss, zDec, '\t') || !std::getline(ss, yRotDec, '\t')
                || !std::getline(ss, xRotDec, '\t') || !std::getline(ss, onDec, '\t')
                || !std::getline(ss, horizDec)) continue;
            ++cases;
            uint64_t expXBits = (uint64_t)std::stoull(xDec, nullptr, 16);
            uint64_t expYBits = (uint64_t)std::stoull(yDec, nullptr, 16);
            uint64_t expZBits = (uint64_t)std::stoull(zDec, nullptr, 16);
            uint32_t expYRotBits = (uint32_t)std::stoul(yRotDec, nullptr, 16);
            uint32_t expXRotBits = (uint32_t)std::stoul(xRotDec, nullptr, 16);
            bool expOn = std::stoi(onDec) != 0;
            bool expHoriz = std::stoi(horizDec) != 0;

            // read(): readDouble() x3 ++ readFloat() x2 ++ readByte() (unpack flags).
            PacketBuffer dec(unhex(inHex));
            uint64_t gotXBits = doubleToBits(dec.readDouble());
            uint64_t gotYBits = doubleToBits(dec.readDouble());
            uint64_t gotZBits = doubleToBits(dec.readDouble());
            uint32_t gotYRotBits = floatToBits(dec.readFloat());
            uint32_t gotXRotBits = floatToBits(dec.readFloat());
            uint8_t flags = dec.readByte();
            bool gotOn = (flags & 1) != 0;        // unpackOnGround
            bool gotHoriz = (flags & 2) != 0;     // unpackHorizontalCollision
            if (gotXBits != expXBits || gotYBits != expYBits || gotZBits != expZBits
                || gotYRotBits != expYRotBits || gotXRotBits != expXRotBits
                || gotOn != expOn || gotHoriz != expHoriz) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << std::hex
                          << " got xBits=" << gotXBits << " yBits=" << gotYBits
                          << " zBits=" << gotZBits << " yRotBits=" << gotYRotBits
                          << " xRotBits=" << gotXRotBits << std::dec
                          << " on=" << gotOn << " horiz=" << gotHoriz
                          << " want xBits=" << xDec << " yBits=" << yDec << " zBits=" << zDec
                          << " yRotBits=" << yRotDec << " xRotBits=" << xRotDec
                          << " on=" << onDec << " horiz=" << horizDec << "\n";
            }
        }
    }

    std::cout << "PktMovePlayerPosRotParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
