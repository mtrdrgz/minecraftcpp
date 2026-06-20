// Byte-exact parity for net.minecraft.network.protocol.common.ClientboundStoreCookiePacket
// against the REAL STREAM_CODEC (tools/PktStoreCookieCbParity.java ground truth).
//
// The packet is a record (Identifier key, byte[] payload). Its write(FriendlyByteBuf)
// is exactly:
//     writeIdentifier(key);  // == writeUtf(key.toString()): VarInt(byteLen)+UTF-8 bytes
//     writeByteArray(payload);// == VarInt(payload.length) + raw payload bytes
// The read side is readIdentifier() then ByteBufCodecs.byteArray(5120).decode (a
// VarInt-prefixed byte[]). No packet-type id is part of the codec bytes.
//
// We reuse the certified mc::net::PacketBuffer (the FriendlyByteBuf port):
//   writeString == writeUtf,  writeVarInt == VarInt.write,  writeBytes == raw bytes,
//   readString  == readUtf,   readVarInt  == VarInt.read,   readBytes  == raw bytes.
//
// For each ENC case we (a) ENCODE key+payload and require the bytes match Java's hex
// exactly, (b) check readableBytes() == byte count, and (c) DECODE the Java bytes back
// through PacketBuffer and require key/payload round-trip identically.
//
//   pkt_store_cookie_cb_parity [--cases mcpp/build/pkt_store_cookie_cb.tsv]
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
    std::string casesPath = "build/pkt_store_cookie_cb.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t key_utf8_hex \t payload_hex \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, keyHex, payHex, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, keyHex, '\t')) continue;
        if (!std::getline(ss, payHex, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // key column is UTF-8 HEX (ASCII-safe transport); decode to the exact byte
        // string so writeUtf runs on real key bytes. payload "-" sentinel == empty.
        std::string key;
        { std::vector<uint8_t> kb = unhex(keyHex); key.assign(kb.begin(), kb.end()); }
        std::vector<uint8_t> payload = (payHex == "-") ? std::vector<uint8_t>{} : unhex(payHex);
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce write(FriendlyByteBuf) exactly.
        PacketBuffer enc;
        try {
            enc.writeString(key);                 // writeIdentifier == writeUtf(key.toString())
            enc.writeVarInt((int32_t)payload.size()); // VarInt length prefix
            enc.writeBytes(payload);              // raw payload bytes
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

        // (b) DECODE the Java bytes -> key/payload round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            std::string gotKey = dec.readString();           // readIdentifier == readUtf
            int32_t plen = dec.readVarInt();                 // byteArray length prefix
            std::vector<uint8_t> gotPayload = dec.readBytes((size_t)plen);
            if (gotKey != key) {
                std::cerr << "DECODE-KEY-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotPayload != payload) {
                std::cerr << "DECODE-PAYLOAD-MISMATCH " << name << " gotlen "
                          << gotPayload.size() << " want " << payload.size() << "\n";
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

    std::cout << "PktStoreCookieCbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
