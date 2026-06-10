// Byte-exact parity for net.minecraft.network.protocol.login.ClientboundHelloPacket
// against the REAL STREAM_CODEC (tools/PktHelloCbParity.java ground truth).
//
// The packet has four fields (String serverId, byte[] publicKey, byte[] challenge,
// boolean shouldAuthenticate). Its write(FriendlyByteBuf) is EXACTLY:
//     writeUtf(serverId);          // VarInt UTF-8 byte length + UTF-8 bytes
//     writeByteArray(publicKey);   // VarInt length + raw bytes
//     writeByteArray(challenge);   // VarInt length + raw bytes
//     writeBoolean(shouldAuthenticate);
// read is: readUtf(20); readByteArray(); readByteArray(); readBoolean().
// No packet-type id is part of the codec bytes. We reuse the certified
// mc::net::PacketBuffer (the FriendlyByteBuf port): writeString == writeUtf,
// writeVarInt == VarInt.write, writeBytes == ByteBuf.writeBytes, writeBool ==
// writeBoolean (and the matching read* methods).
//
// For each ENC case we (a) ENCODE the four fields and require the bytes match Java's
// hex exactly, (b) check readableBytes() == byte count, and (c) DECODE the Java bytes
// back through PacketBuffer and require every field round-trips identically.
//
//   pkt_hello_cb_parity [--cases mcpp/build/pkt_hello_cb.tsv]
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
    std::string casesPath = "mcpp/build/pkt_hello_cb.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t serverIdHex \t publicKeyHex \t challengeHex \t shouldAuth \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, serverIdHex, publicKeyHex, challengeHex, authStr, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, serverIdHex, '\t')) continue;
        if (!std::getline(ss, publicKeyHex, '\t')) continue;
        if (!std::getline(ss, challengeHex, '\t')) continue;
        if (!std::getline(ss, authStr, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // serverId column is UTF-8 HEX; publicKey/challenge are binary HEX (ASCII-safe
        // transport, see GT tool). Decode each to its exact byte sequence.
        std::vector<uint8_t> sidBytes = unhex(serverIdHex);
        std::string serverId(sidBytes.begin(), sidBytes.end());
        std::vector<uint8_t> publicKey = unhex(publicKeyHex);
        std::vector<uint8_t> challenge = unhex(challengeHex);
        bool shouldAuthenticate = (authStr == "1");
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly.
        //     writeUtf(serverId); writeByteArray(publicKey); writeByteArray(challenge);
        //     writeBoolean(shouldAuthenticate).
        PacketBuffer enc;
        try {
            enc.writeString(serverId);                       // writeUtf (max 32767)
            enc.writeVarInt((int32_t)publicKey.size());      // writeByteArray length prefix
            enc.writeBytes(publicKey);                       // writeByteArray bytes
            enc.writeVarInt((int32_t)challenge.size());      // writeByteArray length prefix
            enc.writeBytes(challenge);                       // writeByteArray bytes
            enc.writeBool(shouldAuthenticate);               // writeBoolean
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

        // (b) DECODE the Java bytes -> field round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            std::string gotServerId = dec.readString(20);    // readUtf(20)
            int32_t pkLen = dec.readVarInt();                // readByteArray length
            std::vector<uint8_t> gotPublicKey = dec.readBytes((size_t)pkLen);
            int32_t chLen = dec.readVarInt();                // readByteArray length
            std::vector<uint8_t> gotChallenge = dec.readBytes((size_t)chLen);
            bool gotAuth = dec.readBool();                   // readBoolean

            if (gotServerId != serverId) {
                std::cerr << "DECODE-SERVERID-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotPublicKey != publicKey) {
                std::cerr << "DECODE-PUBLICKEY-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotChallenge != challenge) {
                std::cerr << "DECODE-CHALLENGE-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotAuth != shouldAuthenticate) {
                std::cerr << "DECODE-AUTH-MISMATCH " << name << " got " << gotAuth
                          << " want " << shouldAuthenticate << "\n";
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

    std::cout << "PktHelloCbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
