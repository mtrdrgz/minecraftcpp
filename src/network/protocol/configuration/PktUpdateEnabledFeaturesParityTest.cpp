// Byte-exact parity for
//   net.minecraft.network.protocol.configuration.ClientboundUpdateEnabledFeaturesPacket
// against the REAL STREAM_CODEC (tools/PktUpdateEnabledFeaturesParity.java).
//
// The packet is a record (Set<Identifier> features). Its codec is exactly:
//     output.writeCollection(this.features, FriendlyByteBuf::writeIdentifier);
// i.e. (FriendlyByteBuf.writeCollection + writeIdentifier + Identifier.toString):
//     writeVarInt(count);
//     for each id: writeUtf(id.toString());   // VarInt(byteLen)+UTF-8 bytes
// where Identifier.toString() == "namespace:path" (namespace ALWAYS present).
// No packet-type id is part of the codec bytes (framing lives outside the codec).
//
// We reuse the certified mc::net::PacketBuffer (the FriendlyByteBuf port):
//   writeVarInt == writeVarInt, writeString == writeUtf (Utf8String.write:
//   VarInt byte-length prefix + UTF-8 bytes), readVarInt/readString == read side.
//
// Set iteration order is fixed on BOTH sides: the GT builds the packet from a
// LinkedHashSet (insertion order) and emits ids in that exact order; this test
// writes the SAME ordered list. The codec FORMAT (count + each writeIdentifier)
// is what this gate certifies.
//
// For each ENC case we (a) ENCODE the ordered ids and require the bytes match
// Java's hex exactly, (b) check data().size() == byte count, and (c) DECODE the
// Java bytes back through PacketBuffer (readVarInt count, then readString per id)
// and require the ids round-trip identically.
//
//   pkt_update_enabled_features_parity [--cases mcpp/build/pkt_update_enabled_features.tsv]
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
// Decode one hex-encoded id-string (UTF-8 bytes -> exact std::string).
std::string unhexStr(const std::string& s) {
    std::vector<uint8_t> b = unhex(s);
    return std::string(b.begin(), b.end());
}
// Split the '|'-separated idsHex column into the ordered list of id strings.
// An empty column (the empty-set case) yields an empty list.
std::vector<std::string> splitIds(const std::string& col) {
    std::vector<std::string> out;
    if (col.empty()) return out;
    std::string tok;
    std::istringstream ss(col);
    while (std::getline(ss, tok, '|')) out.push_back(unhexStr(tok));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_update_enabled_features.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t count \t idsHex \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, countStr, idsCol, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, countStr, '\t')) continue;
        if (!std::getline(ss, idsCol, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        size_t count = (size_t)std::stoull(countStr);
        std::vector<std::string> ids = splitIds(idsCol);
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;
        if (ids.size() != count) {
            std::cerr << "COUNT-COLUMN-MISMATCH " << name << " ids=" << ids.size()
                      << " count=" << count << "\n";
            ok = false;
        }

        // (a) ENCODE: reproduce the codec exactly (writeCollection + writeIdentifier).
        PacketBuffer enc;
        try {
            enc.writeVarInt((int32_t)ids.size());
            for (const std::string& id : ids) enc.writeString(id);
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

        // (b) DECODE the Java bytes -> ordered id list round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            int32_t n = dec.readVarInt();
            if ((size_t)n != count) {
                std::cerr << "DECODE-COUNT-MISMATCH " << name << " got " << n
                          << " want " << count << "\n";
                ok = false;
            }
            for (int32_t i = 0; i < n && ok; ++i) {
                std::string gotId = dec.readString();
                if ((size_t)i >= ids.size() || gotId != ids[(size_t)i]) {
                    std::cerr << "DECODE-ID-MISMATCH " << name << " at " << i << "\n";
                    ok = false;
                }
            }
            if (ok && dec.remaining() != 0) {
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

    std::cout << "PktUpdateEnabledFeaturesParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
