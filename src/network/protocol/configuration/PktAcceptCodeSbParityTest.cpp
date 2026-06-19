// ServerboundAcceptCodeOfConductPacket codec parity vs the REAL net.minecraft
// StreamCodec (tools/PktAcceptCodeSbParity.java ground truth).
//
// ServerboundAcceptCodeOfConductPacket.STREAM_CODEC == StreamCodec.unit(INSTANCE): a
// unit codec whose encode writes NOTHING to the buffer and whose decode returns the
// singleton without consuming any bytes. The record packet has no components and no
// fields of any kind on the wire. So the encoded body is EXACTLY zero bytes
// (readableBytes()==0).
//
// The C++ side models this directly: an empty PacketBuffer (we write nothing, exactly
// as StreamCodec.unit's encoder writes nothing) must have size()==0, and its hex must
// equal the expected hex (the empty string). Decoding the (empty) Java bytes must
// likewise leave a buffer with nothing readable.
//
// TSV rows:  ENC <name> <readableBytes> <hexBytes(empty)> <isInstance>
//
//   pkt_accept_code_sb_parity [--cases mcpp/build/pkt_accept_code_sb.tsv]
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
    std::string casesPath = "mcpp/build/pkt_accept_code_sb.tsv";
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
        std::string tag, name, lenStr, expHex, instStr;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        // Note: expHex is genuinely empty for a zero-byte body, so it sits between two
        // tabs ("...\t\t...") and getline returns the empty string for that column.
        if (!std::getline(ss, name, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex, '\t') || !std::getline(ss, instStr)) continue;
        ++cases;

        size_t expLen = (size_t)std::stoull(lenStr);
        int isInstance = (int)std::stoll(instStr);

        bool ok = true;

        // The unit codec encodes the singleton by writing NOTHING. Mirror that: write
        // nothing into the PacketBuffer. The body must be exactly zero bytes.
        PacketBuffer enc;
        // (intentionally write nothing — StreamCodec.unit's encoder is a no-op body)
        std::string got = hex(enc.data());

        if (got != expHex) {
            ok = false;
            std::cerr << "ENC-MISMATCH name=" << name << "\n  got  '" << got
                      << "'\n  want '" << expHex << "'\n";
        }
        if (enc.data().size() != expLen) {
            ok = false;
            std::cerr << "LEN-MISMATCH name=" << name << " got " << enc.data().size()
                      << " want " << expLen << "\n";
        }
        // Sanity: the ground-truth invariant is that the body is empty.
        if (expLen != 0 || !expHex.empty()) {
            ok = false;
            std::cerr << "INVARIANT-MISMATCH name=" << name
                      << " expected zero-byte body but len=" << expLen
                      << " hex='" << expHex << "'\n";
        }
        // Sanity: Java's decode returned the singleton.
        if (isInstance != 1) {
            ok = false;
            std::cerr << "INSTANCE-MISMATCH name=" << name
                      << " java decode did not return the singleton\n";
        }

        // DECODE: an empty PacketBuffer built from the (empty) Java bytes must have
        // nothing readable — the unit decoder consumes no bytes and reads no fields.
        PacketBuffer dec(unhex(expHex));
        if (dec.size() != 0 || dec.remaining() != 0 || !dec.eof()) {
            ok = false;
            std::cerr << "DECODE-MISMATCH name=" << name << " expected empty buffer, got size="
                      << dec.size() << " remaining=" << dec.remaining() << "\n";
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktAcceptCodeSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
