// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundCommandSuggestionPacket
// against the REAL STREAM_CODEC (tools/PktCommandSuggestionSbParity.java ground truth).
//
// The packet body is exactly (ServerboundCommandSuggestionPacket.java:20-28):
//   write : output.writeVarInt(this.id);          // plain LEB128 VarInt (NOT zig-zag)
//           output.writeUtf(this.command, 32500); // VarInt byte-length prefix + UTF-8 bytes
//   read  : this.id      = input.readVarInt();
//           this.command = input.readUtf(32500);
// Packet.codec -> StreamCodec.ofMember (Packet.java): body only, no id/length prefix.
//
// id is a raw transaction id (a plain int): writeVarInt is plain LEB128, so negative ids
// encode to five bytes. The command's maxLength is 32500 (NOT the default 32767).
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
//   writeVarInt(id) + writeString(command, 32500)
// is byte-for-byte the same as the real codec, and readVarInt()/readString(32500)
// round-trips the fields value-for-value. writeString == writeUtf (Utf8String.write:
// VarInt byte-length prefix + UTF-8 bytes). The command arrives as a UTF-8 HEX column.
//
//   pkt_command_suggestion_sb_parity [--cases mcpp/build/pkt_command_suggestion_sb.tsv]
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

// A UTF-8 HEX column -> the raw byte string the packet carries.
std::string hexToStr(const std::string& h) {
    std::vector<uint8_t> b = unhex(h);
    return std::string(b.begin(), b.end());
}

// readUtf(32500) -> Utf8String.read: maxLength = 32500 (matches the write side).
constexpr int kMaxLen = 32500;
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_command_suggestion_sb.tsv";
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

        // ENC <name> <id-dec> <command_utf8_hex> <readableBytes> <hexBytes>
        std::string name, idS, cmdH, rbS, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, idS, '\t')
            || !std::getline(ss, cmdH, '\t') || !std::getline(ss, rbS, '\t')
            || !std::getline(ss, expHex))
            continue;
        ++cases;

        int32_t id = (int32_t)std::stoll(idS);
        std::string command = hexToStr(cmdH);
        size_t expReadable = (size_t)std::stoull(rbS);

        bool ok = true;

        // write(): writeVarInt(id) + writeUtf(command, 32500).
        PacketBuffer enc;
        try {
            enc.writeVarInt(id);
            enc.writeString(command, kMaxLen);
        } catch (const std::exception& e) {
            std::cerr << "ENC-EXCEPTION " << name << ": " << e.what() << "\n";
            ok = false;
        }

        if (ok) {
            std::string got = hex(enc.data());
            if (got != expHex) {
                std::cerr << "ENC-MISMATCH " << name << "\n  got  " << got
                          << "\n  want " << expHex << "\n";
                ok = false;
            }
            if (enc.data().size() != expReadable) {
                std::cerr << "LEN-MISMATCH " << name << " got=" << enc.data().size()
                          << " want=" << expReadable << "\n";
                ok = false;
            }
        }

        // read(): decode the expected bytes back and require the fields round-trip.
        try {
            std::vector<uint8_t> bytes = unhex(expHex);
            PacketBuffer dec(bytes);
            int32_t gotId = dec.readVarInt();
            std::string gotCmd = dec.readString(kMaxLen);

            if (gotId != id) {
                std::cerr << "DEC-ID " << name << " got=" << gotId
                          << " want=" << id << "\n";
                ok = false;
            }
            if (gotCmd != command) {
                std::cerr << "DEC-CMD " << name << " mismatch\n";
                ok = false;
            }
            if (dec.remaining() != 0) {
                std::cerr << "DEC-TRAILING " << name << " remaining "
                          << dec.remaining() << "\n";
                ok = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "DEC-EXCEPTION " << name << ": " << e.what() << "\n";
            ok = false;
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktCommandSuggestionSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
