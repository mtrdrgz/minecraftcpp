// Parity gate for net.minecraft.network.protocol.game.ClientboundEntityEventPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktEntityEventParity.java GT).
//
// The packet body is exactly (ClientboundEntityEventPacket.java:23-31):
//   write : FriendlyByteBuf.writeInt(this.entityId)  // entity id, BIG-ENDIAN 4 bytes
//           FriendlyByteBuf.writeByte(this.eventId)   // signed byte (low 8 bits)
//   read  : this.entityId = input.readInt();          // BE 4 bytes -> signed int
//           this.eventId  = input.readByte();         // signed byte -128..127
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// Both fields are plain numbers, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeInt/readInt (big-endian 4 bytes) and writeByte/
// readByte are byte-for-byte / value-for-value identical to the real codec. The eventId
// is a Java signed byte; on the wire it is the low 8 bits, and Java's readByte() yields
// a signed value, so we compare the C++ decode as (int8_t)readByte().
//
//   pkt_entity_event_parity [--cases mcpp/build/pkt_entity_event.tsv]
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
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_entity_event.tsv";
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
            // ENC <entityId> <eventId> <readableBytes> <hexBytes>
            std::string idStr, evStr, nStr, expHex;
            if (!std::getline(ss, idStr, '\t') || !std::getline(ss, evStr, '\t')
                || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t entityId = (int32_t)std::stoll(idStr);
            int32_t eventId = (int32_t)std::stoll(evStr); // signed byte value (-128..127)
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeInt(entityId) [BE 4B] then writeByte(eventId & 0xff).
            PacketBuffer enc;
            enc.writeInt(entityId);
            enc.writeByte((uint8_t)(eventId & 0xff));

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH entityId=" << entityId << " eventId=" << eventId
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <entityId_in> <eventId_in> <entityId_decoded> <eventId_decoded>
            std::string inHex, idStr, evStr, decIdStr, decEvStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, idStr, '\t')
                || !std::getline(ss, evStr, '\t') || !std::getline(ss, decIdStr, '\t')
                || !std::getline(ss, decEvStr)) continue;
            ++cases;
            int32_t expId = (int32_t)std::stoll(decIdStr);
            int32_t expEvent = (int32_t)std::stoll(decEvStr); // signed byte value

            // read(): readInt() [BE 4B] then readByte() interpreted SIGNED (Java byte).
            PacketBuffer dec(unhex(inHex));
            int32_t gotId = dec.readInt();
            int32_t gotEvent = (int32_t)(int8_t)dec.readByte(); // signed -128..127
            if (gotId != expId || gotEvent != expEvent) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " got entityId=" << gotId << " eventId=" << gotEvent
                          << " want entityId=" << expId << " eventId=" << expEvent << "\n";
            }
        }
    }

    std::cout << "PktEntityEventParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
