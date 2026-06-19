// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundDeleteChatPacket
// vs the REAL ClientboundDeleteChatPacket.STREAM_CODEC (tools/DeleteChatParity.java).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src):
//   ClientboundDeleteChatPacket(MessageSignature.Packed messageSignature)
//   STREAM_CODEC = Packet.codec(::write, ::new)  -> body only, NO packet-id prefix.
//   write(FriendlyByteBuf output):
//       MessageSignature.Packed.write(output, this.messageSignature);
//   MessageSignature.Packed.write(out, packed) (MessageSignature.java:85-90):
//       out.writeVarInt(packed.id() + 1);                 // VarInt (plain LEB128, no zig-zag)
//       if (packed.fullSignature() != null)               // == iff id == -1
//           MessageSignature.write(out, packed.fullSignature());  // out.writeBytes(256 raw bytes)
//
//   Two encodings:
//     (A) cached id >= 0:  -> VarInt(id+1)                    [9-byte boundaries, 5 bytes if id+1<0]
//     (B) full signature (id == -1): -> VarInt(0) + 256 raw bytes
//
//   NOTE: Java computes (id + 1) in 32-bit int arithmetic; for id == Integer.MAX_VALUE this
//   overflows to Integer.MIN_VALUE and writeVarInt emits a negative 5-byte VarInt. The C++
//   port reproduces this via int32_t wrap-around before writeVarInt.
//
//   pkt_delete_chat_parity [--cases mcpp/build/pkt_delete_chat.tsv]
//
// Row: ENC <name> <id-dec-signed> <full 0|1> <sigHex-or-"-"> <readableBytes-dec> <hexBytes>
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
    std::string casesPath = "mcpp/build/pkt_delete_chat.tsv";
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
        std::string tag, name, idStr, fullStr, sigHex, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, idStr, '\t') ||
            !std::getline(ss, fullStr, '\t') || !std::getline(ss, sigHex, '\t') ||
            !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t id    = (int32_t)std::stoll(idStr);
        int     full  = std::stoi(fullStr);
        size_t  expBytes = (size_t)std::stoul(nStr);

        // Validate the GT's consistency: id == -1 iff a full signature follows.
        bool wantFull = (full != 0);
        if (wantFull != (id == -1)) {
            ++mismatches;
            std::cerr << "FULL-FLAG-MISMATCH " << name << " id=" << id << " full=" << full << "\n";
            continue;
        }

        std::vector<uint8_t> sig;
        if (wantFull) {
            sig = unhex(sigHex);
            if (sig.size() != 256) {
                ++mismatches;
                std::cerr << "SIG-LEN-MISMATCH " << name << " bytes=" << sig.size() << " want=256\n";
                continue;
            }
        } else if (sigHex != "-") {
            ++mismatches;
            std::cerr << "SIG-PRESENT-MISMATCH " << name << " (expected '-' for cached id)\n";
            continue;
        }

        // (1) ENCODE the full packet through PacketBuffer in the EXACT field order:
        //     writeVarInt(id + 1) [+ writeBytes(256-byte signature)].
        //     int32_t arithmetic wraps exactly like Java's (id + 1).
        int32_t idPlus1 = (int32_t)((uint32_t)id + 1u);
        PacketBuffer enc;
        enc.writeVarInt(idPlus1);
        if (wantFull) enc.writeBytes(sig);

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " id=" << id
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back through PacketBuffer: VarInt(id+1) round-trips,
        //     then the 256-byte signature tail (if any) matches exactly and nothing remains.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t bIdPlus1 = dec.readVarInt();
        int32_t bId = (int32_t)((uint32_t)bIdPlus1 - 1u);
        bool ok = (bId == id);
        if (wantFull) {
            std::vector<uint8_t> tail = dec.readBytes(dec.remaining());
            ok = ok && (tail == sig);
        }
        ok = ok && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " id(got=" << bId << " want=" << id << ")"
                      << " remaining=" << dec.remaining() << "\n";
            continue;
        }
    }

    std::cout << "PktDeleteChatParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
