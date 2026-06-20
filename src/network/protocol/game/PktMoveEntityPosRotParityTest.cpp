// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundMoveEntityPacket.PosRot
// vs the REAL ClientboundMoveEntityPacket.PosRot.STREAM_CODEC (tools/PktMoveEntityPosRotParity.java).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src
// net/minecraft/network/protocol/game/ClientboundMoveEntityPacket.java, PosRot::write):
//   output.writeVarInt(entityId);   // VarInt id
//   output.writeShort(xa);          // BE 2-byte signed short
//   output.writeShort(ya);          // BE 2-byte signed short
//   output.writeShort(za);          // BE 2-byte signed short
//   output.writeByte(yRot);         // 1 signed byte
//   output.writeByte(xRot);         // 1 signed byte
//   output.writeBoolean(onGround);  // 1 byte 0/1
//
// This test exercises mc::net::PacketBuffer ONLY (no per-packet C++ class/header): it replays the
// codec field-by-field through PacketBuffer's writeVarInt / writeShort / writeByte / writeBool in the
// EXACT codec order, requires the produced bytes (as hex) AND the byte count == the Java ground truth,
// then decodes the Java bytes back through PacketBuffer (readVarInt / readShort / readByte / readBool)
// and requires every field round-trips exactly.
//
//   pkt_move_entity_posrot_parity [--cases mcpp/build/pkt_move_entity_posrot.tsv]
//
// Row: ENC <name> <id-dec> <xa-dec> <ya-dec> <za-dec> <yRot-dec> <xRot-dec> <onGround-0|1>
//      <readableBytes-dec> <hexBytes>
#include "../../PacketBuffer.h"

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

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_move_entity_posrot.tsv";
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
        std::string tag, name, idStr, xaStr, yaStr, zaStr, yrStr, xrStr, ogStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t')  || !std::getline(ss, idStr, '\t') ||
            !std::getline(ss, xaStr, '\t') || !std::getline(ss, yaStr, '\t') ||
            !std::getline(ss, zaStr, '\t') || !std::getline(ss, yrStr, '\t') ||
            !std::getline(ss, xrStr, '\t') || !std::getline(ss, ogStr, '\t') ||
            !std::getline(ss, nStr, '\t')  || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t id  = (int32_t)std::stoll(idStr);
        int16_t xa  = (int16_t)std::stoi(xaStr);
        int16_t ya  = (int16_t)std::stoi(yaStr);
        int16_t za  = (int16_t)std::stoi(zaStr);
        int8_t  yr  = (int8_t)std::stoi(yrStr);
        int8_t  xr  = (int8_t)std::stoi(xrStr);
        bool    og  = (std::stoi(ogStr) != 0);
        size_t  expBytes = (size_t)std::stoul(nStr);

        // (1) ENCODE field-by-field in codec order; compare bytes + count.
        PacketBuffer enc;
        enc.writeVarInt(id);
        enc.writeShort(xa);
        enc.writeShort(ya);
        enc.writeShort(za);
        enc.writeByte((uint8_t)yr);
        enc.writeByte((uint8_t)xr);
        enc.writeBool(og);

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " id=" << id
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back through PacketBuffer; require every field round-trips.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t backId = dec.readVarInt();
        int16_t backXa = dec.readShort();
        int16_t backYa = dec.readShort();
        int16_t backZa = dec.readShort();
        int8_t  backYr = (int8_t)dec.readByte();
        int8_t  backXr = (int8_t)dec.readByte();
        bool    backOg = dec.readBool();

        if (backId != id || backXa != xa || backYa != ya || backZa != za ||
            backYr != yr || backXr != xr || backOg != og) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " id=" << backId << "/" << id
                      << " xa=" << backXa << "/" << xa
                      << " ya=" << backYa << "/" << ya
                      << " za=" << backZa << "/" << za
                      << " yr=" << (int)backYr << "/" << (int)yr
                      << " xr=" << (int)backXr << "/" << (int)xr
                      << " og=" << backOg << "/" << og << "\n";
            continue;
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DECODE-TRAILING " << name << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktMoveEntityPosRotParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
