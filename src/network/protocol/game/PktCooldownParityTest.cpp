// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundCooldownPacket
// against the REAL STREAM_CODEC (tools/PktCooldownParity.java ground truth).
//
// The packet is a record (Identifier cooldownGroup, int duration). Its codec is
// StreamCodec.composite of exactly two fields, written in this order:
//   (1) Identifier.STREAM_CODEC = STRING_UTF8.map(parse, toString) -> writeUtf of
//       Identifier.toString() ("namespace:path"): VarInt(UTF-8 byte length) + bytes,
//       maxLength 32767. This is mc::net::PacketBuffer::writeString (the Utf8String
//       port: writeString == writeUtf).
//   (2) ByteBufCodecs.VAR_INT -> writeVarInt(duration): LEB128.
// No packet-type id is part of the codec bytes (that framing lives outside the
// StreamCodec). No registry-held type is on the wire — the Identifier travels purely
// as its toString() text — so we reuse the certified PacketBuffer (FriendlyByteBuf
// port) directly.
//
// For each ENC case we (a) ENCODE cooldownGroup(UTF-8 bytes)+duration and require the
// bytes match Java's hex exactly, (b) check readableBytes() == byte count, and
// (c) DECODE the Java bytes back through PacketBuffer and require both fields
// round-trip identically.
//
//   pkt_cooldown_parity [--cases mcpp/build/pkt_cooldown.tsv]
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
    std::string casesPath = "build/pkt_cooldown.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t cooldownGroup(UTF-8 hex) \t duration(dec) \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, idHex, durStr, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, idHex, '\t')) continue;
        if (!std::getline(ss, durStr, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // cooldownGroup column is the UTF-8 HEX of Identifier.toString(); decode to the
        // exact byte string the packet carries so writeUtf runs on real bytes.
        std::vector<uint8_t> idBytes = unhex(idHex);
        std::string cooldownGroup(idBytes.begin(), idBytes.end());
        int32_t duration = (int32_t)std::stoll(durStr);
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly — writeUtf(toString) then writeVarInt(duration).
        PacketBuffer enc;
        try {
            enc.writeString(cooldownGroup);
            enc.writeVarInt(duration);
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

        // (b) DECODE the Java bytes -> (cooldownGroup, duration) round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            std::string gotGroup = dec.readString();
            int32_t gotDur = dec.readVarInt();
            if (gotGroup != cooldownGroup) {
                std::cerr << "DECODE-GROUP-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotDur != duration) {
                std::cerr << "DECODE-DUR-MISMATCH " << name << " got " << gotDur
                          << " want " << duration << "\n";
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

    std::cout << "PktCooldownParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
