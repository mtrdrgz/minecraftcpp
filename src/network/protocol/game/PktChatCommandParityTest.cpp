// Byte-exact parity for net.minecraft.network.protocol.game.ServerboundChatCommandPacket
// against the REAL STREAM_CODEC (tools/PktChatCommandParity.java ground truth).
//
// The packet is a record (String command). Its codec is exactly:
//     writeUtf(command);   (read: readUtf();)
// No packet-type id is part of the codec bytes. We reuse the certified
// mc::net::PacketBuffer (the FriendlyByteBuf port): writeString == writeUtf
// (Utf8String.write: VarInt byte-length prefix + UTF-8 bytes, maxLen 32767),
// readString == readUtf.
//
// This is the UNSIGNED chat-command packet. The SIGNED variant carries
// signatures and is intentionally out of scope (see GT tool header).
//
// For each ENC case we (a) ENCODE command and require the bytes match Java's
// hex exactly, (b) check readableBytes() == byte count, and (c) DECODE the Java
// bytes back through PacketBuffer and require command round-trips identically.
//
//   pkt_chat_command_parity [--cases mcpp/build/pkt_chat_command.tsv]
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
    std::vector<uint8_t> v;
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        v.push_back((uint8_t)std::stoi(s.substr(i, 2), nullptr, 16));
    return v;
}
// Reverse the GT tool's escaping: "\\\\"->'\\', "\\n"->'\n', "\\r"->'\r'.
std::string unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[i + 1];
            if (c == '\\') { out.push_back('\\'); ++i; continue; }
            if (c == 'n')  { out.push_back('\n'); ++i; continue; }
            if (c == 'r')  { out.push_back('\r'); ++i; continue; }
        }
        out.push_back(s[i]);
    }
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_chat_command.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t command(escaped) \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, cmdEsc, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, cmdEsc, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // command column is UTF-8 HEX (ASCII-safe transport, see GT tool); decode to the
        // exact byte string the packet carries so writeUtf is exercised on real UTF-8.
        std::vector<uint8_t> cmdBytes = unhex(cmdEsc);
        std::string command(cmdBytes.begin(), cmdBytes.end());
        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly (writeUtf == writeString).
        PacketBuffer enc;
        try {
            enc.writeString(command);
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
            if (enc.data().size() != readable) {
                std::cerr << "READABLE-MISMATCH " << name << " got "
                          << enc.data().size() << " want " << readable << "\n";
                ok = false;
            }
        }

        // (b) DECODE the Java bytes -> command round trip (readUtf == readString).
        try {
            PacketBuffer dec(unhex(expHex));
            std::string gotCmd = dec.readString();
            if (gotCmd != command) {
                std::cerr << "DECODE-CMD-MISMATCH " << name << "\n";
                ok = false;
            }
            if (dec.remaining() != 0) {
                std::cerr << "DECODE-TRAILING " << name << " remaining "
                          << dec.remaining() << "\n";
                ok = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "DECODE-EXCEPTION " << name << ": " << e.what() << "\n";
            ok = false;
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktChatCommandParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
