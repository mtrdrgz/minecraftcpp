// Byte-exact parity for net.minecraft.network.protocol.login.ClientboundLoginCompressionPacket.
//
// Ground truth: tools/PktLoginCompressionCbParity.java encodes each threshold
// through the REAL STREAM_CODEC into a FriendlyByteBuf and dumps readableBytes +
// body hex. The packet body is a single int written via FriendlyByteBuf.writeVarInt
// (LEB128); the matching C++ primitive is PacketBuffer::writeVarInt. There is no
// dedicated C++ ClientboundLoginCompressionPacket class — the codec is exactly one
// writeVarInt, so this test exercises the certified PacketBuffer directly: it must
// (a) produce the identical bytes and (b) decode those bytes back to the threshold.
//
//   LOGIN_COMPRESSION_CB <threshold> <readableBytes> <hex>
//
//   pkt_login_compression_cb_parity [--cases mcpp/build/pkt_login_compression_cb.tsv]
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
    std::vector<uint8_t> v;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return v;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_login_compression_cb.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string tag, thrStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t') || !std::getline(ss, thrStr, '\t')
            || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
        if (tag != "LOGIN_COMPRESSION_CB") continue;
        ++cases;

        int32_t threshold = (int32_t)std::stoll(thrStr);
        size_t expN = (size_t)std::stoull(nStr);

        // (a) encode via the same primitive the real codec uses (writeVarInt)
        PacketBuffer enc;
        enc.writeVarInt(threshold);
        std::string got = hex(enc.data());

        bool ok = true;
        if (enc.data().size() != expN) {
            ok = false;
            std::cerr << "LEN-MISMATCH threshold=" << threshold << " got=" << enc.data().size()
                      << " want=" << expN << "\n";
        }
        if (got != expHex) {
            ok = false;
            std::cerr << "ENC-MISMATCH threshold=" << threshold << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }

        // (b) decode Java's bytes back to the threshold
        try {
            PacketBuffer dec(unhex(expHex));
            int32_t back = dec.readVarInt();
            if (back != threshold) {
                ok = false;
                std::cerr << "DECODE-MISMATCH threshold=" << threshold << " got=" << back << "\n";
            }
        } catch (const std::exception& e) {
            ok = false;
            std::cerr << "DECODE-EXCEPTION threshold=" << threshold << ": " << e.what() << "\n";
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktLoginCompressionCbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
