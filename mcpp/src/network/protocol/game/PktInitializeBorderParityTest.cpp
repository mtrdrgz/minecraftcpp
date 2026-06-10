// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundInitializeBorderPacket
// vs the REAL ClientboundInitializeBorderPacket.STREAM_CODEC (tools/PktInitializeBorderParity.java).
//
// The packet's STREAM_CODEC == Packet.codec(::write, ::new), so the wire format is exactly
// ClientboundInitializeBorderPacket.write(FriendlyByteBuf):
//
//   writeDouble(newCenterX)          // FLOAT64 big-endian
//   writeDouble(newCenterZ)          // FLOAT64 big-endian
//   writeDouble(oldSize)             // FLOAT64 big-endian
//   writeDouble(newSize)             // FLOAT64 big-endian
//   writeVarLong(lerpTime)           // VAR_LONG (LEB128)
//   writeVarInt(newAbsoluteMaxSize)  // VAR_INT  (LEB128)
//   writeVarInt(warningBlocks)       // VAR_INT  (LEB128)
//   writeVarInt(warningTime)         // VAR_INT  (LEB128)
//
// For every ENC row the C++ side rebuilds the eight fields, writes them via the certified
// mc::net::PacketBuffer (FriendlyByteBuf port) in that exact order, and must match the Java
// wire bytes byte-for-byte AND <readableBytes>. It then decodes the expected bytes back
// through PacketBuffer in the same READ order and requires every field to round-trip, with
// the buffer fully consumed.
//
//   pkt_initialize_border_parity [--cases mcpp/build/pkt_initialize_border.tsv]
//
// Row: ENC <name> <centerXBits-016x> <centerZBits-016x> <oldSizeBits-016x> <newSizeBits-016x>
//          <lerpTime-dec> <newAbsoluteMaxSize-dec> <warningBlocks-dec> <warningTime-dec>
//          <readableBytes-dec> <hex>
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
    std::string casesPath = "mcpp/build/pkt_initialize_border.tsv";
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
        std::string tag, name, cxStr, czStr, osStr, nsStr, lerpStr, amaxStr, wbStr, wtStr, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') ||
            !std::getline(ss, cxStr, '\t') || !std::getline(ss, czStr, '\t') ||
            !std::getline(ss, osStr, '\t') || !std::getline(ss, nsStr, '\t') ||
            !std::getline(ss, lerpStr, '\t') || !std::getline(ss, amaxStr, '\t') ||
            !std::getline(ss, wbStr, '\t') || !std::getline(ss, wtStr, '\t') ||
            !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        double centerX = bitsToDouble((uint64_t)std::stoull(cxStr, nullptr, 16));
        double centerZ = bitsToDouble((uint64_t)std::stoull(czStr, nullptr, 16));
        double oldSize = bitsToDouble((uint64_t)std::stoull(osStr, nullptr, 16));
        double newSize = bitsToDouble((uint64_t)std::stoull(nsStr, nullptr, 16));
        int64_t lerpTime           = (int64_t)std::stoll(lerpStr);
        int32_t newAbsoluteMaxSize = (int32_t)std::stoll(amaxStr);
        int32_t warningBlocks      = (int32_t)std::stoll(wbStr);
        int32_t warningTime        = (int32_t)std::stoll(wtStr);
        size_t expBytes            = (size_t)std::stoul(nStr);

        // (1) ENCODE: write the eight fields via PacketBuffer in codec order.
        PacketBuffer enc;
        enc.writeDouble(centerX);
        enc.writeDouble(centerZ);
        enc.writeDouble(oldSize);
        enc.writeDouble(newSize);
        enc.writeVarLong(lerpTime);
        enc.writeVarInt(newAbsoluteMaxSize);
        enc.writeVarInt(warningBlocks);
        enc.writeVarInt(warningTime);

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
        double  dCenterX = dec.readDouble();
        double  dCenterZ = dec.readDouble();
        double  dOldSize = dec.readDouble();
        double  dNewSize = dec.readDouble();
        int64_t dLerp    = dec.readVarLong();
        int32_t dAmax    = dec.readVarInt();
        int32_t dWb      = dec.readVarInt();
        int32_t dWt      = dec.readVarInt();

        bool ok =
            std::bit_cast<uint64_t>(dCenterX) == std::bit_cast<uint64_t>(centerX) &&
            std::bit_cast<uint64_t>(dCenterZ) == std::bit_cast<uint64_t>(centerZ) &&
            std::bit_cast<uint64_t>(dOldSize) == std::bit_cast<uint64_t>(oldSize) &&
            std::bit_cast<uint64_t>(dNewSize) == std::bit_cast<uint64_t>(newSize) &&
            dLerp == lerpTime &&
            dAmax == newAbsoluteMaxSize &&
            dWb   == warningBlocks &&
            dWt   == warningTime &&
            dec.remaining() == 0;

        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktInitializeBorderParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
