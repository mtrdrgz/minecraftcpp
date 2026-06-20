// Parity gate for ClientboundSoundEntityPacket's StreamCodec vs the REAL net.minecraft
// codec (tools/PktSoundEntityParity.java ground truth).
//
// The packet carries a Holder<SoundEvent> + SoundSource + entity id + volume + pitch + seed.
// Its body is exactly (ClientboundSoundEntityPacket.java:43-50):
//   SoundEvent.STREAM_CODEC.encode(output, this.sound); // Holder<SoundEvent>
//   output.writeEnum(this.source);                      // SoundSource
//   output.writeVarInt(this.id);                        // int entity id
//   output.writeFloat(this.volume);                     // float (BE)
//   output.writeFloat(this.pitch);                      // float (BE)
//   output.writeLong(this.seed);                        // long (BE)
//
// SoundEvent.STREAM_CODEC = ByteBufCodecs.holder(Registries.SOUND_EVENT, ...) (SoundEvent
// .java:26). holder().encode for a REFERENCE (every vanilla sound) is, VERBATIM
// (ByteBufCodecs.java:603-613): VarInt.write(out, registry.getIdOrThrow(holder) + 1). So the
// on-wire sound field is a single VarInt = (SOUND_EVENT registry id) + 1. We resolve the id
// from the ns:path column via mc::net::NetworkRegistries (the certified wire-id table,
// network_registries_parity 2386/0) and FAIL the gate loudly if the entry is absent.
//
// writeEnum(e) = writeVarInt(e.ordinal()) (FriendlyByteBuf.java:471-473), so SoundSource is
// VarInt(ordinal). volume/pitch are IEEE-754 floats big-endian; seed is a big-endian long.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port): writeVarInt/writeFloat/
// writeLong (and the matching reads) are byte-for-byte / value-for-value the real codec.
//
//   pkt_sound_entity_parity [--cases mcpp/build/pkt_sound_entity.tsv]
//                           [--asset mcpp/src/assets/network_registries.tsv]
#include "../../PacketBuffer.h"
#include "../../NetworkRegistries.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::PacketBuffer;
using mc::net::NetworkRegistries;

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

float bitsToFloat(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
uint32_t floatToBits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_sound_entity.tsv";
    std::string assetPath = "src/assets/network_registries.tsv";
    for (int i = 1; i + 1 < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases") casesPath = argv[++i];
        else if (a == "--asset") assetPath = argv[++i];
    }

    NetworkRegistries reg;
    if (!reg.loadFromFile(assetPath)) {
        std::cerr << "cannot open registry asset " << assetPath << "\n";
        return 2;
    }
    if (!reg.loadOrderOk()) {
        std::cerr << "registry asset is not densely ordered 0..count-1\n";
        return 2;
    }

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    const std::string REG = "minecraft:sound_event";

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tag;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;

        // ENC <name> <soundId> <soundKey> <srcOrdinal> <entityId> <volBits-08x>
        //     <pitchBits-08x> <seed> <readableBytes> <hex>
        std::string nameStr, soundIdStr, soundKey, srcStr, entStr, volStr, pitchStr,
                    seedStr, lenStr, expHex;
        if (!std::getline(ss, nameStr, '\t') || !std::getline(ss, soundIdStr, '\t')
            || !std::getline(ss, soundKey, '\t') || !std::getline(ss, srcStr, '\t')
            || !std::getline(ss, entStr, '\t') || !std::getline(ss, volStr, '\t')
            || !std::getline(ss, pitchStr, '\t') || !std::getline(ss, seedStr, '\t')
            || !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t expSoundId = (int32_t)std::stoll(soundIdStr);
        int32_t srcOrdinal = (int32_t)std::stoll(srcStr);
        int32_t entityId   = (int32_t)std::stoll(entStr);
        uint32_t volBits   = (uint32_t)std::stoul(volStr, nullptr, 16);
        uint32_t pitchBits = (uint32_t)std::stoul(pitchStr, nullptr, 16);
        int64_t seed       = (int64_t)std::stoll(seedStr);
        size_t  expLen     = (size_t)std::stoull(lenStr);
        float volume = bitsToFloat(volBits);
        float pitch  = bitsToFloat(pitchBits);

        // Resolve the registry-held field via the certified wire-id table. FAIL loudly if
        // the ns:path entry is not present (a missing entry would silently break 1:1).
        auto soundIdOpt = reg.id(REG, soundKey);
        if (!soundIdOpt.has_value()) {
            ++mismatches;
            std::cerr << "REG-MISSING " << REG << " key=" << soundKey
                      << " (gt id " << expSoundId << ") — NetworkRegistries cannot resolve it\n";
            continue;
        }
        int32_t soundId = *soundIdOpt;
        if (soundId != expSoundId) {
            ++mismatches;
            std::cerr << "REG-ID-MISMATCH key=" << soundKey << " engine=" << soundId
                      << " gt=" << expSoundId << "\n";
            // keep going to still surface byte diffs
        }

        // write(): VarInt(soundId+1) [holder REFERENCE form] + VarInt(srcOrdinal) +
        //          VarInt(entityId) + float volume + float pitch + long seed.
        PacketBuffer enc;
        enc.writeVarInt(soundId + 1);
        enc.writeVarInt(srcOrdinal);
        enc.writeVarInt(entityId);
        enc.writeFloat(volume);
        enc.writeFloat(pitch);
        enc.writeLong(seed);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << nameStr << " key=" << soundKey
                      << " src=" << srcOrdinal << " ent=" << entityId
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH " << nameStr << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // read(): decode the expected bytes and recover every field, including the registry
        // id -> ns:path round-trip via NetworkRegistries.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotWireSound = dec.readVarInt();   // = soundId + 1
        int32_t gotSoundId   = gotWireSound - 1;
        int32_t gotSrc       = dec.readVarInt();
        int32_t gotEntity    = dec.readVarInt();
        float   gotVolume    = dec.readFloat();
        float   gotPitch     = dec.readFloat();
        int64_t gotSeed      = dec.readLong();

        auto gotName = reg.name(REG, gotSoundId);
        bool nameOk = gotName.has_value() && *gotName == soundKey;
        if (gotSoundId != expSoundId || !nameOk || gotSrc != srcOrdinal
            || gotEntity != entityId || floatToBits(gotVolume) != volBits
            || floatToBits(gotPitch) != pitchBits || gotSeed != seed) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH " << nameStr << " hex=" << expHex
                      << "\n  soundId got=" << gotSoundId << " want=" << expSoundId
                      << " name=" << (gotName ? *gotName : std::string("<none>"))
                      << " src=" << gotSrc << " ent=" << gotEntity
                      << " seed=" << gotSeed << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING " << nameStr << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktSoundEntityParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
