// Parity gate for ServerboundTeleportToEntityPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktTeleportToEntitySbParity.java ground truth).
//
// The packet carries a single UUID `uuid`. Its body is exactly:
//   write : FriendlyByteBuf.writeUUID(this.uuid)
//   read  : input.readUUID()
// (net.minecraft.network.protocol.game.ServerboundTeleportToEntityPacket lines 13-28.)
// FriendlyByteBuf.writeUUID = writeLong(MSB) ++ writeLong(LSB), both big-endian 8-byte
// longs (FriendlyByteBuf.java:498-501). readUUID = new UUID(readLong(), readLong()).
// Packet.codec -> StreamCodec.ofMember: body only, no id/length prefix, so the whole wire
// payload is exactly 16 bytes = MSB(BE) ++ LSB(BE).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeUUID(hi,lo) emits writeLong(hi)++writeLong(lo) and readUUID recovers them, which is
// byte-for-byte / value-for-value the same as the real codec.
//
//   pkt_spectate_entity_sb_parity [--cases mcpp/build/pkt_spectate_entity_sb.tsv]
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

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_spectate_entity_sb.tsv";
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

        // ENC <name> <msb-dec-signed> <lsb-dec-signed> <readableBytes-dec> <hex>
        std::string name, msbStr, lsbStr, lenStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, msbStr, '\t')
            || !std::getline(ss, lsbStr, '\t') || !std::getline(ss, lenStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;

        // Parse the signed decimal longs into the exact 64-bit bit patterns. stoll handles
        // the full signed range incl. LONG_MIN; we reinterpret to unsigned for writeUUID.
        int64_t msbS = (int64_t)std::stoll(msbStr);
        int64_t lsbS = (int64_t)std::stoll(lsbStr);
        uint64_t hi = (uint64_t)msbS;
        uint64_t lo = (uint64_t)lsbS;
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): FriendlyByteBuf.writeUUID(uuid) == writeLong(MSB) ++ writeLong(LSB) BE.
        PacketBuffer enc;
        enc.writeUUID(hi, lo);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " msb=" << msbStr << " lsb=" << lsbStr
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH " << name << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // read(): input.readUUID() must recover the exact (MSB,LSB) bit patterns.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        uint64_t gotHi = 0, gotLo = 0;
        dec.readUUID(gotHi, gotLo);
        if (gotHi != hi || gotLo != lo) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH " << name << " hex=" << expHex
                      << " got=(" << (int64_t)gotHi << "," << (int64_t)gotLo << ")"
                      << " want=(" << msbStr << "," << lsbStr << ")\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING " << name << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktTeleportToEntitySbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
