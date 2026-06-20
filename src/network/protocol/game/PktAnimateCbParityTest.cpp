// Parity gate for net.minecraft.network.protocol.game.ClientboundAnimatePacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktAnimateCbParity.java GT).
//
// The packet body is exactly (ClientboundAnimatePacket.java:31-34 / 26-29):
//   write : FriendlyByteBuf.writeVarInt(this.id)    // entity id (VarInt / LEB128)
//           FriendlyByteBuf.writeByte(this.action)  // low 8 bits of the int action
//   read  : this.id     = input.readVarInt();
//           this.action = input.readUnsignedByte(); // 0..255
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// Both fields are plain numbers, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeVarInt/readVarInt (LEB128) and writeByte/
// readByte are byte-for-byte / value-for-value identical to the real codec.
//
//   pkt_animate_cb_parity [--cases mcpp/build/pkt_animate_cb.tsv]
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
    std::string casesPath = "build/pkt_animate_cb.tsv";
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
            // ENC <id> <action_in> <readableBytes> <hexBytes>
            std::string idStr, actStr, nStr, expHex;
            if (!std::getline(ss, idStr, '\t') || !std::getline(ss, actStr, '\t')
                || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t id = (int32_t)std::stoll(idStr);
            int32_t action = (int32_t)std::stoll(actStr);
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeVarInt(id) then writeByte(action & 0xff).
            PacketBuffer enc;
            enc.writeVarInt(id);
            enc.writeByte((uint8_t)(action & 0xff));

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH id=" << id << " action=" << action
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <id_in> <action_in> <id_decoded> <action_decoded>
            std::string inHex, idStr, actStr, decIdStr, decActStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, idStr, '\t')
                || !std::getline(ss, actStr, '\t') || !std::getline(ss, decIdStr, '\t')
                || !std::getline(ss, decActStr)) continue;
            ++cases;
            int32_t expId = (int32_t)std::stoll(decIdStr);
            int32_t expAction = (int32_t)std::stoll(decActStr);

            // read(): readVarInt() then readUnsignedByte() (0..255).
            PacketBuffer dec(unhex(inHex));
            int32_t gotId = dec.readVarInt();
            int32_t gotAction = (int32_t)dec.readByte(); // readByte() returns uint8_t 0..255
            if (gotId != expId || gotAction != expAction) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " got id=" << gotId << " action=" << gotAction
                          << " want id=" << expId << " action=" << expAction << "\n";
            }
        }
    }

    std::cout << "PktAnimateCbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
