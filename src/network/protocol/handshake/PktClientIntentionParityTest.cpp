// Byte-exact parity for net.minecraft.network.protocol.handshake.ClientIntentionPacket
// against the REAL STREAM_CODEC (tools/PktClientIntentionParity.java ground truth).
//
// The packet is a record (int protocolVersion, String hostName, int port,
// ClientIntent intention). Its codec body is exactly (from 26.1.2/src
// ClientIntentionPacket.write):
//     writeVarInt(protocolVersion);   // LEB128 VarInt
//     writeUtf(hostName);             // VarInt UTF-8 byte-length prefix + UTF-8 bytes
//     writeShort(port);               // netty writeShort: low 16 bits, big-endian
//     writeVarInt(intention.id());    // id() is 1/2/3 (STATUS/LOGIN/TRANSFER), NOT ordinal
// and reads back: readVarInt(), readUtf(255), readUnsignedShort(), byId(readVarInt()).
//
// No packet-type id is part of the codec bytes (that framing lives outside the
// StreamCodec). We reuse the certified mc::net::PacketBuffer (the FriendlyByteBuf
// port): writeVarInt==VarInt.write, writeString==writeUtf (Utf8String.write),
// writeShort==FriendlyByteBuf.writeShort (low-16 big-endian), readVarInt/readString.
//
// CRITICAL: the intention on the wire is intention.id() (1/2/3), supplied by the GT
// tool as the <intentId> column. The C++ writes/reads that VarInt verbatim.
//
// For each ENC case we (a) ENCODE the four fields and require the bytes match Java's
// hex exactly, (b) check readableBytes() == byte count, and (c) DECODE the Java
// bytes back through PacketBuffer and require every field round-trips identically.
//
//   pkt_client_intention_parity [--cases mcpp/build/pkt_client_intention.tsv]
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
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> v;
    if (s == "-") return v;  // sentinel for empty host
    v.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return v;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_client_intention.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t protocolVersion \t hostNameHex \t port \t intentId \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, pvStr, hostHex, portStr, intentStr, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, pvStr, '\t')) continue;
        if (!std::getline(ss, hostHex, '\t')) continue;
        if (!std::getline(ss, portStr, '\t')) continue;
        if (!std::getline(ss, intentStr, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        int32_t protocolVersion = (int32_t)std::stoll(pvStr);
        // hostName column is UTF-8 HEX; decode to the exact byte string the packet
        // carries so writeUtf is exercised on real (possibly multi-byte) UTF-8.
        std::vector<uint8_t> hb = unhex(hostHex);
        std::string hostName(hb.begin(), hb.end());
        int32_t port = (int32_t)std::stoll(portStr);
        int32_t intentId = (int32_t)std::stoll(intentStr);
        size_t readable = (size_t)std::stoull(readableStr);

        // The wire short is the low 16 bits of port, big-endian; the decode reads it
        // unsigned (readUnsignedShort -> 0..65535). Mirror both here.
        uint16_t portWire = (uint16_t)(port & 0xffff);

        bool ok = true;

        // (a) ENCODE: reproduce the codec body exactly, in order.
        PacketBuffer enc;
        try {
            enc.writeVarInt(protocolVersion);      // writeVarInt
            enc.writeString(hostName);             // writeUtf (default maxLen 32767, matches write side)
            enc.writeShort((int16_t)portWire);     // writeShort: low 16 bits, big-endian
            enc.writeVarInt(intentId);             // writeVarInt(intention.id())
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

        // (b) DECODE the Java bytes -> all fields round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            int32_t gotPv = dec.readVarInt();             // readVarInt
            std::string gotHost = dec.readString(255);    // readUtf(255)
            uint16_t gotPort = (uint16_t)dec.readShort(); // readUnsignedShort: take low-16 unsigned
            int32_t gotIntent = dec.readVarInt();         // ClientIntent.byId(readVarInt())

            if (gotPv != protocolVersion) {
                std::cerr << "DECODE-PV-MISMATCH " << name << " got " << gotPv
                          << " want " << protocolVersion << "\n";
                ok = false;
            }
            if (gotHost != hostName) {
                std::cerr << "DECODE-HOST-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotPort != portWire) {
                std::cerr << "DECODE-PORT-MISMATCH " << name << " got " << gotPort
                          << " want " << portWire << "\n";
                ok = false;
            }
            if (gotIntent != intentId) {
                std::cerr << "DECODE-INTENT-MISMATCH " << name << " got " << gotIntent
                          << " want " << intentId << "\n";
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

    std::cout << "PktClientIntentionParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
