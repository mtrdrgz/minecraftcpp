// Byte-exact parity for net.minecraft.network.protocol.common.ClientboundPingPacket.
//
// Ground truth: tools/PktPingCbParity.java encodes each id through the REAL
// STREAM_CODEC into a FriendlyByteBuf and dumps readableBytes + body hex.
// Here the C++ ClientboundPingPacket codec (writeInt over PacketBuffer) must
// (a) produce the identical bytes and (b) decode those bytes back to the id.
//
//   PING_CB <id> <readableBytes> <hex>
//
//   pkt_ping_cb_parity [--cases mcpp/build/pkt_ping_cb.tsv]
#include "ClientboundPingPacket.h"
#include "../../PacketBuffer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::PacketBuffer;
using mc::net::protocol::common::ClientboundPingPacket;

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
    std::string casesPath = "mcpp/build/pkt_ping_cb.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string tag, idStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t') || !std::getline(ss, idStr, '\t')
            || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
        if (tag != "PING_CB") continue;
        ++cases;

        int32_t id = (int32_t)std::stoll(idStr);
        size_t expN = (size_t)std::stoull(nStr);

        // (a) encode via the real codec path
        PacketBuffer enc;
        ClientboundPingPacket(id).write(enc);
        std::string got = hex(enc.data());

        bool ok = true;
        if (enc.data().size() != expN) {
            ok = false;
            std::cerr << "LEN-MISMATCH id=" << id << " got=" << enc.data().size()
                      << " want=" << expN << "\n";
        }
        if (got != expHex) {
            ok = false;
            std::cerr << "ENC-MISMATCH id=" << id << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }

        // (b) decode Java's bytes back to the id
        try {
            PacketBuffer dec(unhex(expHex));
            ClientboundPingPacket back = ClientboundPingPacket::read(dec);
            if (back.getId() != id) {
                ok = false;
                std::cerr << "DECODE-MISMATCH id=" << id << " got=" << back.getId() << "\n";
            }
        } catch (const std::exception& e) {
            ok = false;
            std::cerr << "DECODE-EXCEPTION id=" << id << ": " << e.what() << "\n";
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktPingCbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
