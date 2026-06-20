// Byte-exact parity gate for net.minecraft.network.protocol.game.ClientboundSetDefaultSpawnPositionPacket
// vs the REAL ClientboundSetDefaultSpawnPositionPacket.STREAM_CODEC (tools/PktSetDefaultSpawnParity.java).
//
// The packet wraps LevelData.RespawnData(GlobalPos globalPos, float yaw, float pitch).
// The FULL wire body, in codec order, is (see GT tool header for the verbatim source map):
//   writeUtf(dimensionId)   -- VarInt UTF-8 byte length + UTF-8 bytes  (Identifier.STREAM_CODEC -> STRING_UTF8)
//   writeLong(pos.asLong()) -- big-endian 8-byte long                  (BlockPos.STREAM_CODEC -> writeBlockPos)
//   writeFloat(yaw)         -- big-endian 4 bytes (rawIntBits)         (ByteBufCodecs.FLOAT)
//   writeFloat(pitch)       -- big-endian 4 bytes (rawIntBits)         (ByteBufCodecs.FLOAT)
//
// No Holder / ItemStack / Component / NBT / registry id is on the wire -- the dimension is a
// plain ResourceLocation string -- so the certified PacketBuffer (the FriendlyByteBuf port)
// rebuilds the body directly: writeString(dim) + writeLong(posLong) + writeFloat(yaw) + writeFloat(pitch).
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte and the
// same readableBytes; decode(Java bytes) must then recover dim/posLong/yawBits/pitchBits.
//
//   pkt_set_default_spawn_parity [--cases mcpp/build/pkt_set_default_spawn.tsv]
//
// Row: ENC <name> <dimHex> <posLong-dec> <yawBits-%08x> <pitchBits-%08x> <readableBytes> <hex>
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
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> v;
    v.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return v;
}

// Decode a hex string of UTF-8 bytes back into the raw std::string (the dimension id).
std::string hexToStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((char)(uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return out;
}

float bitsToFloat(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_set_default_spawn.tsv";
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
        std::string tag, name, dimHex, posStr, yawStr, pitchStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, dimHex, '\t') ||
            !std::getline(ss, posStr, '\t') || !std::getline(ss, yawStr, '\t') ||
            !std::getline(ss, pitchStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        std::string dim = hexToStr(dimHex);
        int64_t posLong = (int64_t)std::stoll(posStr);
        uint32_t yawBits = (uint32_t)std::stoul(yawStr, nullptr, 16);
        uint32_t pitchBits = (uint32_t)std::stoul(pitchStr, nullptr, 16);
        size_t readableBytes = (size_t)std::stoul(lenStr);

        // (1) ENCODE: write the body in the same codec order via PacketBuffer and compare.
        PacketBuffer enc;
        enc.writeString(dim);                 // writeUtf: VarInt len + UTF-8 bytes
        enc.writeLong(posLong);               // writeBlockPos == writeLong(asLong), BE
        enc.writeFloat(bitsToFloat(yawBits)); // ByteBufCodecs.FLOAT, BE rawIntBits
        enc.writeFloat(bitsToFloat(pitchBits));

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " dim=" << dim << " pos=" << posLong
                      << " yawBits=" << yawStr << " pitchBits=" << pitchStr
                      << "\n  got  (" << enc.data().size() << ") " << got
                      << "\n  want (" << readableBytes << ") " << expHex << "\n";
            continue;
        }

        // (2) DECODE: read the Java bytes back and verify every field.
        std::vector<uint8_t> raw = unhex(expHex);
        if (raw.size() != readableBytes) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH " << name << " hex=" << expHex
                      << " bytes=" << raw.size() << " want=" << readableBytes << "\n";
            continue;
        }
        PacketBuffer dec(raw);
        std::string gotDim = dec.readString();
        int64_t gotPos = dec.readLong();
        float gotYaw = dec.readFloat();
        float gotPitch = dec.readFloat();
        uint32_t gotYawBits, gotPitchBits;
        std::memcpy(&gotYawBits, &gotYaw, 4);
        std::memcpy(&gotPitchBits, &gotPitch, 4);

        bool ok = (gotDim == dim) && (gotPos == posLong) &&
                  (gotYawBits == yawBits) && (gotPitchBits == pitchBits) &&
                  (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name << " dim=" << dim << "/" << gotDim
                      << " pos=" << posLong << "/" << gotPos
                      << " yawBits=" << yawStr << " pitchBits=" << pitchStr
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktSetDefaultSpawnParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
