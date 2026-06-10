// Parity gate for net.minecraft.network.protocol.game.ClientboundSetTitleTextPacket.
//
// The packet is `record ClientboundSetTitleTextPacket(Component text)` with
//   STREAM_CODEC = StreamCodec.composite(ComponentSerialization.TRUSTED_STREAM_CODEC,
//                      ClientboundSetTitleTextPacket::text, ClientboundSetTitleTextPacket::new)
// so the entire wire payload is a single Component encoded via TRUSTED_STREAM_CODEC. For a
// PLAIN literal Component (no style, no siblings) that collapses to a root NBT StringTag
// (08 <u16 MUTF8 len> <bytes>) written by FriendlyByteBuf.writeNbt(Tag). There is NO Optional
// framing and NO other field. We reproduce the wire payload with the engine: build
// nbt::NbtTag::string_(text) and emit it via mc::net::PacketBuffer::writeNbt(const NbtTag&)
// (-> NbtWriter::writeAnyRoot), requiring byte-identical output to the real packet codec +
// matching readableBytes + a leading 0x08 (StringTag id).
//
//   tools/run_groundtruth.ps1 -Tool PktSetTitleTextParity -Out mcpp/build/pkt_set_title_text.tsv
//   pkt_set_title_text_parity --cases mcpp/build/pkt_set_title_text.tsv
//
// Row: ENC \t <textHex(UTF-8)> \t <readableBytes> \t <wireHex>

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
        std::string tag, textHex, readableStr, wireHex;
        std::getline(ss, tag, '\t');
        if (tag != "ENC") continue;
        std::getline(ss, textHex, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;

        std::string text = fromHexBytes(textHex);   // exact UTF-8 bytes of the literal

        // The whole packet payload IS the single Component, encoded as a root StringTag.
        mc::net::PacketBuffer buf;
        buf.writeNbt(mc::nbt::NbtTag::string_(text));
        std::string got = hex(buf.data());
        size_t wantReadable = (size_t)std::stoul(readableStr);

        bool ok = got == wireHex && buf.data().size() == wantReadable
                  && !buf.data().empty() && buf.data()[0] == 0x08;
        if (!ok) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "MISMATCH textHex=" << textHex << "\n  got  " << got
                          << " (" << buf.data().size() << "B)\n  want " << wireHex
                          << " (" << wantReadable << "B)\n";
        }
    }
    std::cout << "PktSetTitleText cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
