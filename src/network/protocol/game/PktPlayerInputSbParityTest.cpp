// Parity gate for net.minecraft.network.protocol.game.ServerboundPlayerInputPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktPlayerInputSbParity.java GT).
//
// The packet is a record wrapping a single net.minecraft.world.entity.player.Input
// (real 26.1.2 source, ServerboundPlayerInputPacket lines 9-12):
//   STREAM_CODEC = StreamCodec.composite(Input.STREAM_CODEC, ...::input, ...::new)
// so the whole wire payload is exactly Input.STREAM_CODEC's output (NO packet-id prefix).
// Input.STREAM_CODEC (real 26.1.2 source, Input lines 7-38) packs 7 booleans into a
// single byte:
//   FLAG_FORWARD=1, FLAG_BACKWARD=2, FLAG_LEFT=4, FLAG_RIGHT=8,
//   FLAG_JUMP=16, FLAG_SHIFT=32, FLAG_SPRINT=64
//   write : flags = (fwd?1:0)|(back?2:0)|(left?4:0)|(right?8:0)
//                  |(jump?16:0)|(shift?32:0)|(sprint?64:0)
//           writeByte(flags)                    // single raw byte
//   read  : flags = readByte();
//           fwd=(flags&1)!=0; back=(flags&2)!=0; left=(flags&4)!=0; right=(flags&8)!=0;
//           jump=(flags&16)!=0; shift=(flags&32)!=0; sprint=(flags&64)!=0;
// So the wire payload is EXACTLY 1 byte (bit 0x80 unused; max value 0x7f).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly: writeByte
// (single raw byte) is byte-for-byte the same as the real codec. For each ENC row we:
//   (1) ENCODE: pack the seven flag bits into a byte EXACTLY as the Java write() does,
//       writeByte(flags); require bytes == Java hex AND byte count == readableBytes.
//   (2) DECODE: read the Java byte back via readByte(), unpack the seven flag bits, and
//       require exact flag recovery + no trailing bytes.
//
//   pkt_player_input_sb_parity [--cases mcpp/build/pkt_player_input_sb.tsv]
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
        out.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_player_input_sb.tsv";
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

        // ENC <name-hex> <fwd> <back> <left> <right> <jump> <shift> <sprint>
        //     <readableBytes-dec> <hex>
        std::string nameStr, fwdStr, backStr, leftStr, rightStr, jumpStr, shiftStr,
                    sprintStr, lenStr, expHex;
        if (!std::getline(ss, nameStr, '\t') || !std::getline(ss, fwdStr, '\t')
            || !std::getline(ss, backStr, '\t') || !std::getline(ss, leftStr, '\t')
            || !std::getline(ss, rightStr, '\t') || !std::getline(ss, jumpStr, '\t')
            || !std::getline(ss, shiftStr, '\t') || !std::getline(ss, sprintStr, '\t')
            || !std::getline(ss, lenStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        bool forward  = (std::stoi(fwdStr)    != 0);
        bool backward = (std::stoi(backStr)   != 0);
        bool left     = (std::stoi(leftStr)   != 0);
        bool right    = (std::stoi(rightStr)  != 0);
        bool jump     = (std::stoi(jumpStr)   != 0);
        bool shift    = (std::stoi(shiftStr)  != 0);
        bool sprint   = (std::stoi(sprintStr) != 0);
        size_t expLen = (size_t)std::stoull(lenStr);

        // (1) ENCODE: pack flag bits EXACTLY as Java write(), then a single byte.
        uint8_t flags = 0;
        if (forward)  flags = (uint8_t)(flags | 1);
        if (backward) flags = (uint8_t)(flags | 2);
        if (left)     flags = (uint8_t)(flags | 4);
        if (right)    flags = (uint8_t)(flags | 8);
        if (jump)     flags = (uint8_t)(flags | 16);
        if (shift)    flags = (uint8_t)(flags | 32);
        if (sprint)   flags = (uint8_t)(flags | 64);

        PacketBuffer enc;
        enc.writeByte(flags);

        std::string got = hex(enc.data());
        if (got != expHex) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH name=" << nameStr << " flags=" << (int)flags
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
        }
        if (enc.data().size() != expLen) {
            ++mismatches;
            std::cerr << "LEN-MISMATCH name=" << nameStr << " got=" << enc.data().size()
                      << " want=" << expLen << "\n";
        }

        // (2) DECODE: readByte() recovers the exact seven flag bits.
        std::vector<uint8_t> bytes = unhex(expHex);
        PacketBuffer dec(bytes);
        uint8_t gotFlags = dec.readByte();
        bool gotFwd    = (gotFlags & 1)  != 0;
        bool gotBack   = (gotFlags & 2)  != 0;
        bool gotLeft   = (gotFlags & 4)  != 0;
        bool gotRight  = (gotFlags & 8)  != 0;
        bool gotJump   = (gotFlags & 16) != 0;
        bool gotShift  = (gotFlags & 32) != 0;
        bool gotSprint = (gotFlags & 64) != 0;

        if (gotFwd != forward || gotBack != backward || gotLeft != left
            || gotRight != right || gotJump != jump || gotShift != shift
            || gotSprint != sprint) {
            ++mismatches;
            std::cerr << "DEC-FLAG-MISMATCH hex=" << expHex
                      << " gotFlags=" << (int)gotFlags
                      << " want(" << forward << "," << backward << "," << left << ","
                      << right << "," << jump << "," << shift << "," << sprint << ")\n";
        }
        if (dec.remaining() != 0) {
            ++mismatches;
            std::cerr << "DEC-TRAILING name=" << nameStr << " remaining="
                      << dec.remaining() << "\n";
        }
    }

    std::cout << "PktPlayerInputSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
