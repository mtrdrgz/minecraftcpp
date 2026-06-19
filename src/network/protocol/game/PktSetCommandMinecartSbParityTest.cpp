// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundSetCommandMinecartPacket
// against the REAL STREAM_CODEC (tools/PktSetCommandMinecartSbParity.java ground truth).
//
// The packet body is exactly (ServerboundSetCommandMinecartPacket.java:33-37):
//   write : output.writeVarInt(this.entity);      // LEB128 VarInt
//           output.writeUtf(this.command);        // VarInt byte-len + UTF-8
//           output.writeBoolean(this.trackOutput);// single byte 0/1
//   read  : this.entity      = input.readVarInt();
//           this.command     = input.readUtf();   // default maxLen 32767
//           this.trackOutput = input.readBoolean();
// Packet.codec -> StreamCodec.ofMember (Packet.java): body only, no id/length prefix.
//
// This reuses the certified PacketBuffer (the FriendlyByteBuf port) directly:
//   writeVarInt(entity) + writeString(command) + writeBool(trackOutput)
// is byte-for-byte the same as the real codec, and readVarInt()/readString()/readBool()
// round-trips the fields value-for-value. writeString == writeUtf (Utf8String.write:
// VarInt byte-length prefix + UTF-8 bytes). The command arrives as a UTF-8 HEX column.
//
//   pkt_set_command_minecart_sb_parity [--cases mcpp/build/pkt_set_command_minecart_sb.tsv]
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

// A UTF-8 HEX column -> the raw byte string the packet carries.
std::string hexToStr(const std::string& h) {
    std::vector<uint8_t> b = unhex(h);
    return std::string(b.begin(), b.end());
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_set_command_minecart_sb.tsv";
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

        // ENC <name> <entity> <commandHex> <trackOutput> <readableBytes> <hexBytes>
        std::string name, entS, cmdH, toS, rbS, expHex;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, entS, '\t')
            || !std::getline(ss, cmdH, '\t') || !std::getline(ss, toS, '\t')
            || !std::getline(ss, rbS, '\t') || !std::getline(ss, expHex))
            continue;
        ++cases;

        int32_t entity = (int32_t)std::stoll(entS);
        std::string command = hexToStr(cmdH);
        bool trackOutput = std::stoi(toS) != 0;
        size_t expReadable = (size_t)std::stoull(rbS);

        bool ok = true;

        // write(): writeVarInt(entity) + writeUtf(command) + writeBoolean(trackOutput).
        PacketBuffer enc;
        try {
            enc.writeVarInt(entity);
            enc.writeString(command);
            enc.writeBool(trackOutput);
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
            int32_t gotEntity = dec.readVarInt();
            std::string gotCmd = dec.readString();
            bool gotTrack = dec.readBool();

            if (gotEntity != entity) {
                std::cerr << "DEC-ENTITY " << name << " got=" << gotEntity
                          << " want=" << entity << "\n";
                ok = false;
            }
            if (gotCmd != command) {
                std::cerr << "DEC-COMMAND " << name << " mismatch\n";
                ok = false;
            }
            if (gotTrack != trackOutput) {
                std::cerr << "DEC-TRACK " << name << " got=" << gotTrack
                          << " want=" << trackOutput << "\n";
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

    std::cout << "PktSetCommandMinecartSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
