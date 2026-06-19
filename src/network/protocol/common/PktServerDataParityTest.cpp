// Parity gate for net.minecraft.network.protocol.game.ClientboundServerDataPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktServerDataParity.java GT).
//
// Verbatim from 26.1.2/src ClientboundServerDataPacket.java:
//   public record ClientboundServerDataPacket(Component motd, Optional<byte[]> iconBytes)
//   STREAM_CODEC = StreamCodec.composite(
//       ComponentSerialization.TRUSTED_CONTEXT_FREE_STREAM_CODEC, ::motd,
//       ByteBufCodecs.BYTE_ARRAY.apply(ByteBufCodecs::optional),  ::iconBytes, ::new);
//
// Wire (codec field order):
//   1) motd  : TRUSTED_CONTEXT_FREE_STREAM_CODEC = fromCodecTrusted(CODEC) (ByteBufCodecs
//              322-323 -> fromCodec 326+, NbtOps + tagCodec). A PLAIN literal collapses to
//              a root StringTag emitted by FriendlyByteBuf.writeNbt(Tag): 08 <u16 MUTF8 len>
//              <bytes>. We reproduce this with PacketBuffer.writeNbt(NbtTag::string_(text))
//              -> NbtWriter::writeAnyRoot (certified byte-exact by component_nbt_parity).
//   2) iconBytes : Optional<byte[]> = optional(BYTE_ARRAY) (ByteBufCodecs.optional 373-388):
//                    writeBoolean(present);
//                    if present: BYTE_ARRAY.encode = FriendlyByteBuf.writeByteArray
//                                (FriendlyByteBuf.java:289-292) = VarInt(len) + raw bytes.
//                  -> PacketBuffer.writeBool + writeVarInt(len) + writeBytes(raw).
//
// Only PLAIN-TEXT literal Components are gated (the collapse-to-StringTag domain). The
// packet carries no registry/ItemStack type, so this is fully portable today.
//
//   pkt_server_data_parity [--cases mcpp/build/pkt_server_data.tsv]
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

// "-" sentinel -> empty. Otherwise raw bytes from lowercase hex.
std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    if (s == "-") return out;
    out.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}

// hex of UTF-8 bytes -> the raw UTF-8 std::string (passed straight to NbtTag::string_,
// whose writer measures/encodes exactly like NbtIo MUTF-8). "-" -> "".
std::string fromHexBytes(const std::string& h) {
    std::string out;
    if (h == "-") return out;
    out.reserve(h.size() / 2);
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back((char)(uint8_t)std::stoi(h.substr(i, 2), nullptr, 16));
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_server_data.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0, shown = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tag;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;

        // ENC <name> <motdTextHex> <hasIcon> <iconHex> <readableBytes> <hexBytes>
        std::string name, motdHex, hasIconStr, iconHex, readableStr, expHex;
        if (!std::getline(ss, name, '\t') ||
            !std::getline(ss, motdHex, '\t') ||
            !std::getline(ss, hasIconStr, '\t') ||
            !std::getline(ss, iconHex, '\t') ||
            !std::getline(ss, readableStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        std::string motdText = fromHexBytes(motdHex);        // exact UTF-8 bytes
        bool hasIcon = std::stoi(hasIconStr) != 0;
        std::vector<uint8_t> icon = hasIcon ? unhex(iconHex) : std::vector<uint8_t>();
        int expReadable = std::stoi(readableStr);

        // --- encode: mirror the composite codec field order exactly ---
        PacketBuffer enc;
        // 1) motd: root StringTag (TRUSTED_CONTEXT_FREE_STREAM_CODEC collapse).
        enc.writeNbt(mc::nbt::NbtTag::string_(motdText));
        // 2) iconBytes: Optional<byte[]> = writeBool(present) [+ VarInt(len) + bytes].
        enc.writeBool(hasIcon);
        if (hasIcon) {
            enc.writeVarInt((int32_t)icon.size());           // writeByteArray length prefix
            enc.writeBytes(std::span<const uint8_t>(icon));  // raw icon bytes
        }

        std::string got = hex(enc.data());
        bool okBytes = got == expHex;
        bool okReadable = (int)enc.data().size() == expReadable;
        // The first byte must be the root StringTag id (0x08) — the Component collapse.
        bool okTag = !enc.data().empty() && enc.data()[0] == 0x08;

        if (!okBytes || !okReadable || !okTag) {
            ++mismatches;
            if (shown++ < 25) {
                std::cerr << "MISMATCH " << name << "\n  got  " << got
                          << " (" << enc.data().size() << "B, tag0="
                          << (enc.data().empty() ? -1 : (int)enc.data()[0]) << ")\n"
                          << "  want " << expHex << " (" << expReadable << "B)\n";
            }
        }
    }

    std::cout << "PktServerData cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
