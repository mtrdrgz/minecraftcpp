// Parity gate for net.minecraft.network.protocol.game.ClientboundCustomChatCompletionsPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktCustomChatCompletionsParity.java GT).
//
// Verbatim from 26.1.2/src ClientboundCustomChatCompletionsPacket.java (write 19-22):
//   write(out):
//     out.writeEnum(this.action);                              -> writeVarInt(action.ordinal())
//     out.writeCollection(this.entries, FriendlyByteBuf::writeUtf);
//        = writeVarInt(entries.size()); for e in entries: out.writeUtf(e)
//   writeEnum(v)  = writeVarInt(v.ordinal())     (FriendlyByteBuf:471-473)
//   writeUtf(s)   = VarInt(UTF-8 byte length) + UTF-8 bytes  (FriendlyByteBuf:572-579, Utf8String:35-55)
//
// Action ordinals (ClientboundCustomChatCompletionsPacket.java 33-37): ADD=0 REMOVE=1 SET=2 —
// gated via ENUM rows.
//
// Reuses the certified PacketBuffer (the FriendlyByteBuf port): writeVarInt is LEB128,
// writeString == writeUtf (VarInt UTF-8-byte-length + bytes), both byte-for-byte vs Java.
//
//   pkt_custom_chat_completions_parity [--cases mcpp/build/pkt_custom_chat_completions.tsv]
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
    if (h == "-" || h.empty()) return out;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back((char)(uint8_t)std::stoi(h.substr(i, 2), nullptr, 16));
    return out;
}

// Each entry's UTF-8-hex is TERMINATED by a comma (trailing comma after every entry, see
// GT tool), so getline(',') yields exactly `count` tokens incl. empty-string entries:
// "" -> 0 ; "," -> 1 empty ; "<hexA>,," -> {A, ""}. This disambiguates an empty list from
// a single empty string (which a between-only join + "-" sentinel could not).
std::vector<std::string> parseEntries(const std::string& field) {
    std::vector<std::string> out;
    std::stringstream ss(field);
    std::string tok;
    while (std::getline(ss, tok, ',')) out.push_back(fromHexBytes(tok));
    return out;
}

// Action constants in declaration order (== ordinal), verbatim from
// ClientboundCustomChatCompletionsPacket.java lines 33-37.
const std::array<const char*, 3> kActionNames = {
    "ADD",     // 0
    "REMOVE",  // 1
    "SET",     // 2
};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_custom_chat_completions.tsv";
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
            if (ord < 0 || ord >= (int)kActionNames.size() ||
                name != kActionNames[(size_t)ord]) {
                ++mismatches;
                std::cerr << "ENUM-MISMATCH ord=" << ord << " got="
                          << (ord >= 0 && ord < (int)kActionNames.size()
                                  ? kActionNames[(size_t)ord] : "<oob>")
                          << " want=" << name << "\n";
            }
        } else if (tag == "ENC") {
            // ENC <name> <actionOrdinal> <count> <entriesHex> <readableBytes> <hexBytes>
            std::string caseName, actStr, countStr, entriesField, readableStr, expHex;
            if (!std::getline(ss, caseName, '\t') ||
                !std::getline(ss, actStr, '\t') ||
                !std::getline(ss, countStr, '\t') ||
                !std::getline(ss, entriesField, '\t') ||
                !std::getline(ss, readableStr, '\t') ||
                !std::getline(ss, expHex)) continue;
            ++cases;

            int32_t actionOrdinal = (int32_t)std::stoll(actStr);
            int count = std::stoi(countStr);
            int expReadable = std::stoi(readableStr);
            std::vector<std::string> entries = parseEntries(entriesField);

            if ((int)entries.size() != count) {
                ++mismatches;
                std::cerr << "COUNT-PARSE-MISMATCH " << caseName << " parsed="
                          << entries.size() << " want=" << count << "\n";
            }

            // --- encode: mirror the codec order exactly ---
            // writeEnum(action) == writeVarInt(ordinal); then writeCollection:
            // writeVarInt(size) followed by writeUtf(each entry).
            PacketBuffer enc;
            enc.writeVarInt(actionOrdinal);                   // writeEnum(action)
            enc.writeVarInt((int32_t)entries.size());         // writeCollection size
            for (const std::string& e : entries)
                enc.writeString(e);                           // writeUtf(entry)

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
            int32_t dActionOrdinal = dec.readVarInt();        // readEnum -> readVarInt
            int32_t dCount = dec.readVarInt();                // readList size
            std::vector<std::string> dEntries;
            bool rtOk = dActionOrdinal == actionOrdinal && dCount == (int32_t)entries.size();
            if (rtOk) {
                for (int32_t i = 0; i < dCount; ++i) dEntries.push_back(dec.readString());
                if ((int)dEntries.size() == count) {
                    for (int i = 0; i < count; ++i)
                        if (dEntries[(size_t)i] != entries[(size_t)i]) { rtOk = false; break; }
                } else {
                    rtOk = false;
                }
            }
            if (rtOk && dec.remaining() != 0) rtOk = false;   // consumed exactly
            if (!rtOk) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH " << caseName
                          << " action=" << dActionOrdinal << "/" << actionOrdinal
                          << " count=" << dCount << "/" << count
                          << " rem=" << dec.remaining() << "\n";
            }
        }
    }

    std::cout << "PktCustomChatCompletionsParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
