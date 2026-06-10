// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundPlayerLookAtPacket
// vs the REAL packet write/read (tools/PktPlayerLookAtParity.java).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src):
//   private void write(FriendlyByteBuf output) {
//       output.writeEnum(this.fromAnchor);   -> VarInt(ordinal)   (FEET=0, EYES=1)
//       output.writeDouble(this.x);          -> double x  (8B, BE)
//       output.writeDouble(this.y);          -> double y  (8B, BE)
//       output.writeDouble(this.z);          -> double z  (8B, BE)
//       output.writeBoolean(this.atEntity);  -> byte 0/1
//       if (this.atEntity) {
//           output.writeVarInt(this.entity);   -> VarInt entityId
//           output.writeEnum(this.toAnchor);   -> VarInt(ordinal)
//       }
//   }
//   FriendlyByteBuf.writeEnum == writeVarInt(ordinal). EntityAnchorArgument.Anchor: FEET=0, EYES=1.
//
// Full payload order:
//   VarInt(fromOrd) | x y z (double,BE) | atEntity (byte) [ if atEntity: VarInt(entity) | VarInt(toOrd) ]
//
// This test exercises mc::net::PacketBuffer ONLY (no per-packet C++ class/header). It reconstructs
// the exact doubles from their raw bits and writes the fields through PacketBuffer in the EXACT
// codec order, requires the produced bytes (as hex) AND the byte count == the Java ground truth,
// then decodes the Java bytes back through PacketBuffer and requires every field round-trips.
//
//   pkt_player_look_at_parity [--cases mcpp/build/pkt_player_look_at.tsv]
//
// Row: ENC <name> <fromOrd-dec> <xBits-016x> <yBits-016x> <zBits-016x>
//      <atEntity-dec> <entity-dec> <toOrd-dec> <readableBytes-dec> <hexBytes>
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

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_player_look_at.tsv";
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
        std::string tag, name, fromStr, xStr, yStr, zStr, atStr, entStr, toStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t')   || !std::getline(ss, fromStr, '\t') ||
            !std::getline(ss, xStr, '\t')    || !std::getline(ss, yStr, '\t')   ||
            !std::getline(ss, zStr, '\t')    || !std::getline(ss, atStr, '\t')  ||
            !std::getline(ss, entStr, '\t')  || !std::getline(ss, toStr, '\t')  ||
            !std::getline(ss, nStr, '\t')    || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t fromOrd  = (int32_t)std::stol(fromStr);
        double  x        = bitsToDouble(xStr);
        double  y        = bitsToDouble(yStr);
        double  z        = bitsToDouble(zStr);
        bool    atEntity = (std::stoi(atStr) != 0);
        int32_t entity   = (int32_t)std::stoll(entStr);
        int32_t toOrd    = (int32_t)std::stol(toStr);
        size_t  expBytes = (size_t)std::stoul(nStr);

        // (1) ENCODE through PacketBuffer in the EXACT codec field order.
        //     writeEnum == writeVarInt(ordinal); writeBoolean == 1 byte; conditional tail.
        PacketBuffer enc;
        enc.writeVarInt(fromOrd);
        enc.writeDouble(x);
        enc.writeDouble(y);
        enc.writeDouble(z);
        enc.writeBool(atEntity);
        if (atEntity) {
            enc.writeVarInt(entity);
            enc.writeVarInt(toOrd);
        }

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " atEntity=" << atEntity
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back through PacketBuffer; require every field round-trips
        //     bit-for-bit and that exactly expBytes are consumed.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t bFrom = dec.readVarInt();
        double  bx    = dec.readDouble();
        double  by    = dec.readDouble();
        double  bz    = dec.readDouble();
        bool    bat   = dec.readBool();
        int32_t bent  = 0, bto = 0;
        if (bat) {
            bent = dec.readVarInt();
            bto  = dec.readVarInt();
        }

        bool ok =
            bFrom == fromOrd &&
            std::bit_cast<uint64_t>(bx) == std::bit_cast<uint64_t>(x) &&
            std::bit_cast<uint64_t>(by) == std::bit_cast<uint64_t>(y) &&
            std::bit_cast<uint64_t>(bz) == std::bit_cast<uint64_t>(z) &&
            bat == atEntity;
        if (ok && atEntity) {
            ok = (bent == entity) && (bto == toOrd);
        }

        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " from(got=" << bFrom << " want=" << fromOrd << ")"
                      << " at(got=" << bat << " want=" << atEntity << ")"
                      << " ent(got=" << bent << " want=" << entity << ")\n";
            continue;
        }
        if (dec.remaining() != 0 || raw.size() != enc.data().size()) {
            ++mismatches;
            std::cerr << "DECODE-LEN-MISMATCH " << name
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktPlayerLookAtParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
