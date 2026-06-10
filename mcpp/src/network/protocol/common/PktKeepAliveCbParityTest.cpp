// Byte-exact parity gate for ClientboundKeepAlivePacket vs the REAL STREAM_CODEC.
//
// Ground truth: tools/PktKeepAliveCbParity.java drives the real net.minecraft
// STREAM_CODEC. Each PKT row is:
//   PKT \t <id-decimal> \t <readableBytes> \t <hex> \t <decodedId-decimal>
//
// The C++ side must:
//   (a) ENCODE the same id via ClientboundKeepAlivePacket::write (PacketBuffer.writeLong)
//       to the identical readableBytes count AND identical bytes, and
//   (b) round-trip DECODE the Java bytes back to the identical id (getId()).
//
//   pkt_keepalive_cb_parity [--cases mcpp/build/pkt_keepalive_cb.tsv]
#include "ClientboundKeepAlivePacket.h"
#include "../../PacketBuffer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::PacketBuffer;
using mc::net::protocol::common::ClientboundKeepAlivePacket;

namespace {
std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> v;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return v;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_keepalive_cb.tsv";
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
        std::string tag, idStr, readableStr, expHex, decIdStr;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "PKT") continue;
        if (!std::getline(ss, idStr, '\t') ||
            !std::getline(ss, readableStr, '\t') ||
            !std::getline(ss, expHex, '\t') ||
            !std::getline(ss, decIdStr)) continue;
        ++cases;

        int64_t id = (int64_t)std::stoll(idStr);
        size_t expReadable = (size_t)std::stoull(readableStr);
        int64_t expDecodedId = (int64_t)std::stoll(decIdStr);

        // (a) ENCODE via the C++ packet
        ClientboundKeepAlivePacket pkt{ id };
        PacketBuffer enc;
        pkt.write(enc);
        std::string got = hex(enc.data());

        if (enc.data().size() != expReadable) {
            ++mismatches;
            std::cerr << "READABLE-MISMATCH id=" << id
                      << " got=" << enc.data().size()
                      << " want=" << expReadable << "\n";
        }
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH id=" << id
                      << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }

        // (b) round-trip DECODE the Java bytes
        PacketBuffer decBuf(unhex(expHex));
        ClientboundKeepAlivePacket dec = ClientboundKeepAlivePacket::read(decBuf);
        if (dec.getId() != expDecodedId) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH want=" << expDecodedId
                      << " got=" << dec.getId() << "\n";
        }
        // and the decoded id must equal the original id (round-trip identity)
        if (dec.getId() != id) {
            ++mismatches;
            std::cerr << "ROUNDTRIP-MISMATCH id=" << id
                      << " got=" << dec.getId() << "\n";
        }
    }

    std::cout << "PktKeepAliveCbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
