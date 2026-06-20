// Byte-exact parity for net.minecraft.network.protocol.common.ClientboundTransferPacket
// against the REAL STREAM_CODEC (tools/PktTransferCbParity.java ground truth).
//
// The packet is a record (String host, int port). Its codec is exactly:
//     writeUtf(host); writeVarInt(port);   (read: readUtf(); readVarInt();)
// No packet-type id is part of the codec bytes. We reuse the certified
// mc::net::PacketBuffer (the FriendlyByteBuf port): writeString == writeUtf,
// writeVarInt == VarInt.write, readString == readUtf, readVarInt == VarInt.read.
//
// For each ENC case we (a) ENCODE host+port and require the bytes match Java's
// hex exactly, (b) check readableBytes() == byte count, and (c) DECODE the Java
// bytes back through PacketBuffer and require host/port round-trip identically.
//
//   pkt_transfer_cb_parity [--cases mcpp/build/pkt_transfer_cb.tsv]
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
    std::string casesPath = "build/pkt_transfer_cb.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t host \t port \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, host, portStr, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, host, '\t')) continue;
        if (!std::getline(ss, portStr, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // host column is UTF-8 HEX (ASCII-safe transport, see GT tool); decode to the
        // exact byte string so writeUtf is exercised on real UTF-8.
        { std::vector<uint8_t> hb = unhex(host); host.assign(hb.begin(), hb.end()); }
        int32_t port = (int32_t)std::stoll(portStr);
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly.
        PacketBuffer enc;
        try {
            enc.writeString(host);   // writeUtf
            enc.writeVarInt(port);   // writeVarInt
        } catch (const std::exception& e) {
            std::cerr << "ENC-EXCEPTION " << name << ": " << e.what() << "\n";
            ok = false;
        }

        if (ok) {
            std::string got = hex(enc.data());
            if (got != expHex) {
                std::cerr << "ENC-MISMATCH " << name << "\n  got  " << got
                          << "\n  want " << expHex << "\n";
                ok = false;
            }
            if (enc.data().size() != readable) {
                std::cerr << "READABLE-MISMATCH " << name << " got "
                          << enc.data().size() << " want " << readable << "\n";
                ok = false;
            }
        }

        // (b) DECODE the Java bytes -> host/port round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            std::string gotHost = dec.readString();   // readUtf
            int32_t gotPort = dec.readVarInt();       // readVarInt
            if (gotHost != host) {
                std::cerr << "DECODE-HOST-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotPort != port) {
                std::cerr << "DECODE-PORT-MISMATCH " << name << " got " << gotPort
                          << " want " << port << "\n";
                ok = false;
            }
            if (dec.remaining() != 0) {
                std::cerr << "DECODE-TRAILING " << name << " remaining "
                          << dec.remaining() << "\n";
                ok = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "DECODE-EXCEPTION " << name << ": " << e.what() << "\n";
            ok = false;
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktTransferCbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
