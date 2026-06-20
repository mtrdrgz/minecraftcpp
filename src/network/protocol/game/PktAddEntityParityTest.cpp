// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundAddEntityPacket
// vs the REAL ClientboundAddEntityPacket.STREAM_CODEC (tools/PktAddEntityParity.java).
//
// 26.1.2 wire body, field-by-field in codec order (ClientboundAddEntityPacket.java:109-121):
//   writeVarInt(id)
//   writeUUID(uuid)                         -> writeLong(MSB), writeLong(LSB)  (BE)
//   ByteBufCodecs.registry(ENTITY_TYPE)     -> VarInt(BuiltInRegistries.ENTITY_TYPE.getId(type))  PLAIN id (no holder/+1)
//   writeDouble(x), writeDouble(y), writeDouble(z)   (BE)
//   Vec3.LP_STREAM_CODEC.encode(movement)   -> net.minecraft.network.LpVec3.write (quantized velocity)
//   writeByte(xRot), writeByte(yRot), writeByte(yHeadRot)
//       where each byte = Mth.packDegrees(deg) = (byte)(int)floor(deg * 256.0F / 360.0F)  (ctor, Mth.java:181-183)
//   writeVarInt(data)
//
// The registry-held EntityType field is resolved name(ns:path) -> wire id via
// mc::net::NetworkRegistries (assets/network_registries.tsv). registry(R) is the PLAIN
// VarInt(getId) form (ByteBufCodecs.java:560-582) -- NOT holder() -- so NO +1 is applied.
//
// This test exercises mc::net::PacketBuffer + NetworkRegistries ONLY (no per-packet C++
// class/header). The LpVec3.write algorithm is replayed here through PacketBuffer's
// writeByte/writeInt/writeVarInt (helpers verbatim from the certified mcpp/src/network/LpVec3.h
// jdkRoundD + JDK clamp form, so byte-identical to that gated port without its includes).
//
//   pkt_add_entity_parity [--cases mcpp/build/pkt_add_entity.tsv]
//
// Row: ENC <name> <id> <uuidHi-016x> <uuidLo-016x> <typeName> <typeId> <x><y><z-016x>
//          <vx><vy><vz-016x> <xRot><yRot><yHeadRot-08x> <data> <readableBytes> <hex>
#include "../../NetworkRegistries.h"
#include "../../PacketBuffer.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::NetworkRegistries;
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

double bitsToDouble(const std::string& h) { return std::bit_cast<double>((uint64_t)std::stoull(h, nullptr, 16)); }
float  bitsToFloat (const std::string& h) { return std::bit_cast<float>((uint32_t)std::stoul(h, nullptr, 16)); }

// ── LpVec3 (verbatim from the certified net.minecraft.network.LpVec3 / mcpp LpVec3.h) ──
constexpr double MAX_QUANTIZED_VALUE = 32766.0;
constexpr double ABS_MAX_VALUE       = 1.7179869183E10;
constexpr double ABS_MIN_VALUE       = 3.051944088384301E-5;

// java.lang.Math.round(double) — the exact JDK bit-twiddle (NOT std::round/floor(a+0.5):
// on a tie those can differ). Argument here is finite, in [0, 32766].
int64_t jdkRoundD(double a) {
    constexpr int     SIGNIFICAND_WIDTH = 53;
    constexpr int     EXP_BIAS          = 1023;
    constexpr int64_t EXP_BIT_MASK      = 0x7FF0000000000000LL;
    constexpr int64_t SIGNIF_BIT_MASK   = 0x000FFFFFFFFFFFFFLL;
    int64_t longBits  = (int64_t)std::bit_cast<uint64_t>(a);
    int64_t biasedExp = (longBits & EXP_BIT_MASK) >> (SIGNIFICAND_WIDTH - 1);
    int64_t shift     = ((int64_t)SIGNIFICAND_WIDTH - 2 + EXP_BIAS) - biasedExp;
    if ((shift & -64LL) == 0) { // shift >= 0 && shift < 64
        int64_t r = ((longBits & SIGNIF_BIT_MASK) | (SIGNIF_BIT_MASK + 1));
        if (longBits < 0) r = -r;
        return ((r >> shift) + 1) >> 1;
    }
    return (int64_t)a;
}
int64_t ceilLong(double v) { return (int64_t)std::ceil(v); }          // Mth.ceilLong
double  absMax(double a, double b) { return std::fmax(std::fabs(a), std::fabs(b)); } // Mth.absMax
double  sanitize(double value) {                                     // LpVec3.sanitize
    if (std::isnan(value)) return 0.0;
    double v = value;
    if (!(v >= -ABS_MAX_VALUE)) v = -ABS_MAX_VALUE;
    if (v > ABS_MAX_VALUE) v = ABS_MAX_VALUE;
    return v;
}
int64_t pack(double value) { return jdkRoundD((value * 0.5 + 0.5) * MAX_QUANTIZED_VALUE); } // LpVec3.pack

// Replay net.minecraft.network.LpVec3.write into a PacketBuffer, exactly as the Java does.
void writeLpVec3(PacketBuffer& buf, double vx, double vy, double vz) {
    double x = sanitize(vx), y = sanitize(vy), z = sanitize(vz);
    double chessboardLength = absMax(x, absMax(y, z));
    if (chessboardLength < ABS_MIN_VALUE) { buf.writeByte(0); return; }
    int64_t scale    = ceilLong(chessboardLength);
    bool    isPartial = (scale & 3LL) != scale;
    int64_t markers  = isPartial ? (scale & 3LL | 4LL) : scale; // & binds tighter than |, as in Java
    int64_t xn = pack(x / (double)scale) << 3;
    int64_t yn = pack(y / (double)scale) << 18;
    int64_t zn = pack(z / (double)scale) << 33;
    int64_t buffer = markers | xn | yn | zn;
    buf.writeByte((uint8_t)buffer);
    buf.writeByte((uint8_t)(buffer >> 8));
    buf.writeInt((int32_t)(buffer >> 16));
    if (isPartial) buf.writeVarInt((int32_t)(scale >> 2));
}

// Mth.packDegrees(float angle) = (byte)Mth.floor(angle * 256.0F / 360.0F)
//                              = (byte)(int)Math.floor(angle * 256.0F / 360.0F)  (float arithmetic).
uint8_t packDegrees(float angle) {
    float scaled = angle * 256.0f / 360.0f;                  // single-precision, as Java
    int32_t i = (int32_t)std::floor((double)scaled);         // Mth.floor(float) = (int)Math.floor(v)
    return (uint8_t)(int8_t)i;                               // (byte) narrowing cast
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_add_entity.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    NetworkRegistries reg;
    if (!reg.loadFromFile("src/assets/network_registries.tsv")) {
        std::cerr << "FATAL: cannot load mcpp/src/assets/network_registries.tsv (run from repo root)\n";
        return 2;
    }
    if (!reg.loadOrderOk()) { std::cerr << "FATAL: network_registries.tsv not dense/in-order\n"; return 2; }

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    const std::string REG = "minecraft:entity_type";
    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string tag, name, idStr, hiStr, loStr, typeName, typeIdStr,
                    xS, yS, zS, vxS, vyS, vzS, xrS, yrS, yhrS, dataStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t')      || !std::getline(ss, idStr, '\t')   ||
            !std::getline(ss, hiStr, '\t')      || !std::getline(ss, loStr, '\t')   ||
            !std::getline(ss, typeName, '\t')   || !std::getline(ss, typeIdStr, '\t') ||
            !std::getline(ss, xS, '\t')         || !std::getline(ss, yS, '\t')      ||
            !std::getline(ss, zS, '\t')         || !std::getline(ss, vxS, '\t')     ||
            !std::getline(ss, vyS, '\t')        || !std::getline(ss, vzS, '\t')     ||
            !std::getline(ss, xrS, '\t')        || !std::getline(ss, yrS, '\t')     ||
            !std::getline(ss, yhrS, '\t')       || !std::getline(ss, dataStr, '\t') ||
            !std::getline(ss, nStr, '\t')       || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t id   = (int32_t)std::stoll(idStr);
        uint64_t hi  = (uint64_t)std::stoull(hiStr, nullptr, 16);
        uint64_t lo  = (uint64_t)std::stoull(loStr, nullptr, 16);
        int32_t expTypeId = (int32_t)std::stoll(typeIdStr);
        double x  = bitsToDouble(xS),  y  = bitsToDouble(yS),  z  = bitsToDouble(zS);
        double vx = bitsToDouble(vxS), vy = bitsToDouble(vyS), vz = bitsToDouble(vzS);
        float  xRot = bitsToFloat(xrS), yRot = bitsToFloat(yrS), yHeadRot = bitsToFloat(yhrS);
        int32_t data = (int32_t)std::stoll(dataStr);
        size_t expBytes = (size_t)std::stoul(nStr);

        // Resolve the registry-held EntityType name -> wire id (FAIL the gate loudly if absent).
        auto typeId = reg.id(REG, typeName);
        if (!typeId) {
            ++mismatches;
            std::cerr << "REGISTRY-MISS " << name << " entity_type=" << typeName << " not in network_registries.tsv\n";
            continue;
        }
        if (*typeId != expTypeId) {
            ++mismatches;
            std::cerr << "REGISTRY-ID-MISMATCH " << name << " " << typeName
                      << " resolved=" << *typeId << " want=" << expTypeId << "\n";
            continue;
        }

        // (1) ENCODE in codec order via PacketBuffer; compare bytes + count.
        PacketBuffer enc;
        enc.writeVarInt(id);
        enc.writeUUID(hi, lo);
        enc.writeVarInt(*typeId);          // registry(ENTITY_TYPE) = PLAIN VarInt(getId), no +1
        enc.writeDouble(x);
        enc.writeDouble(y);
        enc.writeDouble(z);
        writeLpVec3(enc, vx, vy, vz);
        enc.writeByte(packDegrees(xRot));
        enc.writeByte(packDegrees(yRot));
        enc.writeByte(packDegrees(yHeadRot));
        enc.writeVarInt(data);

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " type=" << typeName << " id=" << id
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes: pull id, uuid, type-id (then back to name), data; require round-trip.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t backId = dec.readVarInt();
        uint64_t backHi, backLo; dec.readUUID(backHi, backLo);
        int32_t backTypeId = dec.readVarInt();
        (void)dec.readDouble(); (void)dec.readDouble(); (void)dec.readDouble();
        // skip the variable-length LpVec3 movement: 1 byte (0) or 6 bytes + optional continuation VarInt.
        {
            uint8_t lowest = dec.readByte();
            if (lowest != 0) {
                (void)dec.readByte();          // middle
                (void)dec.readInt();           // highest (4 bytes)
                if ((lowest & 4) == 4) (void)dec.readVarInt(); // continuation scale bits
            }
        }
        (void)dec.readByte(); (void)dec.readByte(); (void)dec.readByte(); // xRot,yRot,yHeadRot
        int32_t backData = dec.readVarInt();

        auto backName = reg.name(REG, backTypeId);
        if (backId != id || backHi != hi || backLo != lo || backTypeId != *typeId ||
            backData != data || !backName || *backName != typeName) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " id(" << backId << "/" << id << ")"
                      << " typeId(" << backTypeId << "/" << *typeId << ")"
                      << " data(" << backData << "/" << data << ")"
                      << " name(" << (backName ? *backName : std::string("<none>")) << "/" << typeName << ")\n";
            continue;
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DECODE-TRAILING " << name << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktAddEntityParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
