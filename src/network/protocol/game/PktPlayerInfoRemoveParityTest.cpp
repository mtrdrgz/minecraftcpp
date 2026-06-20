// Parity gate for ClientboundPlayerInfoRemovePacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktPlayerInfoRemoveParity.java ground truth).
//
// The packet body is exactly FriendlyByteBuf.writeCollection(profileIds, UUIDUtil.STREAM_CODEC):
//   write : writeVarInt(ids.size()); for each UUID -> writeUUID(msb,lsb)
//   read  : int count = readVarInt(); loop count * readUUID
// (net.minecraft.network.protocol.game.ClientboundPlayerInfoRemovePacket lines 11-22,
//  FriendlyByteBuf.writeCollection/readList lines 134-144,
//  FriendlyByteBuf.writeUUID/readUUID lines 498-509 = writeLong(msb) then writeLong(lsb),
//  UUIDUtil.STREAM_CODEC lines 42-50 delegates to FriendlyByteBuf.{write,read}UUID.)
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// Every field is a VarInt (plain LEB128) count prefix plus, per UUID, two big-endian
// 8-byte longs, so this reuses the certified PacketBuffer (the FriendlyByteBuf port):
// writeVarInt matches VarInt.write, and writeUUID(hi,lo) is exactly two writeLong
// (writeBE<uint64_t>) calls — msb then lsb.
//
//   pkt_player_info_remove_parity [--cases mcpp/build/pkt_player_info_remove.tsv]
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

// One UUID as the raw 64-bit halves (bit-pattern of the signed Java longs).
struct Uuid { uint64_t msb; uint64_t lsb; };

// Parse the ';'-joined "msb:lsb" decimal-signed-long column; "-" means an empty list.
// stoll parses the signed decimal; reinterpret the bits as uint64_t so writeUUID's
// big-endian writeLong reproduces the exact wire bytes.
std::vector<Uuid> parseUuids(const std::string& col) {
    std::vector<Uuid> out;
    if (col == "-" || col.empty()) return out;
    std::istringstream ss(col);
    std::string pair;
    while (std::getline(ss, pair, ';')) {
        if (pair.empty()) continue;
        size_t colon = pair.find(':');
        if (colon == std::string::npos) continue;
        long long msb = std::stoll(pair.substr(0, colon));
        long long lsb = std::stoll(pair.substr(colon + 1));
        out.push_back(Uuid{(uint64_t)msb, (uint64_t)lsb});
    }
    return out;
}

std::vector<uint8_t> unhex(const std::string& s) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        bytes.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return bytes;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_player_info_remove.tsv";
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

        // ENC <name> <count> <msb0:lsb0;...|-> <readableBytes> <hexBytes>
        std::string name, countStr, uuidsCol, readableStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, countStr, '\t')
            || !std::getline(ss, uuidsCol, '\t') || !std::getline(ss, readableStr, '\t')
            || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t count = (int32_t)std::stoll(countStr);
        int32_t expReadable = (int32_t)std::stoll(readableStr);
        std::vector<Uuid> uuids = parseUuids(uuidsCol);

        if ((int32_t)uuids.size() != count) {
            ++mismatches;
            std::cerr << "BAD-ROW name=" << name << " count=" << count
                      << " parsedUuids=" << uuids.size() << "\n";
            continue;
        }

        // write(): writeCollection -> writeVarInt(size) + each writeUUID(msb,lsb).
        PacketBuffer enc;
        enc.writeVarInt((int32_t)uuids.size());
        for (const Uuid& u : uuids) enc.writeUUID(u.msb, u.lsb);

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

        // read(): decode the expected bytes back -> readVarInt(count) + count * readUUID.
        PacketBuffer dec(unhex(expHex));
        int32_t decCount = dec.readVarInt();
        if (decCount != count) {
            ++mismatches;
            std::cerr << "DEC-COUNT-MISMATCH name=" << name
                      << " got=" << decCount << " want=" << count << "\n";
            continue;
        }
        bool uuidMismatch = false;
        for (int32_t i = 0; i < decCount; ++i) {
            uint64_t hi, lo;
            dec.readUUID(hi, lo);
            if (hi != uuids[(size_t)i].msb || lo != uuids[(size_t)i].lsb) {
                ++mismatches;
                uuidMismatch = true;
                std::cerr << "DEC-UUID-MISMATCH name=" << name << " idx=" << i << "\n";
                break;
            }
        }
        if (uuidMismatch) continue;
    }

    std::cout << "PktPlayerInfoRemoveParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
