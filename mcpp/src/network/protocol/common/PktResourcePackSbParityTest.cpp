// Parity gate for ServerboundResourcePackPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktResourcePackSbParity.java ground truth).
//
// The packet body is exactly (ServerboundResourcePackPacket.java lines 18-21):
//   write : FriendlyByteBuf.writeUUID(id);     -> writeLong(hiBE) + writeLong(loBE)
//           FriendlyByteBuf.writeEnum(action); -> writeVarInt(action.ordinal())
//   read  : id     = input.readUUID();              (2 BE longs: MSB then LSB)
//           action = input.readEnum(Action.class);  getEnumConstants()[readVarInt()]
// (FriendlyByteBuf.writeUUID 498-501 / writeEnum 471-473 / readEnum 467-469;
//  Packet.codec -> no packet-id prefix, just the body: 16 UUID bytes + a VarInt
//  ordinal. The STREAM_CODEC parameterises over plain FriendlyByteBuf.)
//
// Action declaration order (ServerboundResourcePackPacket.Action, lines 32-40):
//   SUCCESSFULLY_LOADED=0, DECLINED=1, FAILED_DOWNLOAD=2, ACCEPTED=3,
//   DOWNLOADED=4, INVALID_URL=5, FAILED_RELOAD=6, DISCARDED=7
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeUUID(hi,lo) emits two big-endian longs MSB-then-LSB, and writeVarInt is
// LEB128 — matching the real codec byte-for-byte.
//
//   pkt_resource_pack_sb_parity [--cases mcpp/build/pkt_resource_pack_sb.tsv]
#include "../../PacketBuffer.h"

#include <array>
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

// ServerboundResourcePackPacket.Action constants, in declaration order
// (== ordinal). Verbatim from the Java enum body lines 32-40.
const std::array<const char*, 8> kActionNames = {
    "SUCCESSFULLY_LOADED",  // ordinal 0
    "DECLINED",             // ordinal 1
    "FAILED_DOWNLOAD",      // ordinal 2
    "ACCEPTED",             // ordinal 3
    "DOWNLOADED",           // ordinal 4
    "INVALID_URL",          // ordinal 5
    "FAILED_RELOAD",        // ordinal 6
    "DISCARDED",            // ordinal 7
};

// Java longs are signed; reinterpret the decimal text as the same 64 bits.
uint64_t parseLongBits(const std::string& s) {
    return (uint64_t)(int64_t)std::stoll(s);
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_resource_pack_sb.tsv";
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

        if (tag == "ENUM") {
            // ENUM <ordinal> <name>
            std::string ordStr, name;
            if (!std::getline(ss, ordStr, '\t') || !std::getline(ss, name)) continue;
            ++cases;
            int ord = std::stoi(ordStr);
            if (ord < 0 || ord >= (int)kActionNames.size() ||
                name != kActionNames[(size_t)ord]) {
                ++mismatches;
                std::cerr << "ENUM-MISMATCH ord=" << ord << " got="
                          << (ord >= 0 && ord < (int)kActionNames.size()
                                  ? kActionNames[(size_t)ord] : "<oob>")
                          << " want=" << name << "\n";
            }
        } else if (tag == "ENC") {
            // ENC <case> <uuidHi> <uuidLo> <ordinal> <readableBytes> <hex>
            std::string cname, hiStr, loStr, ordStr, lenStr, expHex;
            if (!std::getline(ss, cname, '\t') || !std::getline(ss, hiStr, '\t') ||
                !std::getline(ss, loStr, '\t') || !std::getline(ss, ordStr, '\t') ||
                !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            uint64_t hi = parseLongBits(hiStr);
            uint64_t lo = parseLongBits(loStr);
            int32_t ord = (int32_t)std::stoll(ordStr);
            size_t expLen = (size_t)std::stoull(lenStr);

            // write(): writeUUID(id) -> writeLong(hi)+writeLong(lo) (BE, MSB then LSB),
            //          writeEnum(action) -> writeVarInt(ordinal).
            PacketBuffer enc;
            enc.writeUUID(hi, lo);
            enc.writeVarInt(ord);
            std::string got = hex(enc.data());
            if (got != expHex || enc.data().size() != expLen) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH case=" << cname
                          << "\n  got  " << got << " (len " << enc.data().size() << ")"
                          << "\n  want " << expHex << " (len " << expLen << ")\n";
            }
        } else if (tag == "DEC") {
            // DEC <hex> <uuidHi_out> <uuidLo_out> <ordinal_out>
            std::string inHex, hiStr, loStr, ordStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, hiStr, '\t') ||
                !std::getline(ss, loStr, '\t') || !std::getline(ss, ordStr)) continue;
            ++cases;
            uint64_t expHi = parseLongBits(hiStr);
            uint64_t expLo = parseLongBits(loStr);
            int32_t expOrd = (int32_t)std::stoll(ordStr);

            // read(): readUUID() (2 BE longs MSB then LSB), readEnum -> readVarInt().
            PacketBuffer dec(unhex(inHex));
            uint64_t gotHi = 0, gotLo = 0;
            dec.readUUID(gotHi, gotLo);
            int32_t gotOrd = dec.readVarInt();
            if (gotHi != expHi || gotLo != expLo || gotOrd != expOrd) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " got hi=" << (int64_t)gotHi << " lo=" << (int64_t)gotLo
                          << " ord=" << gotOrd
                          << " want hi=" << (int64_t)expHi << " lo=" << (int64_t)expLo
                          << " ord=" << expOrd << "\n";
            }
        }
    }

    std::cout << "PktResourcePackSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
