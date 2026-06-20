// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundMoveEntityPacket.Pos
// vs the REAL Pos.STREAM_CODEC (tools/PktMoveEntityPosParity.java ground truth).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src ClientboundMoveEntityPacket.java —
// nested static class Pos, its private write/read reached via Pos.STREAM_CODEC =
// Packet.codec(Pos::write, Pos::read)):
//
//   Pos.write(output):
//       output.writeVarInt(this.entityId);   // VarInt(entityId)  (plain LEB128, NOT zig-zag)
//       output.writeShort(this.xa);          // BE 2-byte short
//       output.writeShort(this.ya);          // BE 2-byte short
//       output.writeShort(this.za);          // BE 2-byte short
//       output.writeBoolean(this.onGround);  // 1 byte 0/1
//   Pos.read(input):
//       readVarInt(); readShort()*3 (SIGNED); readBoolean().
//   Packet.codec -> StreamCodec.ofMember: body only, no packet-id / length prefix.
//
// Every field is a plain primitive PacketBuffer supports, so this reuses the certified
// PacketBuffer (the FriendlyByteBuf port) directly: writeVarInt(entityId) + writeShort(xa) +
// writeShort(ya) + writeShort(za) + writeBool(onGround) is byte-for-byte the real codec, and
// readVarInt()/readShort()/readBool() round-trips every field. No new per-packet C++ class/header.
//
//   pkt_move_entity_pos_parity [--cases mcpp/build/pkt_move_entity_pos.tsv]
//
// Row: ENC <name> <id-dec> <xa-dec> <ya-dec> <za-dec> <onGround-0/1> <readableBytes-dec> <hexBytes>
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
    std::string casesPath = "build/pkt_move_entity_pos.tsv";
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
        std::string tag, name, idStr, xaStr, yaStr, zaStr, ogStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, idStr, '\t') ||
            !std::getline(ss, xaStr, '\t') || !std::getline(ss, yaStr, '\t') ||
            !std::getline(ss, zaStr, '\t') || !std::getline(ss, ogStr, '\t') ||
            !std::getline(ss, nStr, '\t')  || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t id  = (int32_t)std::stoll(idStr);
        int16_t xa  = (int16_t)std::stoll(xaStr);   // Java short, signed
        int16_t ya  = (int16_t)std::stoll(yaStr);
        int16_t za  = (int16_t)std::stoll(zaStr);
        bool onGround = (std::stoi(ogStr) != 0);
        size_t expBytes = (size_t)std::stoul(nStr);

        // (1) ENCODE: replay the exact Pos.write codec order through PacketBuffer.
        PacketBuffer enc;
        enc.writeVarInt(id);
        enc.writeShort(xa);
        enc.writeShort(ya);
        enc.writeShort(za);
        enc.writeBool(onGround);

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " id=" << id
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE: read the Java bytes back through PacketBuffer; require every field round-trips
        //     and the buffer is fully consumed.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t backId   = dec.readVarInt();
        int16_t backXa   = dec.readShort();
        int16_t backYa   = dec.readShort();
        int16_t backZa   = dec.readShort();
        bool    backOg   = dec.readBool();

        if (backId != id || backXa != xa || backYa != ya || backZa != za || backOg != onGround) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " id(" << backId << "/" << id << ")"
                      << " xa(" << backXa << "/" << xa << ")"
                      << " ya(" << backYa << "/" << ya << ")"
                      << " za(" << backZa << "/" << za << ")"
                      << " og(" << backOg << "/" << onGround << ")\n";
            continue;
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DECODE-TRAILING " << name << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktMoveEntityPosParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
