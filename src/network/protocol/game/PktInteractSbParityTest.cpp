// Byte-exact parity for net.minecraft.network.protocol.game.ServerboundInteractPacket
// vs the REAL ServerboundInteractPacket.STREAM_CODEC (tools/PktInteractSbParity.java).
//
// 26.1.2 wire format (verified against 26.1.2/src — ServerboundInteractPacket.java):
//   STREAM_CODEC = StreamCodec.composite(
//       ByteBufCodecs.VAR_INT,         ::entityId,             -> VarInt entity id
//       InteractionHand.STREAM_CODEC,  ::hand,                 -> VarInt hand id (0/1)
//       Vec3.LP_STREAM_CODEC,          ::location,             -> LpVec3-quantized Vec3
//       ByteBufCodecs.BOOL,            ::usingSecondaryAction, -> 1 byte 0/1
//       ::new)
//   InteractionHand.STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, h -> h.id):
//       encode writes VarInt(h.id); MAIN_HAND.id=0, OFF_HAND.id=1
//       (net.minecraft.world.InteractionHand.java:14-15, ByteBufCodecs.java:549-552).
//   Vec3.LP_STREAM_CODEC = StreamCodec.of(LpVec3::write, LpVec3::read) (net.minecraft.network.LpVec3).
//   ByteBufCodecs.BOOL = writeBoolean/readBoolean -> single byte 0x00 / 0x01.
// The composite codec is over plain ByteBuf, so there is NO packet-id prefix: the bytes are just
// the concatenated field bodies, in declaration order.
//
// This test exercises mc::net::PacketBuffer ONLY (no per-packet C++ class/header): it replays the
// EXACT net.minecraft.network.LpVec3.write algorithm here (helpers copied VERBATIM from the
// already-certified mcpp/src/network/LpVec3.h — same jdkRoundD bit-twiddle and JDK clamp form, so
// results are byte-identical to that certified port without dragging in its transitive includes),
// and emits the hand id + secondary flag through PacketBuffer's writeVarInt / writeByte /
// writeInt / writeVarInt / writeBool. It then requires the produced bytes (as hex) AND the byte
// count == the Java ground truth, and decodes the Java bytes back through PacketBuffer requiring
// the VarInt entityId, VarInt handId and BOOL secondary all round-trip (the quantized Vec3 is
// lossy, so full-byte equality + those primitives are the gate).
//
//   pkt_interact_sb_parity [--cases mcpp/build/pkt_interact_sb.tsv]
//
// Rows: ENUM <id> <name> ; ENC <name> <entityId> <handId> <xBits> <yBits> <zBits> <sec> <n> <hex>
#include "../../PacketBuffer.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
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

// InteractionHand id-mapper table: BY_ID/getId index is the enum id (== ordinal here). The wire
// value the idMapper writes is h.id. Index = id -> name, mirroring net.minecraft.world.InteractionHand.
constexpr std::array<std::string_view, 2> kInteractionHand = {"MAIN_HAND", "OFF_HAND"};

// ── LpVec3 constants (verbatim from net.minecraft.network.LpVec3) ──────────────
constexpr double MAX_QUANTIZED_VALUE = 32766.0;
constexpr double ABS_MAX_VALUE       = 1.7179869183E10;
constexpr double ABS_MIN_VALUE       = 3.051944088384301E-5;

// java.lang.Math.round(double) — the exact JDK bit-twiddle (copied verbatim from the certified
// mcpp/src/network/LpVec3.h jdkRoundD). NOT std::round / floor(a+0.5): on a tie those can differ
// from the JDK's result. Argument here is finite, in [0, 32766].
int64_t jdkRoundD(double a) {
    constexpr int     SIGNIFICAND_WIDTH = 53;
    constexpr int     EXP_BIAS          = 1023;
    constexpr int64_t EXP_BIT_MASK      = 0x7FF0000000000000LL;
    constexpr int64_t SIGNIF_BIT_MASK   = 0x000FFFFFFFFFFFFFLL;

    int64_t longBits = (int64_t)std::bit_cast<uint64_t>(a);
    int64_t biasedExp = (longBits & EXP_BIT_MASK) >> (SIGNIFICAND_WIDTH - 1);
    int64_t shift = ((int64_t)SIGNIFICAND_WIDTH - 2 + EXP_BIAS) - biasedExp;
    if ((shift & -64LL) == 0) { // shift >= 0 && shift < 64
        int64_t r = ((longBits & SIGNIF_BIT_MASK) | (SIGNIF_BIT_MASK + 1));
        if (longBits < 0) r = -r;
        return ((r >> shift) + 1) >> 1;
    } else {
        return (int64_t)a; // too large for long, or rounds to 0; finite & in-range here
    }
}

// Mth.ceilLong(double) = (long)Math.ceil(v).
int64_t ceilLong(double v) { return (int64_t)std::ceil(v); }

// Mth.absMax(a, b) = Math.max(Math.abs(a), Math.abs(b)).
double absMax(double a, double b) { return std::fmax(std::fabs(a), std::fabs(b)); }

// LpVec3.sanitize: NaN -> 0, else java.lang.Math.clamp(v, -ABS_MAX, +ABS_MAX) using the
// JDK !(value >= min) form (verbatim from the certified LpVec3.h). +-Inf -> +-ABS_MAX.
double sanitize(double value) {
    if (std::isnan(value)) return 0.0;
    double v = value;
    if (!(v >= -ABS_MAX_VALUE)) v = -ABS_MAX_VALUE;
    if (v > ABS_MAX_VALUE) v = ABS_MAX_VALUE;
    return v;
}

// LpVec3.pack(value) = Math.round((value*0.5 + 0.5) * 32766.0).
int64_t pack(double value) {
    return jdkRoundD((value * 0.5 + 0.5) * MAX_QUANTIZED_VALUE);
}

// Replay net.minecraft.network.LpVec3.write into a PacketBuffer, exactly as the Java does.
void writeLpVec3(PacketBuffer& buf, double vx, double vy, double vz) {
    double x = sanitize(vx);
    double y = sanitize(vy);
    double z = sanitize(vz);
    double chessboardLength = absMax(x, absMax(y, z));
    if (chessboardLength < ABS_MIN_VALUE) {
        buf.writeByte(0);
        return;
    }
    int64_t scale = ceilLong(chessboardLength);
    bool isPartial = (scale & 3LL) != scale;
    int64_t markers = isPartial ? (scale & 3LL | 4LL) : scale; // & binds tighter than |, as in Java
    int64_t xn = pack(x / (double)scale) << 3;
    int64_t yn = pack(y / (double)scale) << 18;
    int64_t zn = pack(z / (double)scale) << 33;
    int64_t buffer = markers | xn | yn | zn;
    buf.writeByte((uint8_t)buffer);             // out.writeByte((byte)buffer)
    buf.writeByte((uint8_t)(buffer >> 8));      // out.writeByte((byte)(buffer >> 8))
    buf.writeInt((int32_t)(buffer >> 16));      // out.writeInt((int)(buffer >> 16))  (BE 4B)
    if (isPartial) {
        buf.writeVarInt((int32_t)(scale >> 2)); // VarInt.write(out, (int)(scale >> 2))
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_interact_sb.tsv";
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

        if (tag == "ENUM") {
            // ENUM <id> <name> — pin our hand id-mapper table against the real enum.
            std::string idStr, nm;
            if (!std::getline(ss, idStr, '\t') || !std::getline(ss, nm)) continue;
            ++cases;
            int32_t hid = (int32_t)std::stoll(idStr);
            if (hid < 0 || hid >= (int32_t)kInteractionHand.size() || kInteractionHand[hid] != nm) {
                ++mismatches;
                std::cerr << "ENUM-MISMATCH id=" << hid << " name=" << nm
                          << " ours=" << (hid >= 0 && hid < (int32_t)kInteractionHand.size()
                                              ? std::string(kInteractionHand[hid]) : "<oob>") << "\n";
            }
            continue;
        }
        if (tag != "ENC") continue;

        std::string name, idStr, handStr, xStr, yStr, zStr, secStr, nStr, expHex;
        if (!std::getline(ss, name, '\t')  || !std::getline(ss, idStr, '\t')  ||
            !std::getline(ss, handStr, '\t')|| !std::getline(ss, xStr, '\t')  ||
            !std::getline(ss, yStr, '\t')   || !std::getline(ss, zStr, '\t')  ||
            !std::getline(ss, secStr, '\t') || !std::getline(ss, nStr, '\t')  ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t id = (int32_t)std::stoll(idStr);
        int32_t handId = (int32_t)std::stoll(handStr);
        double x = bitsToDouble(xStr);
        double y = bitsToDouble(yStr);
        double z = bitsToDouble(zStr);
        bool sec = (std::stoi(secStr) != 0);
        size_t expBytes = (size_t)std::stoul(nStr);

        // (1) ENCODE: VarInt(entityId) + VarInt(handId) + LpVec3(x,y,z) + BOOL(secondary).
        PacketBuffer enc;
        enc.writeVarInt(id);
        enc.writeVarInt(handId);           // idMapper writes VarInt(h.id)
        writeLpVec3(enc, x, y, z);
        enc.writeBool(sec);                // ByteBufCodecs.BOOL -> single byte

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " id=" << id << " hand=" << handId
                      << " sec=" << (int)sec
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE: pull entityId + handId + secondary back out of the Java bytes. The Vec3 is
        // lossy so we skip over its body using the same branch logic, then read the trailing BOOL.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t backId = dec.readVarInt();
        int32_t backHand = dec.readVarInt();
        if (backId != id || backHand != handId) {
            ++mismatches;
            std::cerr << "DECODE-HEAD-MISMATCH " << name << " gotId=" << backId
                      << " gotHand=" << backHand << " wantId=" << id << " wantHand=" << handId << "\n";
            continue;
        }
        // Skip LpVec3 body: 1 byte if first byte == 0, else 6 bytes (+ VarInt if continuation flag).
        uint8_t lowest = dec.readByte();
        if (lowest != 0) {
            dec.readByte();                // middle
            dec.readInt();                 // highest int (BE 4B)
            if ((lowest & 4) == 4) {       // CONTINUATION_FLAG -> trailing VarInt scale bits
                dec.readVarInt();
            }
        }
        bool backSec = dec.readBool();
        if (backSec != sec) {
            ++mismatches;
            std::cerr << "DECODE-SEC-MISMATCH " << name << " got=" << (int)backSec
                      << " want=" << (int)sec << "\n";
            continue;
        }
        if (dec.readPos() != raw.size()) {
            ++mismatches;
            std::cerr << "DECODE-LEN-MISMATCH " << name << " consumed=" << dec.readPos()
                      << " size=" << raw.size() << "\n";
        }
    }

    std::cout << "PktInteractSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
