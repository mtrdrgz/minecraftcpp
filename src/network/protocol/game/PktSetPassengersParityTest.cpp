// Parity gate for ClientboundSetPassengersPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktSetPassengersParity.java ground truth).
//
// The packet body is (ClientboundSetPassengersPacket lines 27-35 in 26.1.2/src):
//   write : writeVarInt(vehicle); writeVarIntArray(passengers);
//   read  : vehicle = readVarInt(); passengers = readVarIntArray();
// FriendlyByteBuf.writeVarIntArray(int[]) (lines 309-317) is:
//   writeVarInt(ints.length); for (int i : ints) writeVarInt(i);
// readVarIntArray() (lines 319-335): int size = readVarInt(); loop size * readVarInt().
//
// So the wire body is exactly:
//   VarInt vehicle | VarInt count | count * VarInt passengerId
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// Every field is a VarInt, so this reuses the certified PacketBuffer (the
// FriendlyByteBuf port): writeVarInt is LEB128 and readVarInt is its inverse,
// matching VarInt.write/read byte-for-byte.
//
//   pkt_set_passengers_parity [--cases mcpp/build/pkt_set_passengers.tsv]
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

// Parse the comma-joined decimal passenger-id list column; "-" means an empty list.
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
    std::string casesPath = "mcpp/build/pkt_set_passengers.tsv";
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

        // ENC <name> <vehicle> <count> <id0,id1,...|-> <readableBytes> <hexBytes>
        std::string name, vehicleStr, countStr, idsCol, readableStr, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, vehicleStr, '\t')
            || !std::getline(ss, countStr, '\t') || !std::getline(ss, idsCol, '\t')
            || !std::getline(ss, readableStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t vehicle = (int32_t)std::stoll(vehicleStr);
        int32_t count = (int32_t)std::stoll(countStr);
        int32_t expReadable = (int32_t)std::stoll(readableStr);
        std::vector<int32_t> ids = parseIds(idsCol);

        if ((int32_t)ids.size() != count) {
            ++mismatches;
            std::cerr << "BAD-ROW name=" << name << " count=" << count
                      << " parsedIds=" << ids.size() << "\n";
            continue;
        }

        // write(): writeVarInt(vehicle) then writeVarIntArray -> writeVarInt(size)
        // + each passenger id VarInt.
        PacketBuffer enc;
        enc.writeVarInt(vehicle);
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

        // read(): decode the expected bytes back -> readVarInt(vehicle),
        // readVarInt(count), count * readVarInt(passenger id).
        PacketBuffer dec(unhex(expHex));
        int32_t decVehicle = dec.readVarInt();
        if (decVehicle != vehicle) {
            ++mismatches;
            std::cerr << "DEC-VEHICLE-MISMATCH name=" << name
                      << " got=" << decVehicle << " want=" << vehicle << "\n";
            continue;
        }
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

    std::cout << "PktSetPassengersParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
