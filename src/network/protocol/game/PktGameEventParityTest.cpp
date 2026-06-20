// Parity gate for net.minecraft.network.protocol.game.ClientboundGameEventPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktGameEventParity.java GT).
//
// The packet body is exactly (ClientboundGameEventPacket.java:46-49 / 41-44):
//   write : FriendlyByteBuf.writeByte(this.event.id)  // low 8 bits of Type id (0..13)
//           FriendlyByteBuf.writeFloat(this.param)     // big-endian IEEE-754 float (4B)
//   read  : this.event = Type.TYPES.get(input.readUnsignedByte()); // 0..255
//           this.param = input.readFloat();
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body (5 bytes).
//
// Both fields are plain numbers, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeByte/readByte and writeFloat/readFloat (BE
// IEEE-754 bit-cast) are byte-for-byte / bit-for-bit identical to the real codec.
// param is carried in the TSV as raw int bits (Float.floatToRawIntBits) so NaN payload
// and -0.0 survive exactly; we bit-cast it to float before writeFloat, mirroring Java.
//
//   pkt_game_event_parity [--cases mcpp/build/pkt_game_event.tsv]
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

float bitsToFloat(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

uint32_t floatToBits(float f) {
    uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_game_event.tsv";
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
            // ENC <eventType> <paramRawBits> <readableBytes> <hexBytes>
            std::string evtStr, bitsStr, nStr, expHex;
            if (!std::getline(ss, evtStr, '\t') || !std::getline(ss, bitsStr, '\t')
                || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t eventType = (int32_t)std::stoll(evtStr);
            // paramRawBits is a Java signed int (Float.floatToRawIntBits); reinterpret.
            uint32_t paramBits = (uint32_t)(int32_t)std::stoll(bitsStr);
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeByte(event.id & 0xff) then writeFloat(param).
            PacketBuffer enc;
            enc.writeByte((uint8_t)(eventType & 0xff));
            enc.writeFloat(bitsToFloat(paramBits));

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH eventType=" << eventType
                          << " paramBits=0x" << std::hex << paramBits << std::dec
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <eventType_in> <paramRawBits_in> <eventType_decoded> <paramRawBits_decoded>
            std::string inHex, evtStr, bitsStr, decEvtStr, decBitsStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, evtStr, '\t')
                || !std::getline(ss, bitsStr, '\t') || !std::getline(ss, decEvtStr, '\t')
                || !std::getline(ss, decBitsStr)) continue;
            ++cases;
            int32_t expEvent = (int32_t)std::stoll(decEvtStr);
            uint32_t expBits = (uint32_t)(int32_t)std::stoll(decBitsStr);

            // read(): readUnsignedByte() (0..255) then readFloat().
            PacketBuffer dec(unhex(inHex));
            int32_t gotEvent = (int32_t)dec.readByte(); // readByte() returns uint8_t 0..255
            uint32_t gotBits = floatToBits(dec.readFloat());
            if (gotEvent != expEvent || gotBits != expBits) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " got eventType=" << gotEvent
                          << " paramBits=0x" << std::hex << gotBits
                          << " want eventType=" << std::dec << expEvent
                          << " paramBits=0x" << std::hex << expBits << std::dec << "\n";
            }
        }
    }

    std::cout << "PktGameEventParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
