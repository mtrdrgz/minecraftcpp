// Parity gate for net.minecraft.network.protocol.game.ClientboundStopSoundPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktStopSoundParity.java GT).
//
// Verbatim from 26.1.2/src ClientboundStopSoundPacket.java (write 40-56, read 25-38):
//   write(out):
//     if (source != null) {
//         if (name != null) { out.writeByte(3); out.writeEnum(source); out.writeIdentifier(name); }
//         else              { out.writeByte(1); out.writeEnum(source); }
//     } else if (name != null) { out.writeByte(2); out.writeIdentifier(name); }
//     else                     { out.writeByte(0); }
//   writeEnum(s)    = writeVarInt(s.ordinal())     (FriendlyByteBuf:471-473)
//   writeIdentifier = writeUtf(id.toString())      (FriendlyByteBuf:585-588)
//
// The flag byte is therefore exactly (source?1:0) | (name?2:0). We reconstruct the
// packet from the TSV columns and re-derive the flag byte from the same booleans, so
// the C++ encoder mirrors the Java branch structure rather than echoing the flag.
//
// SoundSource ordinals (SoundSource.java 4-14): MASTER=0..UI=10 — gated via ENUM rows.
//
// Reuses the certified PacketBuffer (the FriendlyByteBuf port): writeVarInt is LEB128,
// writeString == writeUtf (VarInt UTF-8-byte-length + bytes), both byte-for-byte vs Java.
//
//   pkt_stop_sound_parity [--cases mcpp/build/pkt_stop_sound.tsv]
#include "../../PacketBuffer.h"

#include <array>
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
    std::vector<uint8_t> out;
    if (s == "-") return out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}

// hex of UTF-8 bytes -> the raw UTF-8 std::string (passed straight to writeString,
// which itself measures/encodes exactly like FriendlyByteBuf.writeUtf).
std::string fromHexBytes(const std::string& h) {
    std::string out;
    if (h == "-") return out;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back((char)(uint8_t)std::stoi(h.substr(i, 2), nullptr, 16));
    return out;
}

// SoundSource constants in declaration order (== ordinal), verbatim from
// SoundSource.java lines 4-14.
const std::array<const char*, 11> kSoundSourceNames = {
    "MASTER",   // 0
    "MUSIC",    // 1
    "RECORDS",  // 2
    "WEATHER",  // 3
    "BLOCKS",   // 4
    "HOSTILE",  // 5
    "NEUTRAL",  // 6
    "PLAYERS",  // 7
    "AMBIENT",  // 8
    "VOICE",    // 9
    "UI",       // 10
};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_stop_sound.tsv";
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
        std::string tag;
        if (!std::getline(ss, tag, '\t')) continue;

        if (tag == "ENUM") {
            // ENUM <ordinal> <name>
            std::string ordStr, name;
            if (!std::getline(ss, ordStr, '\t') || !std::getline(ss, name)) continue;
            ++cases;
            int ord = std::stoi(ordStr);
            if (ord < 0 || ord >= (int)kSoundSourceNames.size() ||
                name != kSoundSourceNames[(size_t)ord]) {
                ++mismatches;
                std::cerr << "ENUM-MISMATCH ord=" << ord << " got="
                          << (ord >= 0 && ord < (int)kSoundSourceNames.size()
                                  ? kSoundSourceNames[(size_t)ord] : "<oob>")
                          << " want=" << name << "\n";
            }
        } else if (tag == "ENC") {
            // ENC <name> <hasSource> <srcOrdinal> <hasName> <nameHex> <readableBytes> <hexBytes>
            std::string caseName, hasSrcStr, srcOrdStr, hasNameStr, nameHex,
                        readableStr, expHex;
            if (!std::getline(ss, caseName, '\t') ||
                !std::getline(ss, hasSrcStr, '\t') ||
                !std::getline(ss, srcOrdStr, '\t') ||
                !std::getline(ss, hasNameStr, '\t') ||
                !std::getline(ss, nameHex, '\t') ||
                !std::getline(ss, readableStr, '\t') ||
                !std::getline(ss, expHex)) continue;
            ++cases;

            bool hasSource = std::stoi(hasSrcStr) != 0;
            int32_t srcOrdinal = (int32_t)std::stoll(srcOrdStr);
            bool hasName = std::stoi(hasNameStr) != 0;
            int expReadable = std::stoi(readableStr);
            std::string nameStr = hasName ? fromHexBytes(nameHex) : std::string();

            // --- encode: mirror the Java branch structure exactly ---
            // flag = (source?1:0) | (name?2:0); then source then name in codec order.
            PacketBuffer enc;
            uint8_t flag = (uint8_t)((hasSource ? 1 : 0) | (hasName ? 2 : 0));
            enc.writeByte(flag);
            if (hasSource) enc.writeVarInt(srcOrdinal);       // writeEnum(ordinal)
            if (hasName)   enc.writeString(nameStr);          // writeIdentifier(toString())

            std::string got = hex(enc.data());
            if (got != expHex) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH " << caseName << "\n  got  " << got
                          << "\n  want " << expHex << "\n";
            }
            if ((int)enc.data().size() != expReadable) {
                ++mismatches;
                std::cerr << "READABLE-MISMATCH " << caseName << " got="
                          << enc.data().size() << " want=" << expReadable << "\n";
            }

            // --- decode round-trip: parse expected bytes back via PacketBuffer ---
            std::vector<uint8_t> bytes = unhex(expHex);
            PacketBuffer dec(bytes);
            uint8_t rflag = dec.readByte();
            bool dHasSource = (rflag & 1) > 0;
            bool dHasName = (rflag & 2) > 0;
            int32_t dSrcOrdinal = -1;
            std::string dName;
            if (dHasSource) dSrcOrdinal = dec.readVarInt();
            if (dHasName)   dName = dec.readString();

            bool rtOk = dHasSource == hasSource && dHasName == hasName;
            if (rtOk && hasSource) rtOk = dSrcOrdinal == srcOrdinal;
            if (rtOk && hasName)   rtOk = dName == nameStr;
            if (rtOk && dec.remaining() != 0) rtOk = false;  // consumed exactly
            if (!rtOk) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH " << caseName
                          << " flag=" << (int)rflag
                          << " src=" << dSrcOrdinal << "/" << srcOrdinal
                          << " name=" << dName << "/" << nameStr
                          << " rem=" << dec.remaining() << "\n";
            }
        }
    }

    std::cout << "PktStopSoundParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
