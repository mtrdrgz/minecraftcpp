// Parity gate for ClientboundRemoveEntitiesPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktRemoveEntitiesParity.java ground truth).
//
// The packet body is exactly FriendlyByteBuf.writeIntIdList(entityIds):
//   write : writeVarInt(ids.size()); ids.forEach(this::writeVarInt);
//   read  : int count = readVarInt(); loop count * readVarInt();
// (net.minecraft.network.protocol.game.ClientboundRemoveEntitiesPacket lines 24-30,
//  FriendlyByteBuf.readIntIdList/writeIntIdList lines 146-160; Packet.codec ->
//  StreamCodec.ofMember: no packet-id prefix, just the body.)
//
// Every field is a VarInt, so this reuses the certified PacketBuffer (the
// FriendlyByteBuf port): writeVarInt is LEB128 and readVarInt is its inverse,
// matching VarInt.write/read byte-for-byte.
//
//   pkt_remove_entities_parity [--cases mcpp/build/pkt_remove_entities.tsv]
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

// Parse the comma-joined decimal id list column; "-" means an empty list.
std::vector<int32_t> parseIds(const std::string& col) {
    std::vector<int32_t> ids;
    if (col == "-" || col.empty()) return ids;
    std::istringstream ss(col);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (tok.empty()) continue;
        ids.push_back((int32_t)std::stoll(tok));
    }
    return ids;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        bytes.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return bytes;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_remove_entities.tsv";
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

        // ENC <name> <count> <id0,id1,...|-> <readableBytes> <hexBytes>
        std::string name, countStr, idsCol, readableStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, countStr, '\t')
            || !std::getline(ss, idsCol, '\t') || !std::getline(ss, readableStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t count = (int32_t)std::stoll(countStr);
        int32_t expReadable = (int32_t)std::stoll(readableStr);
        std::vector<int32_t> ids = parseIds(idsCol);

        if ((int32_t)ids.size() != count) {
            ++mismatches;
            std::cerr << "BAD-ROW name=" << name << " count=" << count
                      << " parsedIds=" << ids.size() << "\n";
            continue;
        }

        // write(): FriendlyByteBuf.writeIntIdList -> writeVarInt(size) + each id VarInt.
        PacketBuffer enc;
        enc.writeVarInt((int32_t)ids.size());
        for (int32_t id : ids) enc.writeVarInt(id);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH name=" << name
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }
        if ((int32_t)enc.size() != expReadable) {
            ++mismatches;
            std::cerr << "READABLE-MISMATCH name=" << name
                      << " got=" << enc.size() << " want=" << expReadable << "\n";
            continue;
        }

        // read(): decode the expected bytes back -> readVarInt(count) + count * readVarInt(id).
        PacketBuffer dec(unhex(expHex));
        int32_t decCount = dec.readVarInt();
        if (decCount != count) {
            ++mismatches;
            std::cerr << "DEC-COUNT-MISMATCH name=" << name
                      << " got=" << decCount << " want=" << count << "\n";
            continue;
        }
        bool idMismatch = false;
        for (int32_t i = 0; i < decCount; ++i) {
            int32_t id = dec.readVarInt();
            if (id != ids[(size_t)i]) {
                ++mismatches;
                idMismatch = true;
                std::cerr << "DEC-ID-MISMATCH name=" << name << " idx=" << i
                          << " got=" << id << " want=" << ids[(size_t)i] << "\n";
                break;
            }
        }
        if (idMismatch) continue;
    }

    std::cout << "PktRemoveEntitiesParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
