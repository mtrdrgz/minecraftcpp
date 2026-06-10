// Parity gate for ClientboundUpdateMobEffectPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktUpdateMobEffectParity.java ground truth).
//
// STREAM_CODEC = Packet.codec(write, new) -> StreamCodec.ofMember: body only, no
// packet-id/length prefix. Body (ClientboundUpdateMobEffectPacket.write, lines 58-64):
//   output.writeVarInt(this.entityId)
//   MobEffect.STREAM_CODEC.encode(output, this.effect)   // Holder<MobEffect>
//   output.writeVarInt(this.effectAmplifier)
//   output.writeVarInt(this.effectDurationTicks)
//   output.writeByte(this.flags)
//
// MobEffect.STREAM_CODEC (net.minecraft.world.effect.MobEffect:41) =
//   ByteBufCodecs.holderRegistry(Registries.MOB_EFFECT)
// -> registry(key, Registry::asHolderIdMap), whose encode (ByteBufCodecs:573-576) is a
//    PLAIN VarInt of getIdOrThrow(value) — NO +1 (that +1 is the holder() reference form,
//    not holderRegistry). asHolderIdMap().getId == BuiltInRegistries.MOB_EFFECT.getId,
//    which is exactly the id stored in assets/network_registries.tsv for
//    minecraft:mob_effect. So we resolve the effect ns:path to its wire VarInt via
//    NetworkRegistries and emit it with NO offset.
//
// flags byte: AMBIENT=1, VISIBLE=2, SHOW_ICON=4, BLEND=8 (already folded into the column).
//
//   pkt_update_mob_effect_parity [--cases mcpp/build/pkt_update_mob_effect.tsv]
// Run from repo root so mcpp/src/assets/network_registries.tsv resolves.
#include "../../NetworkRegistries.h"
#include "../../PacketBuffer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::NetworkRegistries;
using mc::net::PacketBuffer;

namespace {
std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}

constexpr const char* MOB_EFFECT = "minecraft:mob_effect";
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_update_mob_effect.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    NetworkRegistries reg;
    const std::string regPath = "mcpp/src/assets/network_registries.tsv";
    if (!reg.loadFromFile(regPath)) {
        std::cerr << "FATAL cannot open " << regPath
                  << " (run from repo root)\n";
        return 2;
    }
    if (!reg.loadOrderOk()) {
        std::cerr << "FATAL network_registries.tsv not dense/in-order\n";
        return 2;
    }

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

        // ENC <entityId> <effectName ns:path> <amplifier> <duration> <flags> <len> <hex>
        std::string eidStr, effName, ampStr, durStr, flagsStr, lenStr, expHex;
        if (!std::getline(ss, eidStr, '\t') || !std::getline(ss, effName, '\t')
            || !std::getline(ss, ampStr, '\t') || !std::getline(ss, durStr, '\t')
            || !std::getline(ss, flagsStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t entityId  = (int32_t)std::stoll(eidStr);
        int32_t amplifier = (int32_t)std::stoll(ampStr);
        int32_t duration  = (int32_t)std::stoll(durStr);
        int32_t flags     = (int32_t)std::stoll(flagsStr);
        size_t  expLen    = (size_t)std::stoull(lenStr);

        // Resolve the effect ns:path -> its vanilla wire VarInt id. holderRegistry form:
        // plain getId, NO +1. FAIL loudly if the registry can't resolve it.
        auto effId = reg.id(MOB_EFFECT, effName);
        if (!effId) {
            ++mismatches;
            std::cerr << "REG-MISS effect=" << effName
                      << " not found in " << MOB_EFFECT << "\n";
            continue;
        }

        // write(): entityId, effectId, amplifier, duration (VarInts), then flags byte.
        PacketBuffer enc;
        enc.writeVarInt(entityId);
        enc.writeVarInt(*effId);
        enc.writeVarInt(amplifier);
        enc.writeVarInt(duration);
        enc.writeByte((uint8_t)(flags & 0xff));

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH eid=" << entityId << " eff=" << effName
                      << " (id=" << *effId << ") amp=" << amplifier << " dur=" << duration
                      << " flags=" << flags << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH eid=" << entityId << " eff=" << effName
                      << " got=" << enc.data().size() << " want=" << expLen << "\n";
        }

        // read(): recover every field from the expected bytes and verify, incl. resolving
        // the effect id back to its ns:path via the registry.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotEid = dec.readVarInt();
        int32_t gotEffId = dec.readVarInt();
        int32_t gotAmp = dec.readVarInt();
        int32_t gotDur = dec.readVarInt();
        uint8_t gotFlags = dec.readByte();
        auto gotName = reg.name(MOB_EFFECT, gotEffId);

        if (gotEid != entityId || gotEffId != *effId || !gotName || *gotName != effName
            || gotAmp != amplifier || gotDur != duration
            || (int32_t)gotFlags != flags) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex
                      << " got=(eid=" << gotEid << ",effId=" << gotEffId
                      << ",eff=" << (gotName ? *gotName : std::string("<none>"))
                      << ",amp=" << gotAmp << ",dur=" << gotDur
                      << ",flags=" << (int)gotFlags << ")"
                      << " want=(eid=" << entityId << ",effId=" << *effId
                      << ",eff=" << effName << ",amp=" << amplifier << ",dur=" << duration
                      << ",flags=" << flags << ")\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING eff=" << effName
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktUpdateMobEffectParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
