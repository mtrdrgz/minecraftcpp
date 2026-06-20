// Parity gate for net.minecraft.network.protocol.game.ClientboundSoundPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktSoundParity.java GT).
//
// Verbatim wire form, from 26.1.2/src ClientboundSoundPacket.java (write 56-65) +
// SoundEvent.java:26 + ByteBufCodecs.java holder(...) 588-614 + FriendlyByteBuf 471-473:
//
//   SoundEvent.STREAM_CODEC.encode(out, sound):
//       sound is a *registered REFERENCE* holder -> case REFERENCE:
//         id = BuiltInRegistries.SOUND_EVENT.getId(value);  VarInt.write(out, id + 1)
//       (the DIRECT/inline form, VarInt(0)+value, is never used here: ClientboundSoundPacket
//        only ever carries registered sounds. We therefore encode VarInt(id+1).)
//   out.writeEnum(source) = writeVarInt(source.ordinal())   [MASTER=0..UI=10]
//   out.writeInt(x); out.writeInt(y); out.writeInt(z)        BE int, each = (int)(coord*8.0)
//   out.writeFloat(volume); out.writeFloat(pitch)            BE float
//   out.writeLong(seed)                                      BE long
//
// The registry id comes from mc::net::NetworkRegistries (assets/network_registries.tsv,
// gated byte-exact by network_registries_parity): reg.id("minecraft:sound_event", name)
// == BuiltInRegistries.SOUND_EVENT.getId(value). We FAIL loudly if a name is absent.
//
// Reuses the certified PacketBuffer (FriendlyByteBuf port): writeVarInt is LEB128,
// writeInt/Long/Float are big-endian — all byte-for-byte vs Java.
//
//   pkt_sound_parity [--cases mcpp/build/pkt_sound.tsv]
// Run from the repo ROOT so mcpp/src/assets/network_registries.tsv resolves.
#include "../../PacketBuffer.h"
#include "../../NetworkRegistries.h"

#include <array>
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
const char* kSoundRegistry = "minecraft:sound_event";

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

// float bits (%08x in the TSV) -> the IEEE-754 float passed to writeFloat (BE).
float floatFromBits(const std::string& h) {
    uint32_t u = (uint32_t)std::stoul(h, nullptr, 16);
    float f; std::memcpy(&f, &u, 4); return f;
}

// SoundSource constants in declaration order (== ordinal), verbatim from
// SoundSource.java lines 4-14.
const std::array<const char*, 11> kSoundSourceNames = {
    "MASTER",   // 0
    "MUSIC",    // 1
    "RECORDS",  // 2
    "WEATHER",  // 3
    "BLOCKS",   // 4
    "HOSTILE",  // 5
    "NEUTRAL",  // 6
    "PLAYERS",  // 7
    "AMBIENT",  // 8
    "VOICE",    // 9
    "UI",       // 10
};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_sound.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    NetworkRegistries reg;
    if (!reg.loadFromFile("src/assets/network_registries.tsv")) {
        std::cerr << "FATAL: cannot load mcpp/src/assets/network_registries.tsv "
                     "(run from repo root)\n";
        return 2;
    }
    if (!reg.loadOrderOk()) {
        std::cerr << "FATAL: network_registries.tsv not dense/in-order\n";
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

        if (tag == "ENUM") {
            // ENUM <ordinal> <name>
            std::string ordStr, name;
            if (!std::getline(ss, ordStr, '\t') || !std::getline(ss, name)) continue;
            ++cases;
            int ord = std::stoi(ordStr);
            if (ord < 0 || ord >= (int)kSoundSourceNames.size() ||
                name != kSoundSourceNames[(size_t)ord]) {
                ++mismatches;
                std::cerr << "ENUM-MISMATCH ord=" << ord << " got="
                          << (ord >= 0 && ord < (int)kSoundSourceNames.size()
                                  ? kSoundSourceNames[(size_t)ord] : "<oob>")
                          << " want=" << name << "\n";
            }
        } else if (tag == "ENC") {
            // ENC <case> <soundName> <soundWireId> <srcOrdinal> <xi> <yi> <zi>
            //     <volHex> <pitchHex> <seedDec> <readableBytes> <hexBytes>
            std::string caseName, soundName, soundWireStr, srcOrdStr, xiStr, yiStr,
                        ziStr, volHex, pitchHex, seedStr, readableStr, expHex;
            if (!std::getline(ss, caseName, '\t') ||
                !std::getline(ss, soundName, '\t') ||
                !std::getline(ss, soundWireStr, '\t') ||
                !std::getline(ss, srcOrdStr, '\t') ||
                !std::getline(ss, xiStr, '\t') ||
                !std::getline(ss, yiStr, '\t') ||
                !std::getline(ss, ziStr, '\t') ||
                !std::getline(ss, volHex, '\t') ||
                !std::getline(ss, pitchHex, '\t') ||
                !std::getline(ss, seedStr, '\t') ||
                !std::getline(ss, readableStr, '\t') ||
                !std::getline(ss, expHex)) continue;
            ++cases;

            int32_t soundWireGt = (int32_t)std::stol(soundWireStr);
            int32_t srcOrdinal  = (int32_t)std::stol(srcOrdStr);
            int32_t xi          = (int32_t)std::stol(xiStr);
            int32_t yi          = (int32_t)std::stol(yiStr);
            int32_t zi          = (int32_t)std::stol(ziStr);
            float   volume      = floatFromBits(volHex);
            float   pitch       = floatFromBits(pitchHex);
            int64_t seed        = (int64_t)std::stoll(seedStr);
            int     expReadable = std::stoi(readableStr);

            // Resolve the registry-held sound name -> wire id. FAIL the gate loudly
            // if absent; cross-check it equals the GT's BuiltInRegistries id.
            auto idOpt = reg.id(kSoundRegistry, soundName);
            if (!idOpt) {
                ++mismatches;
                std::cerr << "REGISTRY-MISS " << caseName << " sound='" << soundName
                          << "' not in " << kSoundRegistry << "\n";
                continue;
            }
            int32_t soundId = *idOpt;
            if (soundId != soundWireGt) {
                ++mismatches;
                std::cerr << "REGISTRY-ID-MISMATCH " << caseName << " sound='"
                          << soundName << "' got=" << soundId
                          << " wantGT=" << soundWireGt << "\n";
            }

            // --- encode: mirror the codec order exactly ---
            // holder() REFERENCE form -> VarInt(id + 1); then enum/ints/floats/long.
            PacketBuffer enc;
            enc.writeVarInt(soundId + 1);  // SoundEvent.STREAM_CODEC, REFERENCE: getId+1
            enc.writeVarInt(srcOrdinal);   // writeEnum(source) == VarInt(ordinal)
            enc.writeInt(xi);              // BE int
            enc.writeInt(yi);
            enc.writeInt(zi);
            enc.writeFloat(volume);        // BE float
            enc.writeFloat(pitch);
            enc.writeLong(seed);           // BE long

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
            int32_t dHolderId = dec.readVarInt();   // id + 1 (REFERENCE)
            int32_t dSoundId  = dHolderId - 1;       // strip the +1
            int32_t dSrc      = dec.readVarInt();
            int32_t dx        = dec.readInt();
            int32_t dy        = dec.readInt();
            int32_t dz        = dec.readInt();
            float   dVol      = dec.readFloat();
            float   dPitch    = dec.readFloat();
            int64_t dSeed     = dec.readLong();

            // Resolve the decoded id back to a name and require it round-trips.
            auto nameBack = reg.name(kSoundRegistry, dSoundId);
            bool rtOk = nameBack.has_value() && *nameBack == soundName &&
                        dSoundId == soundId &&
                        dSrc == srcOrdinal &&
                        dx == xi && dy == yi && dz == zi &&
                        std::memcmp(&dVol, &volume, 4) == 0 &&
                        std::memcmp(&dPitch, &pitch, 4) == 0 &&
                        dSeed == seed &&
                        dec.remaining() == 0;
            if (!rtOk) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH " << caseName
                          << " id=" << dSoundId << "/" << soundId
                          << " name=" << (nameBack ? *nameBack : "<none>") << "/" << soundName
                          << " src=" << dSrc << "/" << srcOrdinal
                          << " xyz=" << dx << "," << dy << "," << dz
                          << " seed=" << dSeed << "/" << seed
                          << " rem=" << dec.remaining() << "\n";
            }
        }
    }

    std::cout << "PktSoundParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
