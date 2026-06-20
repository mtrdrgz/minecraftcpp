// Byte-exact parity gate for net.minecraft.network.protocol.game.ClientboundGameTestHighlightPosPacket
// vs the REAL ClientboundGameTestHighlightPosPacket.STREAM_CODEC (tools/PktGameTestHighlightPosParity.java).
//
// The packet is a record(BlockPos absolutePos, BlockPos relativePos). Its STREAM_CODEC,
// VERBATIM (ClientboundGameTestHighlightPosPacket.java:11-17), is:
//   StreamCodec.composite(
//      BlockPos.STREAM_CODEC, ::absolutePos,
//      BlockPos.STREAM_CODEC, ::relativePos,
//      ::new)
// typed StreamCodec<ByteBuf, ...> (a PLAIN io.netty ByteBuf, NOT a
// RegistryFriendlyByteBuf): there is NO Holder / ResourceLocation / ItemStack /
// Component / NBT / registry id on the wire. The body, field-by-field in codec order:
//   BlockPos.STREAM_CODEC.encode(absolutePos) -> writeBlockPos(pos) -> writeLong(pos.asLong())
//        big-endian 8-byte long (BlockPos.java:39-46, FriendlyByteBuf.java:398-400)
//   BlockPos.STREAM_CODEC.encode(relativePos) -> writeLong(relativePos.asLong())
//        a second big-endian 8-byte long.
//
// So the whole payload is exactly two 8-byte big-endian longs (16 bytes), and the
// certified PacketBuffer (the FriendlyByteBuf port) rebuilds it directly:
//   writeLong(absLong) + writeLong(relLong)
//
// For each ENC row the C++ encode must reproduce the Java wire bytes byte-for-byte
// and the same readableBytes; decode(Java bytes) must then recover absLong/relLong.
//
//   pkt_game_test_highlight_pos_parity [--cases mcpp/build/pkt_game_test_highlight_pos.tsv]
//
// Row: ENC <name> <absLong-dec> <relLong-dec> <readableBytes> <hex>
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

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_game_test_highlight_pos.tsv";
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
        std::string tag, name, absStr, relStr, lenStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, absStr, '\t') ||
            !std::getline(ss, relStr, '\t') || !std::getline(ss, lenStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int64_t absLong = (int64_t)std::stoll(absStr);
        int64_t relLong = (int64_t)std::stoll(relStr);
        size_t readableBytes = (size_t)std::stoul(lenStr);

        // (1) ENCODE: write the body in the same codec order via PacketBuffer.
        PacketBuffer enc;
        enc.writeLong(absLong);                       // writeBlockPos(absolutePos) == writeLong(asLong)
        enc.writeLong(relLong);                       // writeBlockPos(relativePos) == writeLong(asLong)

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != readableBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " abs=" << absLong
                      << " rel=" << relLong
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
        int64_t gotAbs = dec.readLong();
        int64_t gotRel = dec.readLong();

        bool ok = (gotAbs == absLong) && (gotRel == relLong) && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name << " abs=" << absLong
                      << " (got " << gotAbs << ") rel=" << relLong << " (got " << gotRel << ")"
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktGameTestHighlightPosParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
