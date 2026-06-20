// Parity gate for net.minecraft.network.protocol.game.ServerboundMoveVehiclePacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktMoveVehicleSbParity.java GT).
//
// The packet is a record(Vec3 position, float yRot, float xRot, boolean onGround) and
// its STREAM_CODEC is StreamCodec.composite over, in this exact wire order
// (ServerboundMoveVehiclePacket.java:11-22):
//   Vec3.STREAM_CODEC   -> position  (Vec3.java:25-35: writeDouble(x); writeDouble(y); writeDouble(z))
//   ByteBufCodecs.FLOAT -> yRot      (ByteBufCodecs.java:132-140: writeFloat)
//   ByteBufCodecs.FLOAT -> xRot      (ByteBufCodecs.java:132-140: writeFloat)
//   ByteBufCodecs.BOOL  -> onGround  (ByteBufCodecs.java:56-64:  writeBoolean)
// So the body is exactly: double x, double y, double z (each big-endian IEEE-754, 8B)
// then float yRot, float xRot (each big-endian IEEE-754, 4B) then 1 byte bool (00/01).
// Packet.codec -> no packet-id prefix, just the body (always 8+8+8+4+4+1 = 33 bytes).
//
// Every field is a plain number/bool, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeDouble/readDouble and writeFloat/readFloat
// (big-endian IEEE-754 bit-cast) and writeBool/readBool are byte-for-byte / bit-for-bit
// identical to the real codec. Doubles/floats are carried through the TSV as their raw
// long/int bits (%016x / %08x) so NaN/Inf/-0.0 are exercised exactly without parse
// rounding.
//
//   pkt_move_vehicle_sb_parity [--cases mcpp/build/pkt_move_vehicle_sb.tsv]
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
    double d;
    std::memcpy(&d, &bits, 8);
    return d;
}
uint64_t doubleToBits(double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, 8);
    return bits;
}
// Reinterpret raw 32-bit IEEE-754 bits as a float (matches Float.intBitsToFloat).
float bitsToFloat(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
uint32_t floatToBits(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_move_vehicle_sb.tsv";
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
            // ENC <xBits> <yBits> <zBits> <yRotBits> <xRotBits> <onGround> <readableBytes> <hexBytes>
            std::string xS, yS, zS, yrS, xrS, gS, nS, expHex;
            if (!std::getline(ss, xS, '\t') || !std::getline(ss, yS, '\t')
                || !std::getline(ss, zS, '\t') || !std::getline(ss, yrS, '\t')
                || !std::getline(ss, xrS, '\t') || !std::getline(ss, gS, '\t')
                || !std::getline(ss, nS, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            double x = bitsToDouble((uint64_t)std::stoull(xS, nullptr, 16));
            double y = bitsToDouble((uint64_t)std::stoull(yS, nullptr, 16));
            double z = bitsToDouble((uint64_t)std::stoull(zS, nullptr, 16));
            float yRot = bitsToFloat((uint32_t)std::stoul(yrS, nullptr, 16));
            float xRot = bitsToFloat((uint32_t)std::stoul(xrS, nullptr, 16));
            bool onGround = std::stoi(gS) != 0;
            size_t expN = (size_t)std::stoull(nS);

            // write(): the composite codec writes, in order, writeDouble(x),
            // writeDouble(y), writeDouble(z) [Vec3], writeFloat(yRot), writeFloat(xRot),
            // writeBool(onGround).
            PacketBuffer enc;
            enc.writeDouble(x);
            enc.writeDouble(y);
            enc.writeDouble(z);
            enc.writeFloat(yRot);
            enc.writeFloat(xRot);
            enc.writeBool(onGround);

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH x=" << xS << " y=" << yS << " z=" << zS
                          << " yRot=" << yrS << " xRot=" << xrS << " g=" << gS
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <xBits> <yBits> <zBits> <yRotBits> <xRotBits> <onGround>
            //     <decXBits> <decYBits> <decZBits> <decYRotBits> <decXRotBits> <decOnGround>
            std::string inHex, xS, yS, zS, yrS, xrS, gS,
                        dxS, dyS, dzS, dyrS, dxrS, dgS;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, xS, '\t')
                || !std::getline(ss, yS, '\t') || !std::getline(ss, zS, '\t')
                || !std::getline(ss, yrS, '\t') || !std::getline(ss, xrS, '\t')
                || !std::getline(ss, gS, '\t') || !std::getline(ss, dxS, '\t')
                || !std::getline(ss, dyS, '\t') || !std::getline(ss, dzS, '\t')
                || !std::getline(ss, dyrS, '\t') || !std::getline(ss, dxrS, '\t')
                || !std::getline(ss, dgS)) continue;
            ++cases;
            uint64_t expXBits = (uint64_t)std::stoull(dxS, nullptr, 16);
            uint64_t expYBits = (uint64_t)std::stoull(dyS, nullptr, 16);
            uint64_t expZBits = (uint64_t)std::stoull(dzS, nullptr, 16);
            uint32_t expYRotBits = (uint32_t)std::stoul(dyrS, nullptr, 16);
            uint32_t expXRotBits = (uint32_t)std::stoul(dxrS, nullptr, 16);
            bool expGround = std::stoi(dgS) != 0;

            // read(): readDouble x3, readFloat x2, readBool, in that exact order.
            PacketBuffer dec(unhex(inHex));
            double gx = dec.readDouble();
            double gy = dec.readDouble();
            double gz = dec.readDouble();
            float gYRot = dec.readFloat();
            float gXRot = dec.readFloat();
            bool gGround = dec.readBool();
            uint64_t gxBits = doubleToBits(gx);
            uint64_t gyBits = doubleToBits(gy);
            uint64_t gzBits = doubleToBits(gz);
            uint32_t gYRotBits = floatToBits(gYRot);
            uint32_t gXRotBits = floatToBits(gXRot);
            if (gxBits != expXBits || gyBits != expYBits || gzBits != expZBits
                || gYRotBits != expYRotBits || gXRotBits != expXRotBits
                || gGround != expGround || dec.remaining() != 0) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " remaining=" << dec.remaining() << "\n";
            }
        }
    }

    std::cout << "PktMoveVehicleSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
