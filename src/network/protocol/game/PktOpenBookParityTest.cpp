// Parity gate for ClientboundOpenBookPacket's StreamCodec vs the REAL net.minecraft
// codec (tools/PktOpenBookParity.java ground truth).
//
// The packet body is exactly:
//   write : FriendlyByteBuf.writeEnum(hand)  == writeVarInt(hand.ordinal())
//   read  : FriendlyByteBuf.readEnum(InteractionHand.class)
//           == InteractionHand.values()[readVarInt()]
// (net.minecraft.network.protocol.game.ClientboundOpenBookPacket lines 19-25;
//  FriendlyByteBuf.java:467-473.) STREAM_CODEC = Packet.codec(write, new): no
//  packet-id prefix, just the body.
//
// InteractionHand (net.minecraft.world.InteractionHand.java:10-13):
//   MAIN_HAND ordinal 0, OFF_HAND ordinal 1 -> wire body 0x00 / 0x01 (1 byte each).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt(ordinal) and readVarInt() are byte-for-byte / value-for-value the
// same as the real codec. For each ENC row we encode via PacketBuffer and require
// hex==expected AND readableBytes==expected; then we decode the expected bytes back
// through PacketBuffer and require the ordinal round-trips.
//
//   pkt_open_book_parity [--cases mcpp/build/pkt_open_book.tsv]
#include "../../PacketBuffer.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
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

// The InteractionHand constants in declaration order == ordinal order, mirroring
// net.minecraft.world.InteractionHand.values() (the array readEnum indexes into).
constexpr std::array<std::string_view, 2> kInteractionHand = {"MAIN_HAND", "OFF_HAND"};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_open_book.tsv";
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
            // ENUM <ordinal> <name> — pin our constant table against the real enum.
            std::string ordStr, name;
            if (!std::getline(ss, ordStr, '\t') || !std::getline(ss, name)) continue;
            ++cases;
            int32_t ord = (int32_t)std::stoll(ordStr);
            if (ord < 0 || ord >= (int32_t)kInteractionHand.size()
                || kInteractionHand[ord] != name) {
                ++mismatches;
                std::cerr << "ENUM-MISMATCH ord=" << ord << " name=" << name
                          << " ours=" << (ord >= 0 && ord < (int32_t)kInteractionHand.size()
                                              ? std::string(kInteractionHand[ord]) : "<oob>")
                          << "\n";
            }
        } else if (tag == "ENC") {
            // ENC <name> <ordinal> <readableBytes> <hexBytes>
            std::string name, ordStr, rbStr, expHex;
            if (!std::getline(ss, name, '\t') || !std::getline(ss, ordStr, '\t')
                || !std::getline(ss, rbStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t ord = (int32_t)std::stoll(ordStr);
            int32_t expReadable = (int32_t)std::stoll(rbStr);

            // Pin the name->ordinal mapping against our enum table too.
            if (ord < 0 || ord >= (int32_t)kInteractionHand.size()
                || kInteractionHand[ord] != name) {
                ++mismatches;
                std::cerr << "ENC-NAME-MISMATCH name=" << name << " ord=" << ord << "\n";
                continue;
            }

            // write(): FriendlyByteBuf.writeEnum(hand) == writeVarInt(hand.ordinal()).
            PacketBuffer enc;
            enc.writeVarInt(ord);
            std::string got = hex(enc.data());
            if (got != expHex) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH name=" << name << " ord=" << ord
                          << "\n  got  " << got << "\n  want " << expHex << "\n";
                continue;
            }
            if ((int32_t)enc.data().size() != expReadable) {
                ++mismatches;
                std::cerr << "ENC-LEN-MISMATCH name=" << name << " got=" << enc.data().size()
                          << " want=" << expReadable << "\n";
                continue;
            }

            // Round-trip: decode the expected bytes back through PacketBuffer and
            // require the ordinal (== InteractionHand.values() index) round-trips.
            PacketBuffer dec(unhex(expHex));
            int32_t gotOrd = dec.readVarInt();
            if (dec.remaining() != 0) {
                ++mismatches;
                std::cerr << "ENC-RT-TRAILING name=" << name
                          << " leftover=" << dec.remaining() << "\n";
                continue;
            }
            if (gotOrd != ord) {
                ++mismatches;
                std::cerr << "ENC-RT-MISMATCH name=" << name << " got=" << gotOrd
                          << " want=" << ord << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <ordinal_in> <ordinal_decoded>
            std::string inHex, ordStr, decStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, ordStr, '\t')
                || !std::getline(ss, decStr)) continue;
            ++cases;
            int32_t expOrd = (int32_t)std::stoll(decStr);

            // read(): FriendlyByteBuf.readEnum(InteractionHand.class)
            //         == InteractionHand.values()[readVarInt()].
            PacketBuffer dec(unhex(inHex));
            int32_t gotOrd = dec.readVarInt();   // index into values() == the ordinal
            // readEnum indexes getEnumConstants()[idx]; legal inputs stay in range.
            if (gotOrd < 0 || gotOrd >= (int32_t)kInteractionHand.size()) {
                ++mismatches;
                std::cerr << "DEC-OOB hex=" << inHex << " idx=" << gotOrd << "\n";
                continue;
            }
            if (gotOrd != expOrd) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex << " got=" << gotOrd
                          << " want=" << expOrd << "\n";
            }
        }
    }

    std::cout << "PktOpenBookParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
