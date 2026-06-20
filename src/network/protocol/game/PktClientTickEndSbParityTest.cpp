// Parity gate for ServerboundClientTickEndPacket's StreamCodec vs the REAL net.minecraft
// codec (tools/PktClientTickEndSbParity.java ground truth).
//
// ServerboundClientTickEndPacket is a parameterless record whose codec is:
//   public static final StreamCodec<ByteBuf, ServerboundClientTickEndPacket> STREAM_CODEC =
//       StreamCodec.unit(INSTANCE);
// (net/minecraft/network/protocol/game/ServerboundClientTickEndPacket.java:8-10.)
//
// StreamCodec.unit (StreamCodec.java:49-61):
//   encode(output, value): asserts equality, writes NOTHING.
//   decode(input):         reads NOTHING, returns the singleton instance.
// So the body payload is exactly ZERO bytes (the packet-id prefix is added by the protocol
// bundler, not this body codec). readableBytes == 0, hex == "".
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly: a unit packet
// writes no fields, so the C++ "encode" produces an empty buffer that must match the empty
// expected payload byte-for-byte (and length 0); the "decode" consumes nothing and must
// leave a fully-consumed buffer.
//
//   pkt_client_tick_end_sb_parity [--cases mcpp/build/pkt_client_tick_end_sb.tsv]
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
    std::string casesPath = "build/pkt_client_tick_end_sb.tsv";
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

        // ENC <name> <readableBytes-dec> <hex>
        // <hex> is empty for this unit packet, so getline on the final tab-field may yield an
        // empty string (or fail at EOL); treat a missing/empty trailing field as "".
        std::string name, lenStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, lenStr, '\t')) continue;
        if (!std::getline(ss, expHex)) expHex.clear();  // empty payload -> empty hex
        ++cases;
        size_t expLen = (size_t)std::stoull(lenStr);

        // encode(): a unit packet writes NOTHING. The buffer must stay empty.
        PacketBuffer enc;
        // (no field writes — StreamCodec.unit emits zero bytes)
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH name=" << name << "\n  got  '" << got
                      << "'\n  want '" << expHex << "'\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH name=" << name << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // decode(): consume the (empty) payload — nothing to read — and require the buffer is
        // fully consumed (a unit packet has no trailing bytes).
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        // (no field reads — StreamCodec.unit reads zero bytes)
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING name=" << name << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktClientTickEndSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
