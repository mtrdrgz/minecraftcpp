// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundSetDisplayObjectivePacket
// against the REAL STREAM_CODEC (tools/PktSetDisplayObjectiveParity.java ground truth).
//
// The packet holds (DisplaySlot slot, String objectiveName). Its codec is exactly:
//     writeById(DisplaySlot::id, slot);   (read: readById(DisplaySlot.BY_ID))
//     writeUtf(objectiveName);            (read: readUtf())
// where writeById = writeVarInt(slot.id()) — a plain VarInt of the slot's id in
// [0..18] (LIST..TEAM_WHITE; DisplaySlot.java) — and writeUtf = Utf8String.write
// (VarInt byte-length prefix + UTF-8 bytes, maxLen 32767). No packet-type id is
// part of the codec bytes. We reuse the certified mc::net::PacketBuffer (the
// FriendlyByteBuf port): writeVarInt == writeVarInt, writeString == writeUtf,
// readVarInt == readVarInt, readString == readUtf.
//
// For each ENC case we (a) ENCODE slot id (VarInt) + objectiveName (writeUtf) and
// require the bytes match Java's hex exactly, (b) check readableBytes() == byte
// count, and (c) DECODE the Java bytes back through PacketBuffer and require both
// fields round-trip identically.
//
//   pkt_set_display_objective_parity [--cases mcpp/build/pkt_set_display_objective.tsv]
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
    std::string casesPath = "mcpp/build/pkt_set_display_objective.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t slotId \t objective_hex \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, slotStr, objHex, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, slotStr, '\t')) continue;
        if (!std::getline(ss, objHex, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // slotId is the DisplaySlot.id() (decimal); objective_hex is the
        // objectiveName as UTF-8 HEX (ASCII-safe transport, see GT tool) so writeUtf
        // is exercised on real UTF-8. objective_hex may be empty (empty string).
        int32_t slotId = (int32_t)std::stol(slotStr);
        std::string objectiveName = unhexStr(objHex);
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly: writeVarInt(slotId); writeUtf(name).
        PacketBuffer enc;
        try {
            enc.writeVarInt(slotId);
            enc.writeString(objectiveName);
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

        // (b) DECODE the Java bytes -> slot id + objectiveName round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            int32_t gotSlot = dec.readVarInt();
            std::string gotObj = dec.readString();

            if (gotSlot != slotId) {
                std::cerr << "DECODE-SLOT-MISMATCH " << name << " got "
                          << gotSlot << " want " << slotId << "\n";
                ok = false;
            }
            if (gotObj != objectiveName) {
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

    std::cout << "PktSetDisplayObjectiveParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
