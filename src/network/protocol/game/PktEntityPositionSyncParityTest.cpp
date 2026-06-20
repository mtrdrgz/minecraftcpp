// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundEntityPositionSyncPacket
// vs the REAL ClientboundEntityPositionSyncPacket.STREAM_CODEC (tools/PktEntityPositionSyncParity.java).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src):
//   ClientboundEntityPositionSyncPacket.STREAM_CODEC = StreamCodec.composite(
//       ByteBufCodecs.VAR_INT,             ::id,       -> VarInt(id)
//       PositionMoveRotation.STREAM_CODEC, ::values,   -> nested record
//       ByteBufCodecs.BOOL,                ::onGround, -> 1 byte
//       ::new)
//   PositionMoveRotation.STREAM_CODEC = StreamCodec.composite(
//       Vec3.STREAM_CODEC,   ::position,      -> double x,y,z       (BE, via writeDouble x3)
//       Vec3.STREAM_CODEC,   ::deltaMovement, -> double vx,vy,vz    (BE, via writeDouble x3)
//       ByteBufCodecs.FLOAT, ::yRot,          -> float yRot         (BE)
//       ByteBufCodecs.FLOAT, ::xRot,          -> float xRot         (BE)
//       ::new)
//   Vec3.STREAM_CODEC.encode = writeDouble(x); writeDouble(y); writeDouble(z).
//
// Full payload order:
//   VarInt(id) | px py pz (double,BE) | vx vy vz (double,BE) | yRot xRot (float,BE) | onGround (byte)
//
// This test exercises mc::net::PacketBuffer ONLY (no per-packet C++ class/header). It reconstructs
// the exact doubles/floats from their raw bits, writes them through PacketBuffer in the EXACT codec
// order (writeVarInt / writeDouble / writeDouble / writeFloat / writeFloat / writeBool), requires the
// produced bytes (as hex) AND the byte count == the Java ground truth, then decodes the Java bytes
// back through PacketBuffer and requires every field round-trips bit-for-bit.
//
//   pkt_entity_position_sync_parity [--cases mcpp/build/pkt_entity_position_sync.tsv]
//
// Row: ENC <name> <id-dec> <pxBits-016x> <pyBits-016x> <pzBits-016x>
//      <vxBits-016x> <vyBits-016x> <vzBits-016x> <yRotBits-08x> <xRotBits-08x>
//      <onGround-dec> <readableBytes-dec> <hexBytes>
#include "../../PacketBuffer.h"

#include <bit>
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

double bitsToDouble(const std::string& h) {
    return std::bit_cast<double>((uint64_t)std::stoull(h, nullptr, 16));
}

float bitsToFloat(const std::string& h) {
    return std::bit_cast<float>((uint32_t)std::stoul(h, nullptr, 16));
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_entity_position_sync.tsv";
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
        std::string tag, name, idStr, pxStr, pyStr, pzStr, vxStr, vyStr, vzStr,
                    yrStr, xrStr, ogStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, idStr, '\t') ||
            !std::getline(ss, pxStr, '\t') || !std::getline(ss, pyStr, '\t') ||
            !std::getline(ss, pzStr, '\t') || !std::getline(ss, vxStr, '\t') ||
            !std::getline(ss, vyStr, '\t') || !std::getline(ss, vzStr, '\t') ||
            !std::getline(ss, yrStr, '\t') || !std::getline(ss, xrStr, '\t') ||
            !std::getline(ss, ogStr, '\t') || !std::getline(ss, nStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t id      = (int32_t)std::stoll(idStr);
        double  px      = bitsToDouble(pxStr);
        double  py      = bitsToDouble(pyStr);
        double  pz      = bitsToDouble(pzStr);
        double  vx      = bitsToDouble(vxStr);
        double  vy      = bitsToDouble(vyStr);
        double  vz      = bitsToDouble(vzStr);
        float   yRot    = bitsToFloat(yrStr);
        float   xRot    = bitsToFloat(xrStr);
        bool    onGround = (std::stoi(ogStr) != 0);
        size_t  expBytes = (size_t)std::stoul(nStr);

        // (1) ENCODE through PacketBuffer in the EXACT codec field order.
        PacketBuffer enc;
        enc.writeVarInt(id);
        // PositionMoveRotation: position (Vec3), deltaMovement (Vec3), yRot, xRot.
        enc.writeDouble(px); enc.writeDouble(py); enc.writeDouble(pz);
        enc.writeDouble(vx); enc.writeDouble(vy); enc.writeDouble(vz);
        enc.writeFloat(yRot);
        enc.writeFloat(xRot);
        enc.writeBool(onGround);

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " id=" << id
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back through PacketBuffer; require every field round-trips
        //     bit-for-bit and that exactly expBytes are consumed.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t bId  = dec.readVarInt();
        double  bpx  = dec.readDouble();
        double  bpy  = dec.readDouble();
        double  bpz  = dec.readDouble();
        double  bvx  = dec.readDouble();
        double  bvy  = dec.readDouble();
        double  bvz  = dec.readDouble();
        float   byr  = dec.readFloat();
        float   bxr  = dec.readFloat();
        bool    bog  = dec.readBool();

        bool ok =
            bId == id &&
            std::bit_cast<uint64_t>(bpx) == std::bit_cast<uint64_t>(px) &&
            std::bit_cast<uint64_t>(bpy) == std::bit_cast<uint64_t>(py) &&
            std::bit_cast<uint64_t>(bpz) == std::bit_cast<uint64_t>(pz) &&
            std::bit_cast<uint64_t>(bvx) == std::bit_cast<uint64_t>(vx) &&
            std::bit_cast<uint64_t>(bvy) == std::bit_cast<uint64_t>(vy) &&
            std::bit_cast<uint64_t>(bvz) == std::bit_cast<uint64_t>(vz) &&
            std::bit_cast<uint32_t>(byr) == std::bit_cast<uint32_t>(yRot) &&
            std::bit_cast<uint32_t>(bxr) == std::bit_cast<uint32_t>(xRot) &&
            bog == onGround;

        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name << " id(got=" << bId << " want=" << id << ")\n";
            continue;
        }
        if (dec.remaining() != 0 || raw.size() != enc.data().size()) {
            ++mismatches;
            std::cerr << "DECODE-LEN-MISMATCH " << name
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktEntityPositionSyncParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
