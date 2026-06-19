// Parity gate for net.minecraft.network.protocol.game.ClientboundSystemChatPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktSystemChatParity.java ground truth).
//
// STREAM_CODEC (ClientboundSystemChatPacket lines 12-18) =
//   StreamCodec.composite(
//     ComponentSerialization.TRUSTED_STREAM_CODEC, ::content,   // Component content
//     ByteBufCodecs.BOOL,                          ::overlay,   // boolean overlay
//     ::new)
// composite() encodes in DECLARATION ORDER, so the body is exactly:
//   ComponentSerialization.TRUSTED_STREAM_CODEC.encode(buf, content)   // root NBT tag
//   ByteBufCodecs.BOOL.encode(buf, overlay)                            // 1 byte (writeBoolean)
//
// content is a BARE Component (NOT Optional) — no leading present-bool. TRUSTED_STREAM_CODEC
// (ComponentSerialization:40) = ByteBufCodecs.fromCodecWithRegistriesTrusted(CODEC): codec-
// encodes the Component to an NBT Tag, then FriendlyByteBuf.writeNbt(tag) (NbtIo.writeAnyTag:
// type byte + UNNAMED payload). For a PLAIN literal (no style, no siblings) it collapses to a
// bare StringTag: 08 <u16 MUTF8 len> <bytes>. overlay=BOOL (ByteBufCodecs:56-64) is a single
// boolean byte (00/01).
//
// We reproduce the wire with the engine: for each ENC row, decode the content text hex ->
// exact UTF-8 std::string, write it via PacketBuffer.writeNbt(NbtTag::string_(text)) (->
// NbtWriter::writeAnyRoot) in the SAME codec order, then writeBool(overlay), and require
// byte-identical output + matching readableBytes. This certifies the engine's root-tag NBT
// framing, MUTF-8 string encoding, and the packet's content+overlay field order.
//
//   pkt_system_chat_parity --cases mcpp/build/pkt_system_chat.tsv
//
// Row: ENC \t <name> \t <contentTextHex(UTF-8)> \t <overlay 0/1> \t <readableBytes> \t <hexBytes>
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
        std::string tag, name, textHex, overlayStr, readableStr, wireHex;
        std::getline(ss, tag, '\t');
        if (tag != "ENC") continue;
        std::getline(ss, name, '\t');
        std::getline(ss, textHex, '\t');
        std::getline(ss, overlayStr, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;

        std::string text = fromHexBytes(textHex);   // exact UTF-8 bytes of the literal
        bool overlay = (overlayStr == "1");
        size_t wantReadable = (size_t)std::stoul(readableStr);

        // Encode in codec declaration order: content (root StringTag NBT) then overlay bool.
        mc::net::PacketBuffer buf;
        buf.writeNbt(mc::nbt::NbtTag::string_(text));
        buf.writeBool(overlay);

        std::string got = hex(buf.data());

        // content must lead with the StringTag type byte 0x08; overlay byte is the last byte.
        bool ok = got == wireHex && buf.data().size() == wantReadable
                  && !buf.data().empty() && buf.data()[0] == 0x08
                  && buf.data().back() == (uint8_t)(overlay ? 1 : 0);
        if (!ok) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "MISMATCH " << name << " overlay=" << overlay
                          << " textHex=" << textHex << "\n  got  " << got
                          << " (" << buf.data().size() << "B)\n  want " << wireHex
                          << " (" << wantReadable << "B)\n";
        }
    }
    std::cout << "PktSystemChat cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
