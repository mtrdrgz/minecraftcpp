// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundSetActionBarTextPacket
// against the REAL STREAM_CODEC (tools/PktSetActionBarTextParity.java ground truth).
//
// The packet is a single-field record (Component text). Its codec is exactly:
//     ComponentSerialization.TRUSTED_STREAM_CODEC.encode(buf, text)
// (Source: 26.1.2/src/net/minecraft/network/protocol/game/ClientboundSetActionBarTextPacket.java)
// There is NO Optional framing and no other field — the whole payload is one Component's
// NBT root. For a PLAIN literal (no style, no siblings) the codec collapses to a bare
// StringTag, so the wire is a root StringTag written by FriendlyByteBuf.writeNbt(Tag):
//     08 <u16 MUTF8 byte-length> <MUTF8 bytes>
//
// We reproduce that with the engine: build nbt::NbtTag::string_(text) and emit it via
// mc::net::PacketBuffer::writeNbt(const NbtTag&) (-> the certified root-tag NBT framing /
// MUTF-8 string writer), requiring byte-identical output to the real packet codec, the
// readableBytes() to match, and a leading 0x08 (StringTag type). This certifies the
// engine encodes the ClientboundSetActionBarText payload identically to vanilla for the
// plain-text-literal Component domain.
//
// (Styled/translatable/keybind Components use the CompoundTag form, which is NOT ported on
// the C++ side and is NOT exercised here — the GT tool feeds plain literals only.)
//
//   pkt_set_actionbar_text_parity [--cases mcpp/build/pkt_set_actionbar_text.tsv]
//
// Row: ENC \t <name> \t <textHex(UTF-8)> \t <readableBytes> \t <wireHex>

#include "../../PacketBuffer.h"
#include "../../../nbt/Tag.h"

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
// Decode lowercase hex into the exact raw bytes (used for the UTF-8 text column).
std::string unhexStr(const std::string& s) {
    std::string out;
    out.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((char)(uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_set_actionbar_text.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0; int shown = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t textHex \t readableBytes \t wireHex
        std::istringstream ss(line);
        std::string kind, name, textHex, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, textHex, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        std::string text = unhexStr(textHex);   // exact UTF-8 bytes of the literal
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // ENCODE: the entire packet payload is the Component NBT root. For a plain
        // literal that is a single root StringTag, exactly as writeNbt(StringTag) emits.
        PacketBuffer enc;
        try {
            enc.writeNbt(mc::nbt::NbtTag::string_(text));
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
            // The root tag MUST be a StringTag (type 0x08) — the collapse-to-string form.
            if (enc.data().empty() || enc.data()[0] != 0x08) {
                std::cerr << "ROOTTAG-MISMATCH " << name << " leading byte not 0x08\n";
                ok = false;
            }
        }

        if (!ok) { ++mismatches; ++shown; }
        (void)shown;
    }

    std::cout << "PktSetActionBarText cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
