// Parity gate for net.minecraft.network.protocol.game.ClientboundRemoveMobEffectPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktRemoveMobEffectParity.java GT).
//
// Verbatim from 26.1.2/src ClientboundRemoveMobEffectPacket.java:14-21:
//   record ClientboundRemoveMobEffectPacket(int entityId, Holder<MobEffect> effect)
//   STREAM_CODEC = StreamCodec.composite(
//      ByteBufCodecs.VAR_INT,   ::entityId,
//      MobEffect.STREAM_CODEC,  ::effect,
//      ::new)
// So the wire body, in codec order, is:
//   (1) VarInt entityId                                  (ByteBufCodecs.VAR_INT)
//   (2) VarInt effectId                                  (MobEffect.STREAM_CODEC)
//       MobEffect.STREAM_CODEC = ByteBufCodecs.holderRegistry(Registries.MOB_EFFECT)
//                                                        (MobEffect.java:41)
//       holderRegistry == registry(key, Registry::asHolderIdMap)  (ByteBufCodecs.java:584-586)
//       registry(...).encode (ByteBufCodecs.java:573-576): VarInt(getIdOrThrow(value))
//       asHolderIdMap().getId(holder) = registry.getId(holder.value())  (Registry.java:145-149)
//       => a PLAIN VarInt of the MobEffect registry id -- NO +1 (this is registry()'s
//          plain-id form, NOT holder()'s direct/reference id+1 form).
//
// The registry-held effect id is NOT hard-coded: we resolve the GT-emitted entry name
// (ns:path) -> id via mc::net::NetworkRegistries over the COMMITTED
// mcpp/src/assets/network_registries.tsv table (network_registries_parity already gates
// that table byte-exact vs the jar). We FAIL the gate loudly if the name is absent or
// resolves to a different id than the GT effectId.
//
// Reuses the certified PacketBuffer (the FriendlyByteBuf port): writeVarInt is LEB128,
// byte-for-byte vs Java VarInt.write.
//
//   pkt_remove_mob_effect_parity [--cases mcpp/build/pkt_remove_mob_effect.tsv]
//                                [--asset mcpp/src/assets/network_registries.tsv]
#include "../../PacketBuffer.h"
#include "../../NetworkRegistries.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::PacketBuffer;
using mc::net::NetworkRegistries;

namespace {
const char* kMobEffectRegistry = "minecraft:mob_effect";

std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    if (s == "-") return out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_remove_mob_effect.tsv";
    std::string assetPath = "mcpp/src/assets/network_registries.tsv";
    for (int i = 1; i + 1 < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases") casesPath = argv[++i];
        else if (a == "--asset") assetPath = argv[++i];
    }

    NetworkRegistries reg;
    if (!reg.loadFromFile(assetPath)) {
        std::cerr << "cannot open network registries asset " << assetPath << "\n";
        return 2;
    }
    if (!reg.loadOrderOk()) {
        std::cerr << "network registries asset not densely ordered 0..count-1\n";
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

        // ENC <name> <entityId-dec> <effectName ns:path> <effectId-dec> <readableBytes> <hex>
        std::string caseName, entityStr, effectName, effectIdStr, readableStr, expHex;
        if (!std::getline(ss, caseName, '\t') ||
            !std::getline(ss, entityStr, '\t') ||
            !std::getline(ss, effectName, '\t') ||
            !std::getline(ss, effectIdStr, '\t') ||
            !std::getline(ss, readableStr, '\t') ||
            !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t entityId = (int32_t)std::stoll(entityStr);
        int32_t gtEffectId = (int32_t)std::stoll(effectIdStr);
        int expReadable = std::stoi(readableStr);

        // Resolve the registry-held effect name -> wire id via NetworkRegistries.
        // holderRegistry form => plain VarInt(id), NO +1. FAIL loudly if absent.
        auto resolved = reg.id(kMobEffectRegistry, effectName);
        if (!resolved.has_value()) {
            ++mismatches;
            std::cerr << "REGISTRY-MISS " << caseName << " effect '" << effectName
                      << "' not found in " << kMobEffectRegistry << "\n";
            continue;
        }
        int32_t effectId = *resolved;
        if (effectId != gtEffectId) {
            ++mismatches;
            std::cerr << "REGISTRY-ID-MISMATCH " << caseName << " effect '" << effectName
                      << "' resolved=" << effectId << " gt=" << gtEffectId << "\n";
            continue;
        }

        // --- encode: codec order entityId then effectId, both plain VarInt ---
        PacketBuffer enc;
        enc.writeVarInt(entityId);
        enc.writeVarInt(effectId);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << caseName << "\n  got  " << got
                      << "\n  want " << expHex << "\n";
        }
        if ((int)enc.data().size() != expReadable) {
            ++mismatches;
            std::cerr << "READABLE-MISMATCH " << caseName << " got="
                      << enc.data().size() << " want=" << expReadable << "\n";
        }

        // --- decode round-trip: parse expected bytes back via PacketBuffer ---
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t dEntityId = dec.readVarInt();
        int32_t dEffectId = dec.readVarInt();
        // reverse-resolve the wire id back to a name and require it matches the GT name.
        auto dName = reg.name(kMobEffectRegistry, dEffectId);

        bool rtOk = dEntityId == entityId && dEffectId == effectId &&
                    dName.has_value() && *dName == effectName &&
                    dec.remaining() == 0;
        if (!rtOk) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH " << caseName
                      << " ent=" << dEntityId << "/" << entityId
                      << " eff=" << dEffectId << "/" << effectId
                      << " name=" << (dName ? *dName : std::string("<none>"))
                      << "/" << effectName
                      << " rem=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktRemoveMobEffectParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
