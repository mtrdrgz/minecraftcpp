// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundMoveMinecartPacket
// vs the REAL ClientboundMoveMinecartPacket.STREAM_CODEC (tools/PktMoveMinecartParity.java).
//
// 26.1.2 wire format (verified against 26.1.2/src):
//   STREAM_CODEC = StreamCodec.composite(
//       ByteBufCodecs.VAR_INT,                                 ::entityId,  -> VarInt(entityId)
//       NewMinecartBehavior.MinecartStep.STREAM_CODEC.apply(ByteBufCodecs.list()), ::lerpSteps,
//       ::new)
//   ByteBufCodecs.list() == collection(ArrayList::new): VarInt(size) then each element.
//   MinecartStep.STREAM_CODEC = StreamCodec.composite(
//       Vec3.STREAM_CODEC,        ::position,  -> writeDouble x,y,z (BE)
//       Vec3.STREAM_CODEC,        ::movement,  -> writeDouble x,y,z (BE)
//       ByteBufCodecs.ROTATION_BYTE, ::yRot,   -> writeByte(Mth.packDegrees(yRot))
//       ByteBufCodecs.ROTATION_BYTE, ::xRot,   -> writeByte(Mth.packDegrees(xRot))
//       ByteBufCodecs.FLOAT,      ::weight,    -> writeFloat(weight) (IEEE-754 32-bit BE)
//       ::new)
//   Mth.packDegrees(a) = (byte)floor(a * 256.0F / 360.0F)  -- FLOAT multiply, Math.floor(double),
//     narrowing cast to byte (low 8 bits). This is the only non-trivial transform; everything
//     else is a raw write through PacketBuffer.
//
// This test exercises mc::net::PacketBuffer ONLY (no per-packet C++ class/header): it re-encodes
// the packet exactly as the Java codec does, then requires the produced bytes (as hex) AND the
// byte count == the Java ground truth, and decodes the Java bytes back through PacketBuffer
// requiring entityId + step count + the lossless position/movement doubles + weight float
// round-trip (yRot/xRot are lossy single bytes, so full-byte equality is the gate for them).
//
//   pkt_move_minecart_parity [--cases mcpp/build/pkt_move_minecart.tsv]
//
// Row: ENC <name> <entityId-dec> <stepCount-dec>
//      [ per step: <px16> <py16> <pz16> <mx16> <my16> <mz16> <yRot8> <xRot8> <weight8> ]
//      <readableBytes-dec> <hexBytes>
#include "../../PacketBuffer.h"

#include <bit>
#include <cmath>
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

double bitsToDouble(const std::string& hex16) {
    uint64_t bits = std::stoull(hex16, nullptr, 16);
    return std::bit_cast<double>(bits);
}

float bitsToFloat(const std::string& hex8) {
    uint32_t bits = (uint32_t)std::stoul(hex8, nullptr, 16);
    return std::bit_cast<float>(bits);
}

// net.minecraft.util.Mth.packDegrees(float angle) = (byte)floor(angle * 256.0F / 360.0F).
// The multiply is FLOAT precision (Mth.floor takes the float and the multiply is float*float);
// Mth.floor(float) returns an int via (int)Math.floor, but here Java applies Math.floor to the
// float result promoted to double, then narrows to byte. We replicate: float product -> double
// std::floor -> truncate to int64 -> low 8 bits as the byte. std::floor matches Math.floor for
// all finite inputs; the values here are finite.
uint8_t packDegrees(float angle) {
    float product = angle * 256.0f / 360.0f;     // float arithmetic, exactly as Java
    double floored = std::floor((double)product); // Math.floor(double)
    return (uint8_t)(int64_t)floored;             // (byte) narrowing cast = low 8 bits
}

struct Step {
    double px, py, pz, mx, my, mz;
    float yRot, xRot, weight;
};

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_move_minecart.tsv";
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
        std::string tag, name, idStr, cntStr;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, idStr, '\t') ||
            !std::getline(ss, cntStr, '\t')) continue;
        ++cases;

        int32_t entityId = (int32_t)std::stoll(idStr);
        int stepCount = std::stoi(cntStr);

        std::vector<Step> steps;
        steps.reserve(stepCount);
        bool parseOk = true;
        for (int i = 0; i < stepCount; ++i) {
            std::string px, py, pz, mx, my, mz, yr, xr, w;
            if (!std::getline(ss, px, '\t') || !std::getline(ss, py, '\t') ||
                !std::getline(ss, pz, '\t') || !std::getline(ss, mx, '\t') ||
                !std::getline(ss, my, '\t') || !std::getline(ss, mz, '\t') ||
                !std::getline(ss, yr, '\t') || !std::getline(ss, xr, '\t') ||
                !std::getline(ss, w,  '\t')) { parseOk = false; break; }
            Step s;
            s.px = bitsToDouble(px); s.py = bitsToDouble(py); s.pz = bitsToDouble(pz);
            s.mx = bitsToDouble(mx); s.my = bitsToDouble(my); s.mz = bitsToDouble(mz);
            s.yRot = bitsToFloat(yr); s.xRot = bitsToFloat(xr); s.weight = bitsToFloat(w);
            steps.push_back(s);
        }
        if (!parseOk) { ++mismatches; std::cerr << "PARSE-FAIL " << name << "\n"; continue; }

        std::string nStr, expHex;
        if (!std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) {
            ++mismatches; std::cerr << "PARSE-TAIL-FAIL " << name << "\n"; continue;
        }
        size_t expBytes = (size_t)std::stoul(nStr);

        // (1) ENCODE through PacketBuffer exactly as the codec does.
        PacketBuffer enc;
        enc.writeVarInt(entityId);                 // ByteBufCodecs.VAR_INT
        enc.writeVarInt((int32_t)steps.size());    // list() count prefix (VarInt)
        for (const Step& s : steps) {
            enc.writeDouble(s.px); enc.writeDouble(s.py); enc.writeDouble(s.pz); // Vec3 position
            enc.writeDouble(s.mx); enc.writeDouble(s.my); enc.writeDouble(s.mz); // Vec3 movement
            enc.writeByte(packDegrees(s.yRot));    // ROTATION_BYTE yRot
            enc.writeByte(packDegrees(s.xRot));    // ROTATION_BYTE xRot
            enc.writeFloat(s.weight);              // FLOAT weight
        }

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " id=" << entityId
                      << " steps=" << steps.size()
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back; require entityId + count + lossless fields round-trip.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t backId = dec.readVarInt();
        int32_t backCount = dec.readVarInt();
        if (backId != entityId || backCount != (int32_t)steps.size()) {
            ++mismatches;
            std::cerr << "DECODE-HDR-MISMATCH " << name
                      << " id=" << backId << "/" << entityId
                      << " count=" << backCount << "/" << steps.size() << "\n";
            continue;
        }
        bool stepOk = true;
        for (int i = 0; i < backCount && stepOk; ++i) {
            double px = dec.readDouble(), py = dec.readDouble(), pz = dec.readDouble();
            double mx = dec.readDouble(), my = dec.readDouble(), mz = dec.readDouble();
            dec.readByte(); dec.readByte();        // yRot/xRot are lossy; gated by full-byte equality
            float w = dec.readFloat();
            const Step& s = steps[i];
            // raw doubles/float must round-trip bit-for-bit (Vec3/FLOAT are lossless writes).
            if (std::bit_cast<uint64_t>(px) != std::bit_cast<uint64_t>(s.px) ||
                std::bit_cast<uint64_t>(py) != std::bit_cast<uint64_t>(s.py) ||
                std::bit_cast<uint64_t>(pz) != std::bit_cast<uint64_t>(s.pz) ||
                std::bit_cast<uint64_t>(mx) != std::bit_cast<uint64_t>(s.mx) ||
                std::bit_cast<uint64_t>(my) != std::bit_cast<uint64_t>(s.my) ||
                std::bit_cast<uint64_t>(mz) != std::bit_cast<uint64_t>(s.mz) ||
                std::bit_cast<uint32_t>(w)  != std::bit_cast<uint32_t>(s.weight)) {
                stepOk = false;
            }
        }
        if (!stepOk) {
            ++mismatches;
            std::cerr << "DECODE-STEP-MISMATCH " << name << "\n";
            continue;
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DECODE-TRAILING " << name << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktMoveMinecartParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
