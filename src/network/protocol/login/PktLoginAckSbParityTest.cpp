// Parity gate for ServerboundLoginAcknowledgedPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktLoginAckSbParity.java ground truth).
//
// The packet's wire form is defined entirely by:
//     STREAM_CODEC = StreamCodec.unit(INSTANCE)
// (net.minecraft.network.protocol.login.ServerboundLoginAcknowledgedPacket line 10).
// StreamCodec.unit (StreamCodec.java 49-63):
//     encode(output, value): writes NOTHING (only asserts value == instance)
//     decode(input):         returns the singleton, reads NOTHING
// So the body is EMPTY: it encodes to zero bytes and decodes by consuming nothing.
// There are no fields to write. The C++ side models this as an empty PacketBuffer
// (no write calls) and must produce zero bytes — matching the unit codec byte-for-byte.
//
//   pkt_login_ack_sb_parity [--cases mcpp/build/pkt_login_ack_sb.tsv]
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
    std::string casesPath = "build/pkt_login_ack_sb.tsv";
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
        // ENC <name> <readableBytes> <hexBytes(may be empty)>
        std::string name, nStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, nStr, '\t')) continue;
        // hexBytes column may be empty (and thus the line ends after the tab).
        if (!std::getline(ss, expHex)) expHex.clear();
        ++cases;

        long expBytes = std::stol(nStr);

        // encode(): StreamCodec.unit writes NOTHING. The C++ model is an empty
        // PacketBuffer with no writes performed.
        PacketBuffer enc;
        std::string got = hex(enc.data());
        long gotBytes = (long)enc.data().size();

        if (got != expHex || gotBytes != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH name=" << name
                      << "\n  got  bytes=" << gotBytes << " hex='" << got << "'"
                      << "\n  want bytes=" << expBytes << " hex='" << expHex << "'\n";
            continue;
        }

        // decode(): the unit codec consumes NOTHING from the input. Decode the
        // expected bytes through PacketBuffer and require nothing is read /
        // remaining matches the empty body (round-trip).
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        if (dec.size() != (size_t)expBytes || dec.remaining() != (size_t)expBytes) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH name=" << name
                      << " size=" << dec.size() << " remaining=" << dec.remaining()
                      << " want=" << expBytes << "\n";
        }
    }

    std::cout << "PktLoginAckSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
