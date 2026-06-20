// Byte-exact parity for net.minecraft.network.protocol.login.ClientboundLoginDisconnectPacket
// against the REAL STREAM_CODEC (tools/PktLoginDisconnectCbParity.java ground truth).
//
// The packet is a record (Component reason). Its codec is:
//     ByteBufCodecs.lenientJson(262144)
//        .apply(ByteBufCodecs.fromCodec(OPS, ComponentSerialization.CODEC))
// On the wire this is purely a JSON STRING written via Utf8String.write
// (== FriendlyByteBuf.writeUtf): VarInt(UTF-8 byte length) + UTF-8 bytes. The
// Component is first serialized to a canonical JSON string by Mojang's codec +
// Gson (disableHtmlEscaping); that string is the ground-truth payload carried in
// the `jsonHex` column. No registry-held / NBT / Holder / ItemStack content ever
// reaches the wire (OPS = RegistryAccess.EMPTY over JsonOps), so the C++ side
// only needs to reproduce writeUtf(jsonString) on the EXACT canonical string.
//
// We reuse the certified mc::net::PacketBuffer (the FriendlyByteBuf port):
// writeString == writeUtf, readString == readUtf.
//
// For each ENC case we (a) ENCODE the json string and require the bytes match
// Java's hex exactly, (b) check readableBytes() == byte count, and (c) DECODE the
// Java bytes back through PacketBuffer and require the json string round-trips.
//
//   pkt_disconnect_login_cb_parity [--cases mcpp/build/pkt_disconnect_login_cb.tsv]
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
    std::string casesPath = "build/pkt_disconnect_login_cb.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t jsonHex \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, jsonHex, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, jsonHex, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // jsonHex column is UTF-8 HEX (ASCII-safe transport, see GT tool); decode to
        // the exact canonical JSON byte string so writeUtf is exercised on real bytes.
        std::string json;
        { std::vector<uint8_t> jb = unhex(jsonHex); json.assign(jb.begin(), jb.end()); }
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly — writeUtf(jsonString).
        PacketBuffer enc;
        try {
            enc.writeString(json, 262144);   // writeUtf with the packet's maxLength
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

        // (b) DECODE the Java bytes -> json string round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            std::string gotJson = dec.readString(262144);   // readUtf
            if (gotJson != json) {
                std::cerr << "DECODE-JSON-MISMATCH " << name << "\n  got  " << gotJson
                          << "\n  want " << json << "\n";
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

    std::cout << "PktLoginDisconnectCbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
