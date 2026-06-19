// Parity gate for net.minecraft.network.protocol.game.ClientboundSetSubtitleTextPacket.
//
//   public record ClientboundSetSubtitleTextPacket(Component text) ...
//   STREAM_CODEC = StreamCodec.composite(
//       ComponentSerialization.TRUSTED_STREAM_CODEC, ClientboundSetSubtitleTextPacket::text,
//       ClientboundSetSubtitleTextPacket::new);
//
// The packet body is a single Component field encoded via ComponentSerialization.TRUSTED_STREAM_CODEC
// (NO Optional framing). For a plain-text literal (no style, no siblings) the codec collapses to a
// root NBT StringTag (08 <u16 MUTF8 len> <bytes>) written by FriendlyByteBuf.writeNbt(Tag). We
// reproduce the whole packet body with the engine: build nbt::NbtTag::string_(text) and emit it via
// mc::net::PacketBuffer::writeNbt(const NbtTag&), requiring byte-identical output to the real packet
// STREAM_CODEC + a leading 0x08 + readableBytes match.
//
//   tools/run_groundtruth.ps1 -Tool PktSetSubtitleTextParity -Out mcpp/build/pkt_set_subtitle_text.tsv
//   pkt_set_subtitle_text_parity --cases mcpp/build/pkt_set_subtitle_text.tsv
//
// Row: ENC \t <name> \t <textHex(UTF-8)> \t <readableBytes> \t <hexBytes>

#include "../../PacketBuffer.h"
#include "../../../nbt/Tag.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::string fromHexBytes(const std::string& h) {
    std::string out;
    out.reserve(h.size() / 2);
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back((char)(uint8_t)std::stoul(h.substr(i, 2), nullptr, 16));
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: --cases <tsv>\n"; return 2; }

    std::ifstream f(casesPath);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long cases = 0, mism = 0; int shown = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::stringstream ss(line);
        std::string tag, name, textHex, readableStr, wireHex;
        std::getline(ss, tag, '\t');
        if (tag != "ENC") continue;
        std::getline(ss, name, '\t');
        std::getline(ss, textHex, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;

        std::string text = fromHexBytes(textHex);   // exact UTF-8 bytes of the literal

        // Encode the packet body in the SAME codec order as the real STREAM_CODEC: a single
        // Component field via TRUSTED_STREAM_CODEC -> root StringTag.
        mc::net::PacketBuffer buf;
        buf.writeNbt(mc::nbt::NbtTag::string_(text));

        std::string got = hex(buf.data());
        size_t wantReadable = (size_t)std::stoul(readableStr);

        bool ok = got == wireHex && buf.data().size() == wantReadable
                  && !buf.data().empty() && buf.data()[0] == 0x08;
        if (!ok) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "MISMATCH name=" << name << " textHex=" << textHex
                          << "\n  got  " << got << " (" << buf.data().size() << "B)"
                          << "\n  want " << wireHex << " (" << wantReadable << "B)\n";
        }
    }
    std::cout << "PktSetSubtitleText cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
