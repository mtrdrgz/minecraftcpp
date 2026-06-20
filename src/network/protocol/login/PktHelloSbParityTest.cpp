// Byte-exact parity for net.minecraft.network.protocol.login.ServerboundHelloPacket
// against the REAL STREAM_CODEC (tools/PktHelloSbParity.java ground truth).
//
// The packet is a record (String name, UUID profileId). Its write(FriendlyByteBuf)
// is exactly:
//     writeUtf(name, 16);   writeUUID(profileId);
//     (read: readUtf(16); readUUID();)
// No packet-type id is part of the codec bytes. We reuse the certified
// mc::net::PacketBuffer (the FriendlyByteBuf port): writeString == writeUtf
// (VarInt byte-length prefix + UTF-8 bytes), writeUUID == two big-endian longs
// (MSB then LSB); readString == readUtf, readUUID == two readLong()s.
//
// For each ENC case we (a) ENCODE name+uuid and require the bytes match Java's
// hex exactly, (b) check readableBytes() == byte count, and (c) DECODE the Java
// bytes back through PacketBuffer and require name/uuid round-trip identically.
//
//   pkt_hello_sb_parity [--cases mcpp/build/pkt_hello_sb.tsv]
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
// Parse a signed decimal long that may be Long.MIN_VALUE (-9223372036854775808),
// which std::stoll handles, but be explicit about the full int64 range.
int64_t parseLong(const std::string& s) {
    return (int64_t)std::stoll(s);
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_hello_sb.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t cname \t nameUtf8Hex \t uuidHi \t uuidLo \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, cname, nameHex, hiStr, loStr, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, cname, '\t')) continue;
        if (!std::getline(ss, nameHex, '\t')) continue;
        if (!std::getline(ss, hiStr, '\t')) continue;
        if (!std::getline(ss, loStr, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // name column is UTF-8 HEX (ASCII-safe transport, see GT tool); decode to
        // the exact byte string so writeUtf is exercised on real UTF-8 bytes.
        std::string name;
        { std::vector<uint8_t> nb = unhex(nameHex); name.assign(nb.begin(), nb.end()); }
        int64_t hi = parseLong(hiStr);
        int64_t lo = parseLong(loStr);
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly. writeUtf has maxLength=16 in
        // the real packet; all GT cases stay within that bound, so the default
        // writeString cap would not differ on these bytes, but we pass 16 to be
        // faithful to write(FriendlyByteBuf).
        PacketBuffer enc;
        try {
            enc.writeString(name, 16);          // writeUtf(name, 16)
            enc.writeUUID((uint64_t)hi, (uint64_t)lo);  // writeUUID(profileId)
        } catch (const std::exception& e) {
            std::cerr << "ENC-EXCEPTION " << cname << ": " << e.what() << "\n";
            ok = false;
        }

        if (ok) {
            std::string got = hex(enc.data());
            if (got != expHex) {
                std::cerr << "ENC-MISMATCH " << cname << "\n  got  " << got
                          << "\n  want " << expHex << "\n";
                ok = false;
            }
            if (enc.data().size() != readable) {
                std::cerr << "READABLE-MISMATCH " << cname << " got "
                          << enc.data().size() << " want " << readable << "\n";
                ok = false;
            }
        }

        // (b) DECODE the Java bytes -> name/uuid round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            std::string gotName = dec.readString(16);   // readUtf(16)
            uint64_t gotHi = 0, gotLo = 0;
            dec.readUUID(gotHi, gotLo);                  // readUUID
            if (gotName != name) {
                std::cerr << "DECODE-NAME-MISMATCH " << cname << "\n";
                ok = false;
            }
            if ((int64_t)gotHi != hi || (int64_t)gotLo != lo) {
                std::cerr << "DECODE-UUID-MISMATCH " << cname << "\n";
                ok = false;
            }
            if (dec.remaining() != 0) {
                std::cerr << "DECODE-TRAILING " << cname << " remaining "
                          << dec.remaining() << "\n";
                ok = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "DECODE-EXCEPTION " << cname << ": " << e.what() << "\n";
            ok = false;
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktHelloSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
