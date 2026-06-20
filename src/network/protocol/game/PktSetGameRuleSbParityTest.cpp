// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundSetGameRulePacket
// vs the REAL ServerboundSetGameRulePacket.STREAM_CODEC (tools/PktSetGameRuleSbParity.java).
//
// The packet is a record (List<Entry> entries), Entry(ResourceKey<GameRule<?>> gameRuleKey,
// String value). Its STREAM_CODEC, in codec order, encodes:
//   ByteBufCodecs.list()          -> writeCount(size) = VarInt(size)   (ByteBufCodecs.java:399-405)
//   per Entry:
//     ResourceKey.streamCodec(GAME_RULE) = Identifier.STREAM_CODEC      (ResourceKey.java:20-22)
//        = ByteBufCodecs.STRING_UTF8.map(parse, Identifier::toString)   (Identifier.java:19)
//        -> writeUtf(gameRuleKey.identifier().toString())               // "namespace:path"
//     ByteBufCodecs.STRING_UTF8 -> writeUtf(value)
//   writeUtf == Utf8String.write: VarInt(byteLength) + UTF-8 bytes      (Utf8String.java:35-55)
//
// No Holder / registry-id / ItemStack / Component / NBT is on the wire: a ResourceKey writes
// ONLY its Identifier location (the registry id is implicit), so every field is a primitive
// (VarInt list count + UTF-8 strings). The certified PacketBuffer (the FriendlyByteBuf port)
// rebuilds the body directly:
//   writeVarInt(count) + per entry [ writeString(keyId) + writeString(value) ]
// writeString == writeUtf: VarInt(byteLength) + UTF-8 bytes, byte-for-byte vs Java.
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte and the
// same readableBytes; decode(Java bytes) must then recover count + every (keyId, value).
//
//   pkt_set_game_rule_sb_parity [--cases mcpp/build/pkt_set_game_rule_sb.tsv]
//
// Row: ENC <name> <count> <keyHex0:valHex0,keyHex1:valHex1,...|-> <readableBytes> <hex>
// where the entries column is a comma-separated list of "keyHex:valHex" pairs (hex of the
// UTF-8 bytes so the ascii TSV transport survives multi-byte strings), or a literal "-" for
// an empty list. Each hex half decodes back to the exact UTF-8 std::string passed to
// writeString (which itself measures/encodes exactly like FriendlyByteBuf.writeUtf).
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
    v.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return v;
}

// Hex of UTF-8 bytes -> the raw UTF-8 std::string (passed straight to writeString).
std::string strFromHex(const std::string& h) {
    std::vector<uint8_t> b = unhex(h);
    return std::string(b.begin(), b.end());
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_set_game_rule_sb.tsv";
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
        std::string tag, name, countStr, entStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, countStr, '\t') ||
            !std::getline(ss, entStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t count = (int32_t)std::stol(countStr);
        size_t readableBytes = (size_t)std::stoul(lenStr);

        // Parse the entries column into (keyId, value) UTF-8 strings.
        std::vector<std::pair<std::string, std::string>> entries;
        if (entStr != "-") {
            std::istringstream es(entStr);
            std::string pair;
            while (std::getline(es, pair, ',')) {
                size_t colon = pair.find(':');
                std::string keyHex = (colon == std::string::npos) ? pair : pair.substr(0, colon);
                std::string valHex = (colon == std::string::npos) ? "" : pair.substr(colon + 1);
                entries.emplace_back(strFromHex(keyHex), strFromHex(valHex));
            }
        }
        if ((int32_t)entries.size() != count) {
            ++mismatches;
            std::cerr << "CASE-PARSE-MISMATCH " << name << " parsed " << entries.size()
                      << " entries, count=" << count << "\n";
            continue;
        }

        // (1) ENCODE: write the body in codec order via PacketBuffer.
        PacketBuffer enc;
        enc.writeVarInt(count);                       // ByteBufCodecs.list() count = VarInt(size)
        for (const auto& [keyId, value] : entries) {
            enc.writeString(keyId);                   // ResourceKey -> Identifier -> writeUtf(toString())
            enc.writeString(value);                   // ByteBufCodecs.STRING_UTF8 -> writeUtf(value)
        }

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " count=" << count
                      << "\n  got  (" << enc.data().size() << ") " << got
                      << "\n  want (" << readableBytes << ") " << expHex << "\n";
            continue;
        }

        // (2) DECODE: read the Java bytes back and verify every field.
        std::vector<uint8_t> raw = unhex(expHex);
        if (raw.size() != readableBytes) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH " << name << " hex=" << expHex
                      << " bytes=" << raw.size() << " want=" << readableBytes << "\n";
            continue;
        }
        PacketBuffer dec(raw);
        int32_t gotCount = dec.readVarInt();
        bool ok = (gotCount == count);
        for (int32_t i = 0; ok && i < gotCount; ++i) {
            std::string gotKey = dec.readString();
            std::string gotVal = dec.readString();
            if (gotKey != entries[i].first || gotVal != entries[i].second) ok = false;
        }
        ok = ok && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name << " count=" << count << "/" << gotCount
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktSetGameRuleSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
