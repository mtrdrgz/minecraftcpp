// Parity gate for ServerboundContainerButtonClickPacket's StreamCodec vs the REAL
// net.minecraft codec (tools/PktContainerButtonClickSbParity.java ground truth).
//
// The packet is a record(int containerId, int buttonId). Its STREAM_CODEC is
// StreamCodec.composite(ByteBufCodecs.CONTAINER_ID, ByteBufCodecs.VAR_INT, new)
// (ServerboundContainerButtonClickPacket.java:10-16):
//   CONTAINER_ID -> FriendlyByteBuf.writeContainerId == VarInt.write  (ByteBufCodecs.java:200-208)
//   VAR_INT      -> VarInt.write                                      (ByteBufCodecs.java:102-110)
// StreamCodec.composite carries no packet-id prefix, just the body, so the whole wire
// payload is: VarInt(containerId) ++ VarInt(buttonId), both signed ints in plain LEB128
// (no zig-zag — negatives encode as 5 bytes).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt(x) / readVarInt() are byte-for-byte / value-for-value the same as the real
// codec.
//
//   pkt_container_button_click_sb_parity [--cases mcpp/build/pkt_container_button_click_sb.tsv]
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
    std::string casesPath = "mcpp/build/pkt_container_button_click_sb.tsv";
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

        // ENC <containerId-dec> <buttonId-dec> <readableBytes-dec> <hex>
        std::string cStr, bStr, lenStr, expHex;
        if (!std::getline(ss, cStr, '\t') || !std::getline(ss, bStr, '\t')
            || !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;
        int32_t containerId = (int32_t)std::stoll(cStr);
        int32_t buttonId = (int32_t)std::stoll(bStr);
        size_t expLen = (size_t)std::stoull(lenStr);

        // write(): writeContainerId(containerId)[== writeVarInt] + writeVarInt(buttonId).
        PacketBuffer enc;
        enc.writeVarInt(containerId);
        enc.writeVarInt(buttonId);
        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH containerId=" << containerId
                      << " buttonId=" << buttonId
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH containerId=" << containerId
                      << " buttonId=" << buttonId
                      << " got=" << enc.data().size() << " want=" << expLen << "\n";
        }

        // read(): readVarInt() twice must recover the exact signed ints, in order.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        int32_t gotC = dec.readVarInt();
        int32_t gotB = dec.readVarInt();
        if (gotC != containerId || gotB != buttonId) {
            ++mismatches;
            std::cerr << "DEC-MISMATCH hex=" << expHex
                      << " got=(" << gotC << "," << gotB << ")"
                      << " want=(" << containerId << "," << buttonId << ")\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING containerId=" << containerId
                      << " buttonId=" << buttonId
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktContainerButtonClickSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
