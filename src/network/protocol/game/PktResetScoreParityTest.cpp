// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundResetScorePacket
// against the REAL STREAM_CODEC (tools/PktResetScoreParity.java ground truth).
//
// The packet is a record (String owner, @Nullable String objectiveName). Its
// codec is exactly:
//     writeUtf(owner);                                   (read: readUtf();)
//     writeNullable(objectiveName, writeUtf);            (read: readNullable(readUtf);)
// where writeNullable = writeBoolean(present) then writeUtf if present, and
// readNullable = readBoolean() ? readUtf() : null. No packet-type id is part of
// the codec bytes. We reuse the certified mc::net::PacketBuffer (the
// FriendlyByteBuf port): writeString == writeUtf (Utf8String.write: VarInt
// byte-length prefix + UTF-8 bytes, maxLen 32767), readString == readUtf,
// writeBool/readBool == writeBoolean/readBoolean.
//
// For each ENC case we (a) ENCODE owner + optional objectiveName and require the
// bytes match Java's hex exactly, (b) check readableBytes() == byte count, and
// (c) DECODE the Java bytes back through PacketBuffer and require both fields
// round-trip identically (incl. the presence boolean).
//
//   pkt_reset_score_parity [--cases mcpp/build/pkt_reset_score.tsv]
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
std::string unhexStr(const std::string& s) {
    std::vector<uint8_t> v = unhex(s);
    return std::string(v.begin(), v.end());
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_reset_score.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t owner_hex \t hasObjective \t objective_hex \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, ownerHex, hasObjStr, objHex, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, ownerHex, '\t')) continue;
        if (!std::getline(ss, hasObjStr, '\t')) continue;
        if (!std::getline(ss, objHex, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // owner / objectiveName columns are UTF-8 HEX (ASCII-safe transport, see GT tool);
        // decode to the exact byte strings the packet carries so writeUtf is exercised on
        // real UTF-8.
        std::string owner = unhexStr(ownerHex);
        bool hasObjective = (hasObjStr == "1");
        std::string objectiveName = unhexStr(objHex);
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly: writeUtf(owner); writeNullable(obj).
        PacketBuffer enc;
        try {
            enc.writeString(owner);
            enc.writeBool(hasObjective);
            if (hasObjective) enc.writeString(objectiveName);
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

        // (b) DECODE the Java bytes -> owner + optional objectiveName round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            std::string gotOwner = dec.readString();
            bool gotHas = dec.readBool();
            std::string gotObj;
            if (gotHas) gotObj = dec.readString();

            if (gotOwner != owner) {
                std::cerr << "DECODE-OWNER-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotHas != hasObjective) {
                std::cerr << "DECODE-HASOBJ-MISMATCH " << name << " got "
                          << gotHas << " want " << hasObjective << "\n";
                ok = false;
            }
            if (gotHas && gotObj != objectiveName) {
                std::cerr << "DECODE-OBJ-MISMATCH " << name << "\n";
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

    std::cout << "PktResetScoreParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
