// Byte-exact parity gate for ClientboundContainerSetDataPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktContainerSetDataParity.java ground truth).
//
// Source (26.1.2/src/.../ClientboundContainerSetDataPacket.java):
//   write(buf): buf.writeContainerId(containerId);  // VarInt (FriendlyByteBuf -> VarInt.write)
//               buf.writeShort(id);                  // 2B big-endian, low 16 bits of int
//               buf.writeShort(value);               // 2B big-endian, low 16 bits of int
//   read(buf) : readContainerId() -> VarInt; readShort() -> id; readShort() -> value;
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body. The whole wire
// payload is VARINT(containerId) ++ SHORT(id) ++ SHORT(value).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly: writeVarInt /
// writeShort and readVarInt / readShort are byte-for-byte / value-for-value identical to the
// real codec (writeShort emits BE low 16 bits, readShort sign-extends).
//
//   pkt_container_set_data_parity [--cases mcpp/build/pkt_container_set_data.tsv]
//
// Row: ENC <name> <containerId-dec> <id-dec(signed short)> <value-dec(signed short)>
//          <readableBytes-dec> <hex>
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
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> out;
    out.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_container_set_data.tsv";
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

        // ENC <name> <containerId> <id> <value> <readableBytes> <hex>
        std::string name, cidStr, idStr, valStr, lenStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, cidStr, '\t')
            || !std::getline(ss, idStr, '\t') || !std::getline(ss, valStr, '\t')
            || !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t containerId = (int32_t)std::stoll(cidStr);
        int16_t id          = (int16_t)std::stoll(idStr);
        int16_t value       = (int16_t)std::stoll(valStr);
        size_t  expLen      = (size_t)std::stoull(lenStr);

        // (1) ENCODE: write the fields in the REAL wire order (VarInt cid, short id, short val).
        PacketBuffer enc;
        enc.writeVarInt(containerId);
        enc.writeShort(id);
        enc.writeShort(value);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH name=" << name << " cid=" << containerId
                      << " id=" << id << " value=" << value
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH name=" << name << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // (2) DECODE: read the Java bytes back and verify fields exactly + ordered.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotCid   = dec.readVarInt();
        int16_t gotId    = dec.readShort();
        int16_t gotValue = dec.readShort();

        if (gotCid != containerId) {
            ++mismatches;
            std::cerr << "DEC-CID-MISMATCH name=" << name << " hex=" << expHex
                      << " got=" << gotCid << " want=" << containerId << "\n";
        }
        if (gotId != id) {
            ++mismatches;
            std::cerr << "DEC-ID-MISMATCH name=" << name << " hex=" << expHex
                      << " got=" << gotId << " want=" << id << "\n";
        }
        if (gotValue != value) {
            ++mismatches;
            std::cerr << "DEC-VALUE-MISMATCH name=" << name << " hex=" << expHex
                      << " got=" << gotValue << " want=" << value << "\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING name=" << name << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktContainerSetDataParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
