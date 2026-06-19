// Parity gate for ClientboundPlayerCombatKillPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktPlayerCombatKillParity.java ground truth).
//
// net.minecraft.network.protocol.game.ClientboundPlayerCombatKillPacket
// (26.1.2/src/.../ClientboundPlayerCombatKillPacket.java lines 11-18):
//
//   public record ClientboundPlayerCombatKillPacket(int playerId, Component message) ... {
//      public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundPlayerCombatKillPacket>
//          STREAM_CODEC = StreamCodec.composite(
//             ByteBufCodecs.VAR_INT,                          ::playerId
//             ComponentSerialization.TRUSTED_STREAM_CODEC,    ::message
//             ::new);
//   }
//
// So the wire body is EXACTLY: VarInt(playerId) ++ Component(message), in that order.
//   - ByteBufCodecs.VAR_INT  -> VarInt.write (ByteBufCodecs.java 102-110) == PacketBuffer::writeVarInt.
//   - ComponentSerialization.TRUSTED_STREAM_CODEC (ComponentSerialization.java line 40) codec-encodes
//     the Component to NBT then FriendlyByteBuf.writeNbt(tag). For a PLAIN literal (no style, no
//     siblings) the codec collapses to a root StringTag: 08 <u16 MUTF8 len> <bytes>. This is the
//     NON-optional TRUSTED variant — there is NO Optional/boolean framing before the NBT.
//
// We RESTRICT message to PLAIN-TEXT literal Components (Component.literal(text)) — the
// collapse-to-StringTag domain. We reproduce the wire with the engine: writeVarInt(playerId)
// then writeNbt(nbt::NbtTag::string_(text)) via mc::net::PacketBuffer, requiring byte-identical
// output to the real codec AND matching readableBytes. (Round-trip NBT decode is harder; we
// assert the encode bytes match, which is the load-bearing certification.)
//
//   pkt_player_combat_kill_parity [--cases mcpp/build/pkt_player_combat_kill.tsv]
//
// Row: ENC <name> <playerId-dec> <msgTextHex(UTF-8)> <readableBytes-dec> <hexBytes>
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

std::string fromHexBytes(const std::string& h) {
    std::string out;
    out.reserve(h.size() / 2);
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back((char)(uint8_t)std::stoul(h.substr(i, 2), nullptr, 16));
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_player_combat_kill.tsv";
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

        // ENC <name> <playerId> <msgTextHex> <readableBytes> <hexBytes>
        std::string name, idStr, textHex, readableStr, wireHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, idStr, '\t')
            || !std::getline(ss, textHex, '\t') || !std::getline(ss, readableStr, '\t')
            || !std::getline(ss, wireHex, '\t')) continue;
        ++cases;

        int32_t playerId = (int32_t)std::stol(idStr);
        std::string text = fromHexBytes(textHex);   // exact UTF-8 bytes of the literal
        size_t wantReadable = (size_t)std::stoul(readableStr);

        // Reproduce the codec order: VarInt(playerId) then the Component NBT (root StringTag).
        PacketBuffer buf;
        buf.writeVarInt(playerId);
        buf.writeNbt(mc::nbt::NbtTag::string_(text));
        std::string got = hex(buf.data());

        bool ok = got == wireHex && buf.data().size() == wantReadable;
        if (!ok) {
            ++mismatches;
            if (shown++ < 25)
                std::cerr << "ENC-MISMATCH name=" << name << " playerId=" << playerId
                          << " textHex=" << textHex << "\n  got  " << got
                          << " (" << buf.data().size() << "B)\n  want " << wireHex
                          << " (" << wantReadable << "B)\n";
        }
    }

    std::cout << "PktPlayerCombatKillParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
