// Parity gate for the PLAY-protocol plain-text Component wire form. A literal Component
// (no style, no siblings) serializes via ComponentSerialization.TRUSTED_STREAM_CODEC to a
// root NBT StringTag (08 <u16 MUTF8 len> <bytes>) written by FriendlyByteBuf.writeNbt(Tag).
// We reproduce that with the engine: build nbt::NbtTag::string_(text) and emit it via
// mc::net::PacketBuffer::writeNbt(const NbtTag&) (-> NbtWriter::writeAnyRoot), requiring
// byte-identical output to the real codec + a leading 0x08. This certifies the engine's
// root-tag NBT framing and MUTF-8 string encoding for the common Component case.
//
//   tools/run_groundtruth.ps1 -Tool ComponentNbtParity -Out mcpp/build/component_nbt.tsv
//   component_nbt_parity --cases mcpp/build/component_nbt.tsv
//
// Row: NBT \t <textHex(UTF-8)> \t <readableBytes> \t <wireHex>

#include "PacketBuffer.h"
#include "../nbt/Tag.h"

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
        if (tag != "NBT") continue;
        std::getline(ss, textHex, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;

        std::string text = fromHexBytes(textHex);   // exact UTF-8 bytes of the literal
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
    std::cout << "ComponentNbt cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
