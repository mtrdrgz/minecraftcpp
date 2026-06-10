// Parity gate for ServerboundUseItemPacket's StreamCodec vs the REAL net.minecraft
// codec (tools/PktUseItemSbParity.java ground truth).
//
// The packet body is exactly, IN THIS ORDER
// (net.minecraft.network.protocol.game.ServerboundUseItemPacket.java:32-37 write /
//  25-30 read):
//   writeEnum(hand)      == writeVarInt(hand.ordinal())   (FriendlyByteBuf.java:471-473)
//   writeVarInt(sequence)                                  // LEB128 VarInt
//   writeFloat(yRot)                                       // big-endian IEEE-754 4 bytes
//   writeFloat(xRot)                                       // big-endian IEEE-754 4 bytes
// read mirrors it: readEnum(InteractionHand.class)==values()[readVarInt()], readVarInt(),
// readFloat(), readFloat(). Packet.codec -> body only, no packet-id/length prefix.
//
// InteractionHand (net.minecraft.world.InteractionHand.java:10-13): MAIN_HAND ordinal 0,
// OFF_HAND ordinal 1 -> the hand wire field is a VarInt of the ordinal.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
// writeVarInt(ordinal)/writeVarInt(sequence)/writeFloat(...) and the matching readers are
// byte-for-byte / value-for-value the same as the real codec.
//
//   pkt_use_item_sb_parity [--cases mcpp/build/pkt_use_item_sb.tsv]
#include "../../PacketBuffer.h"

#include <array>
#include <cstdint>
#include <cstring>
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

// Reconstruct a float from its raw 32-bit pattern (matches Float.floatToRawIntBits).
float floatFromBits(uint32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}
uint32_t bitsFromFloat(float f) {
    uint32_t u;
    std::memcpy(&u, &f, sizeof(u));
    return u;
}

// InteractionHand constants in declaration order == ordinal order, mirroring
// net.minecraft.world.InteractionHand.values() (the array readEnum indexes into).
constexpr std::array<std::string_view, 2> kInteractionHand = {"MAIN_HAND", "OFF_HAND"};
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_use_item_sb.tsv";
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
            // ENC <handOrdinal> <sequence> <yRotBitsHex> <xRotBitsHex> <readableBytes> <hex>
            std::string handStr, seqStr, yStr, xStr, nStr, expHex;
            if (!std::getline(ss, handStr, '\t') || !std::getline(ss, seqStr, '\t')
                || !std::getline(ss, yStr, '\t') || !std::getline(ss, xStr, '\t')
                || !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t hand = (int32_t)std::stoll(handStr);
            int32_t sequence = (int32_t)std::stoll(seqStr);
            uint32_t yBits = (uint32_t)std::stoul(yStr, nullptr, 16);
            uint32_t xBits = (uint32_t)std::stoul(xStr, nullptr, 16);
            int expN = std::stoi(nStr);

            // write(): writeEnum(hand)==writeVarInt(ordinal), writeVarInt(sequence),
            //          writeFloat(yRot), writeFloat(xRot).
            PacketBuffer enc;
            enc.writeVarInt(hand);
            enc.writeVarInt(sequence);
            enc.writeFloat(floatFromBits(yBits));
            enc.writeFloat(floatFromBits(xBits));
            std::string got = hex(enc.data());
            if (got != expHex || (int)enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH hand=" << hand << " seq=" << sequence
                          << " yBits=" << yStr << " xBits=" << xStr
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hex> <handOrd_in> <seq_in> <yBits_in> <xBits_in> \
            //     <handOrd_dec> <seq_dec> <yBits_dec> <xBits_dec>
            std::string inHex, hStr, sStr, yStr, xStr, dhStr, dsStr, dyStr, dxStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, hStr, '\t')
                || !std::getline(ss, sStr, '\t') || !std::getline(ss, yStr, '\t')
                || !std::getline(ss, xStr, '\t') || !std::getline(ss, dhStr, '\t')
                || !std::getline(ss, dsStr, '\t') || !std::getline(ss, dyStr, '\t')
                || !std::getline(ss, dxStr)) continue;
            ++cases;
            int32_t expHand = (int32_t)std::stoll(dhStr);
            int32_t expSeq = (int32_t)std::stoll(dsStr);
            uint32_t expYBits = (uint32_t)std::stoul(dyStr, nullptr, 16);
            uint32_t expXBits = (uint32_t)std::stoul(dxStr, nullptr, 16);

            // read(): readEnum==values()[readVarInt()], readVarInt(), readFloat(), readFloat().
            PacketBuffer dec(unhex(inHex));
            int32_t gotHand = dec.readVarInt();   // index into values() == the ordinal
            int32_t gotSeq = dec.readVarInt();
            uint32_t gotYBits = bitsFromFloat(dec.readFloat());
            uint32_t gotXBits = bitsFromFloat(dec.readFloat());

            // readEnum indexes getEnumConstants()[idx]; legal inputs stay in range.
            if (gotHand < 0 || gotHand >= (int32_t)kInteractionHand.size()) {
                ++mismatches;
                std::cerr << "DEC-OOB hex=" << inHex << " handIdx=" << gotHand << "\n";
                continue;
            }
            if (gotHand != expHand || gotSeq != expSeq
                || gotYBits != expYBits || gotXBits != expXBits) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << "\n  got  hand=" << gotHand << " seq=" << gotSeq
                          << " y=" << std::hex << gotYBits << " x=" << gotXBits << std::dec
                          << "\n  want hand=" << expHand << " seq=" << expSeq
                          << " y=" << std::hex << expYBits << " x=" << expXBits << std::dec
                          << "\n";
            }
        }
    }

    std::cout << "PktUseItemSbParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
