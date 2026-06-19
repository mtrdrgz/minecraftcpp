// Byte-exact parity for net.minecraft.network.protocol.login.ServerboundKeyPacket
// against the REAL STREAM_CODEC (tools/PktKeySbParity.java ground truth).
//
// The packet carries two raw byte[] fields. Its codec is exactly:
//     writeByteArray(keybytes);            // VarInt(len) + raw bytes
//     writeByteArray(encryptedChallenge);  // VarInt(len) + raw bytes
//   (read: readByteArray(); readByteArray();)
// No packet-type id is part of the codec bytes. We reuse the certified
// mc::net::PacketBuffer (the FriendlyByteBuf port): writeByteArray is reproduced
// as writeVarInt(len) + writeBytes(bytes) (FriendlyByteBuf.writeByteArray =
// VarInt.write(bytes.length) + output.writeBytes(bytes)); the read side as
// readVarInt() + readBytes(len).
//
// For each ENC case we (a) ENCODE the two byte[]s and require the bytes match
// Java's hex exactly, (b) check readableBytes() == byte count, and (c) DECODE the
// Java bytes back through PacketBuffer and require both arrays round-trip
// identically with no trailing bytes.
//
//   pkt_key_sb_parity [--cases mcpp/build/pkt_key_sb.tsv]
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
// FriendlyByteBuf.writeByteArray: VarInt(bytes.length) then the raw bytes.
void writeByteArray(PacketBuffer& buf, const std::vector<uint8_t>& bytes) {
    buf.writeVarInt((int32_t)bytes.size());
    buf.writeBytes(bytes);
}
// FriendlyByteBuf.readByteArray: VarInt length then that many bytes.
std::vector<uint8_t> readByteArray(PacketBuffer& buf) {
    int32_t len = buf.readVarInt();
    return buf.readBytes((size_t)len);
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_key_sb.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t keybytes_hex \t encryptedChallenge_hex \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, keyHex, chalHex, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, keyHex, '\t')) continue;
        if (!std::getline(ss, chalHex, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // byte[] columns are raw binary HEX (empty hex => empty array).
        std::vector<uint8_t> keybytes = unhex(keyHex);
        std::vector<uint8_t> encryptedChallenge = unhex(chalHex);
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly (two writeByteArray calls).
        PacketBuffer enc;
        try {
            writeByteArray(enc, keybytes);
            writeByteArray(enc, encryptedChallenge);
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

        // (b) DECODE the Java bytes -> both arrays round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            std::vector<uint8_t> gotKey = readByteArray(dec);
            std::vector<uint8_t> gotChal = readByteArray(dec);
            if (gotKey != keybytes) {
                std::cerr << "DECODE-KEY-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotChal != encryptedChallenge) {
                std::cerr << "DECODE-CHAL-MISMATCH " << name << "\n";
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

    std::cout << "PktKeySbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
