// Parity gate for net.minecraft.network.protocol.game.ClientboundPlayerRotationPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktPlayerRotationParity.java GT).
//
// The packet is a record(float yRot, boolean relativeY, float xRot, boolean relativeX)
// (ClientboundPlayerRotationPacket.java:9-20). Its STREAM_CODEC is StreamCodec.composite
// of, IN THIS EXACT ORDER:
//   ByteBufCodecs.FLOAT -> yRot       // big-endian IEEE-754 4 bytes  (writeFloat)
//   ByteBufCodecs.BOOL  -> relativeY  // 1 byte 0/1                   (writeBoolean)
//   ByteBufCodecs.FLOAT -> xRot       // big-endian IEEE-754 4 bytes  (writeFloat)
//   ByteBufCodecs.BOOL  -> relativeX  // 1 byte 0/1                   (writeBoolean)
// So the on-wire body is exactly: float(yRot) ++ bool(relativeY) ++ float(xRot) ++
// bool(relativeX), 10 bytes, NO packet-id prefix (StreamCodec.composite).
//
// All four fields are plain primitives, so this exercises the certified PacketBuffer
// (the FriendlyByteBuf port) directly: writeFloat/readFloat (big-endian IEEE-754
// bit-cast) and writeBool/readBool (1 byte 0/1) are byte-for-byte / bit-for-bit
// identical to the real codec. yRot/xRot are carried through the TSV as their raw int
// bits (%08x) so NaN/Inf/-0.0 are exercised exactly without float-parse rounding.
//
//   pkt_player_rotation_parity [--cases mcpp/build/pkt_player_rotation.tsv]
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
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_player_rotation.tsv";
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
            // ENC <yRotBits> <relY> <xRotBits> <relX> <readableBytes> <hexBytes>
            std::string yStr, relYStr, xStr, relXStr, nStr, expHex;
            if (!std::getline(ss, yStr, '\t') || !std::getline(ss, relYStr, '\t')
                || !std::getline(ss, xStr, '\t') || !std::getline(ss, relXStr, '\t')
                || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            float yRot = bitsToFloat((uint32_t)std::stoul(yStr, nullptr, 16));
            bool relativeY = std::stoi(relYStr) != 0;
            float xRot = bitsToFloat((uint32_t)std::stoul(xStr, nullptr, 16));
            bool relativeX = std::stoi(relXStr) != 0;
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeFloat(yRot) ++ writeBool(relativeY) ++ writeFloat(xRot)
            //          ++ writeBool(relativeX) -- EXACT codec order.
            PacketBuffer enc;
            enc.writeFloat(yRot);
            enc.writeBool(relativeY);
            enc.writeFloat(xRot);
            enc.writeBool(relativeX);

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH yBits=" << yStr << " relY=" << relYStr
                          << " xBits=" << xStr << " relX=" << relXStr
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <yBits_in> <relY_in> <xBits_in> <relX_in>
            //     <yBits_dec> <relY_dec> <xBits_dec> <relX_dec>
            std::string inHex, yIn, relYIn, xIn, relXIn, yDec, relYDec, xDec, relXDec;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, yIn, '\t')
                || !std::getline(ss, relYIn, '\t') || !std::getline(ss, xIn, '\t')
                || !std::getline(ss, relXIn, '\t') || !std::getline(ss, yDec, '\t')
                || !std::getline(ss, relYDec, '\t') || !std::getline(ss, xDec, '\t')
                || !std::getline(ss, relXDec)) continue;
            ++cases;
            uint32_t expYBits = (uint32_t)std::stoul(yDec, nullptr, 16);
            bool expRelY = std::stoi(relYDec) != 0;
            uint32_t expXBits = (uint32_t)std::stoul(xDec, nullptr, 16);
            bool expRelX = std::stoi(relXDec) != 0;

            // read(): readFloat() ++ readBool() ++ readFloat() ++ readBool().
            PacketBuffer dec(unhex(inHex));
            uint32_t gotYBits = floatToBits(dec.readFloat());
            bool gotRelY = dec.readBool();
            uint32_t gotXBits = floatToBits(dec.readFloat());
            bool gotRelX = dec.readBool();
            if (gotYBits != expYBits || gotRelY != expRelY
                || gotXBits != expXBits || gotRelX != expRelX) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << std::hex
                          << " got yBits=" << gotYBits << " relY=" << std::dec << gotRelY
                          << " xBits=" << std::hex << gotXBits << " relX=" << std::dec << gotRelX
                          << " want yBits=" << yDec << " relY=" << relYDec
                          << " xBits=" << xDec << " relX=" << relXDec << "\n";
            }
        }
    }

    std::cout << "PktPlayerRotationParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
