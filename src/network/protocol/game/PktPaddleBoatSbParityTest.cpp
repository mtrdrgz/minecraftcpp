// Parity gate for ServerboundPaddleBoatPacket's StreamCodec vs the REAL net.minecraft
// codec (tools/PktPaddleBoatSbParity.java ground truth).
//
// The packet has two boolean fields `left` and `right`. Its body is exactly:
//   write : FriendlyByteBuf.writeBoolean(this.left);
//           FriendlyByteBuf.writeBoolean(this.right);
//   read  : this.left  = input.readBoolean();
//           this.right = input.readBoolean();
// (net.minecraft.network.protocol.game.ServerboundPaddleBoatPacket.java lines 20-28.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body, so the whole
// wire payload is two single-byte booleans, left then right (0x01 true / 0x00 false).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeBool(v) writes one byte (v ? 1 : 0) and readBool() returns (byte != 0) — byte-for-
// byte / value-for-value the same as the real codec.
//
//   pkt_paddle_boat_sb_parity [--cases mcpp/build/pkt_paddle_boat_sb.tsv]
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
    std::string casesPath = "build/pkt_paddle_boat_sb.tsv";
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
        if (tag != "ENC") continue;

        // ENC <left-dec(0/1)> <right-dec(0/1)> <readableBytes-dec> <hex>
        std::string leftStr, rightStr, lenStr, expHex;
        if (!std::getline(ss, leftStr, '\t') || !std::getline(ss, rightStr, '\t')
            || !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;
        bool left = std::stoi(leftStr) != 0;
        bool right = std::stoi(rightStr) != 0;
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): writeBoolean(left) then writeBoolean(right) -> two single bytes.
        PacketBuffer enc;
        enc.writeBool(left);
        enc.writeBool(right);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH left=" << left << " right=" << right
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH left=" << left << " right=" << right
                      << " got=" << enc.data().size() << " want=" << expLen << "\n";
        }

        // read(): readBoolean() twice must recover the exact (left,right).
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        bool gotLeft = dec.readBool();
        bool gotRight = dec.readBool();
        if (gotLeft != left || gotRight != right) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex << " got=(" << gotLeft << ","
                      << gotRight << ") want=(" << left << "," << right << ")\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING left=" << left << " right=" << right
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktPaddleBoatSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
