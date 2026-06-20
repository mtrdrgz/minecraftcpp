// Parity gate for net.minecraft.network.protocol.game.ClientboundSetCameraPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktSetCameraParity.java GT).
//
// The packet body is exactly:
//   write : FriendlyByteBuf.writeVarInt(cameraId)   (plain LEB128, NOT zig-zag)
//   read  : FriendlyByteBuf.readVarInt()
// (ClientboundSetCameraPacket lines 21-27; Packet.codec -> StreamCodec.ofMember:
//  no packet-id prefix, just the body.) cameraId is a raw entity id (plain int), so
//  negatives are legal and encode to five bytes.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt/readVarInt are byte-for-byte the net.minecraft VarInt LEB128 codec.
//
//   pkt_set_camera_parity [--cases mcpp/build/pkt_set_camera.tsv]
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
    std::vector<uint8_t> v;
    v.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return v;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_set_camera.tsv";
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

        if (tag == "ENC") {
            // ENC <cameraId_in> <hex>
            std::string idStr, expHex;
            if (!std::getline(ss, idStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t cameraId = (int32_t)std::stoll(idStr);

            // write(): FriendlyByteBuf.writeVarInt(cameraId) -> LEB128.
            PacketBuffer enc;
            enc.writeVarInt(cameraId);
            std::string got = hex(enc.data());
            if (got != expHex) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH cameraId=" << cameraId << "\n  got  " << got
                          << "\n  want " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hex> <cameraId_decoded>
            std::string inHex, idStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, idStr)) continue;
            ++cases;
            int32_t expId = (int32_t)std::stoll(idStr);

            // read(): FriendlyByteBuf.readVarInt().
            std::vector<uint8_t> bytes = unhex(inHex);
            PacketBuffer dec(bytes);
            int32_t gotId = dec.readVarInt();
            bool ok = (gotId == expId) && (dec.remaining() == 0);
            if (!ok) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << " got=" << gotId
                          << " want=" << expId << " remaining=" << dec.remaining() << "\n";
            }
        }
    }

    std::cout << "PktSetCameraParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
