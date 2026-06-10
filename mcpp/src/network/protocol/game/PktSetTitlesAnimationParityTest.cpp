// Parity gate for net.minecraft.network.protocol.game.ClientboundSetTitlesAnimationPacket's
// StreamCodec vs the REAL net.minecraft codec (tools/PktSetTitlesAnimationParity.java GT).
//
// The packet is class(int fadeIn, int stay, int fadeOut) and its body is exactly
// (ClientboundSetTitlesAnimationPacket.java:28-32):
//   write : FriendlyByteBuf.writeInt(this.fadeIn)    // big-endian 4-byte int
//           FriendlyByteBuf.writeInt(this.stay)      // big-endian 4-byte int
//           FriendlyByteBuf.writeInt(this.fadeOut)   // big-endian 4-byte int
//   read  : this.fadeIn  = input.readInt();
//           this.stay    = input.readInt();
//           this.fadeOut = input.readInt();
// Packet.codec -> StreamCodec.ofMember: no packet-id prefix, just the body.
//
// All three fields are plain ints, so this exercises the certified PacketBuffer (the
// FriendlyByteBuf port) directly: writeInt/readInt are big-endian 4-byte and identical
// to the real codec. fadeIn/stay/fadeOut are carried as decimal in the TSV.
//
//   pkt_set_titles_animation_parity [--cases mcpp/build/pkt_set_titles_animation.tsv]
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
    std::string casesPath = "mcpp/build/pkt_set_titles_animation.tsv";
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
            // ENC <fadeIn> <stay> <fadeOut> <readableBytes> <hexBytes>
            std::string fadeInStr, stayStr, fadeOutStr, nStr, expHex;
            if (!std::getline(ss, fadeInStr, '\t') || !std::getline(ss, stayStr, '\t')
                || !std::getline(ss, fadeOutStr, '\t') || !std::getline(ss, nStr, '\t')
                || !std::getline(ss, expHex)) continue;
            ++cases;
            int32_t fadeIn  = (int32_t)std::stoll(fadeInStr);
            int32_t stay    = (int32_t)std::stoll(stayStr);
            int32_t fadeOut = (int32_t)std::stoll(fadeOutStr);
            size_t expN = (size_t)std::stoull(nStr);

            // write(): writeInt(fadeIn) then writeInt(stay) then writeInt(fadeOut).
            PacketBuffer enc;
            enc.writeInt(fadeIn);
            enc.writeInt(stay);
            enc.writeInt(fadeOut);

            std::string got = hex(enc.data());
            if (got != expHex || enc.size() != expN) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH fadeIn=" << fadeIn << " stay=" << stay
                          << " fadeOut=" << fadeOut
                          << "\n  got  n=" << enc.size() << " " << got
                          << "\n  want n=" << expN << " " << expHex << "\n";
            }
        } else if (tag == "DEC") {
            // DEC <hexBytes> <fadeIn_in> <stay_in> <fadeOut_in> <fadeIn_dec> <stay_dec> <fadeOut_dec>
            std::string inHex, fInStr, sInStr, fOutStr, fDecStr, sDecStr, foDecStr;
            if (!std::getline(ss, inHex, '\t') || !std::getline(ss, fInStr, '\t')
                || !std::getline(ss, sInStr, '\t') || !std::getline(ss, fOutStr, '\t')
                || !std::getline(ss, fDecStr, '\t') || !std::getline(ss, sDecStr, '\t')
                || !std::getline(ss, foDecStr)) continue;
            ++cases;
            int32_t expFadeIn  = (int32_t)std::stoll(fDecStr);
            int32_t expStay    = (int32_t)std::stoll(sDecStr);
            int32_t expFadeOut = (int32_t)std::stoll(foDecStr);

            // read(): readInt() then readInt() then readInt().
            PacketBuffer dec(unhex(inHex));
            int32_t gotFadeIn  = dec.readInt();
            int32_t gotStay    = dec.readInt();
            int32_t gotFadeOut = dec.readInt();
            // ensure the buffer was fully consumed (no leftover bytes).
            bool consumed = (dec.remaining() == 0);
            if (gotFadeIn != expFadeIn || gotStay != expStay || gotFadeOut != expFadeOut
                || !consumed) {
                ++mismatches;
                std::cerr << "DEC-MISMATCH hex=" << inHex
                          << " got fadeIn=" << gotFadeIn << " stay=" << gotStay
                          << " fadeOut=" << gotFadeOut << " remaining=" << dec.remaining()
                          << " want fadeIn=" << expFadeIn << " stay=" << expStay
                          << " fadeOut=" << expFadeOut << "\n";
            }
        }
    }

    std::cout << "PktSetTitlesAnimationParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
