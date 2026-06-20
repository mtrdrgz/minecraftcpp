// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundSetBorderLerpSizePacket
// vs the REAL ClientboundSetBorderLerpSizePacket.STREAM_CODEC (tools/PktSetBorderLerpSizeParity.java).
//
// The packet's STREAM_CODEC == Packet.codec(::write, ::new), so the wire format is exactly
// ClientboundSetBorderLerpSizePacket.write(FriendlyByteBuf):
//
//   writeDouble(oldSize)    // FLOAT64 big-endian
//   writeDouble(newSize)    // FLOAT64 big-endian
//   writeVarLong(lerpTime)  // VAR_LONG (LEB128)
//
// For every ENC row the C++ side rebuilds the three fields, writes them via the certified
// mc::net::PacketBuffer (FriendlyByteBuf port) in that exact order, and must match the Java
// wire bytes byte-for-byte AND <readableBytes>. It then decodes the expected bytes back
// through PacketBuffer in the same READ order and requires every field to round-trip, with
// the buffer fully consumed.
//
//   pkt_set_border_lerp_size_parity [--cases mcpp/build/pkt_set_border_lerp_size.tsv]
//
// Row: ENC <name> <oldSizeBits-016x> <newSizeBits-016x> <lerpTime-dec> <readableBytes-dec> <hex>
#include "../../PacketBuffer.h"

#include <bit>
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

double bitsToDouble(uint64_t bits) { return std::bit_cast<double>(bits); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_set_border_lerp_size.tsv";
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
        std::string tag, name, osStr, nsStr, lerpStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') ||
            !std::getline(ss, osStr, '\t') || !std::getline(ss, nsStr, '\t') ||
            !std::getline(ss, lerpStr, '\t') ||
            !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        double  oldSize  = bitsToDouble((uint64_t)std::stoull(osStr, nullptr, 16));
        double  newSize  = bitsToDouble((uint64_t)std::stoull(nsStr, nullptr, 16));
        int64_t lerpTime = (int64_t)std::stoll(lerpStr);
        size_t  expBytes = (size_t)std::stoul(nStr);

        // (1) ENCODE: write the three fields via PacketBuffer in codec order.
        PacketBuffer enc;
        enc.writeDouble(oldSize);
        enc.writeDouble(newSize);
        enc.writeVarLong(lerpTime);

        std::string got = hex(enc.data());
        if (got != expHex || enc.size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name
                      << "\n  got  (" << enc.size() << ") " << got
                      << "\n  want (" << expBytes << ") " << expHex << "\n";
            continue;
        }

        // (2) DECODE: read the expected bytes back in the same READ order.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        double  dOldSize = dec.readDouble();
        double  dNewSize = dec.readDouble();
        int64_t dLerp    = dec.readVarLong();

        bool ok =
            std::bit_cast<uint64_t>(dOldSize) == std::bit_cast<uint64_t>(oldSize) &&
            std::bit_cast<uint64_t>(dNewSize) == std::bit_cast<uint64_t>(newSize) &&
            dLerp == lerpTime &&
            dec.remaining() == 0;

        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktSetBorderLerpSizeParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
