// Parity gate for ClientboundPlayerCombatEnterPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktPlayerCombatEnterParity.java ground truth).
//
// net.minecraft.network.protocol.game.ClientboundPlayerCombatEnterPacket:
//
//   public static final ClientboundPlayerCombatEnterPacket INSTANCE = ...;
//   public static final StreamCodec<ByteBuf, ClientboundPlayerCombatEnterPacket>
//       STREAM_CODEC = StreamCodec.unit(INSTANCE);
//
// StreamCodec.unit(instance).encode writes NOTHING (it only asserts the singleton)
// and decode returns the singleton consuming zero bytes (StreamCodec.java 49-63).
// So the on-wire body is EXACTLY zero bytes — there are no fields of any kind (no
// registry-held types, no ItemStack, no Component, no NBT, no Holder). Nothing for
// the C++ PacketBuffer to build.
//
// This gate mirrors that: a fresh, empty mc::net::PacketBuffer (nothing written) must
// have size()==0 and hex=="", matching the expected zero-byte encoding emitted by the
// real codec. The "decode" side is symmetric: feeding zero bytes consumes zero bytes
// and recovers the (implicit) singleton — there is nothing to read back.
//
//   pkt_player_combat_enter_parity [--cases mcpp/build/pkt_player_combat_enter.tsv]
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
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_player_combat_enter.tsv";
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
        if (tag != "ENC") continue;

        // ENC <name> <readableBytes-dec> <hex(empty)> <isInstance-dec>
        std::string name, lenStr, expHex, instStr;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex, '\t') || !std::getline(ss, instStr)) continue;
        ++cases;

        size_t expLen = (size_t)std::stoull(lenStr);
        int isInstance = std::stoi(instStr);

        // The unit packet writes nothing: a fresh PacketBuffer must stay empty.
        PacketBuffer enc;
        std::string got = hex(enc.data());

        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH name=" << name << "\n  got  '" << got
                      << "'\n  want '" << expHex << "'\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH name=" << name << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }
        // Ground truth must confirm the decode yielded the singleton, and the body must
        // be empty — both encode bytes and expected length are zero for a unit packet.
        if (expLen != 0 || !expHex.empty()) {
            ++mismatches;
            std::cerr << "NONEMPTY-BODY name=" << name << " len=" << expLen
                      << " hex='" << expHex << "' (unit packet must be zero bytes)\n";
        }
        if (isInstance != 1) {
            ++mismatches;
            std::cerr << "NOT-INSTANCE name=" << name
                      << " (codec.decode did not recover the singleton)\n";
        }

        // Decode side: feeding the (empty) expected body consumes zero bytes and leaves
        // nothing to read — symmetric with StreamCodec.unit.decode.
        PacketBuffer dec;  // empty body
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING name=" << name << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktPlayerCombatEnterParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
