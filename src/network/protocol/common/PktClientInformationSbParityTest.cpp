// Byte-exact parity for
// net.minecraft.network.protocol.common.ServerboundClientInformationPacket
// against the REAL STREAM_CODEC (tools/PktClientInformationSbParity.java GT).
//
// The packet wraps a ClientInformation record; its codec delegates entirely to
// ClientInformation.write. Field order is VERBATIM from
// 26.1.2/src/net/minecraft/server/level/ClientInformation.java:35-45:
//     writeUtf(language);              // VarInt(byteLen)+UTF-8 bytes, maxLen 16
//     writeByte(viewDistance);         // 1 byte (low 8 bits of the int)
//     writeEnum(chatVisibility);       // VarInt(ordinal)
//     writeBoolean(chatColors);        // 1 byte
//     writeByte(modelCustomisation);   // 1 byte (low 8 bits)
//     writeEnum(mainHand);             // VarInt(ordinal)
//     writeBoolean(textFilteringEnabled); // 1 byte
//     writeBoolean(allowsListing);     // 1 byte
//     writeEnum(particleStatus);       // VarInt(ordinal)
//
// Read side: readUtf(16), readByte() [SIGNED], readEnum, readBoolean,
// readUnsignedByte() [UNSIGNED], readEnum, readBoolean, readBoolean, readEnum.
// So viewDistance round-trips through a signed byte and modelCustomisation
// through an unsigned byte. We reuse the certified mc::net::PacketBuffer
// (the FriendlyByteBuf port): writeString==writeUtf, writeByte (1 raw byte),
// writeVarInt(ordinal)==writeEnum, writeBool==writeBoolean.
//
// For each ENC case we (a) ENCODE the fields in codec order and require bytes
// match Java's hex exactly + readableBytes match, then (b) DECODE the Java bytes
// back through PacketBuffer and require every field round-trips identically.
//
//   pkt_client_information_sb_parity [--cases mcpp/build/pkt_client_information_sb.tsv]
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
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_client_information_sb.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t langHex \t viewDist \t chatVisOrd \t chatColors
        //     \t modelCust \t mainHandOrd \t textFilter \t allowsListing
        //     \t particleOrd \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, langHex, viewDistStr, chatVisStr, chatColorsStr,
            modelCustStr, mainHandStr, textFilterStr, allowsListingStr,
            particleStr, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, langHex, '\t')) continue;
        if (!std::getline(ss, viewDistStr, '\t')) continue;
        if (!std::getline(ss, chatVisStr, '\t')) continue;
        if (!std::getline(ss, chatColorsStr, '\t')) continue;
        if (!std::getline(ss, modelCustStr, '\t')) continue;
        if (!std::getline(ss, mainHandStr, '\t')) continue;
        if (!std::getline(ss, textFilterStr, '\t')) continue;
        if (!std::getline(ss, allowsListingStr, '\t')) continue;
        if (!std::getline(ss, particleStr, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        // language column is UTF-8 HEX; decode to the exact byte string the
        // packet carries so writeUtf is exercised on real UTF-8.
        std::vector<uint8_t> langBytes = unhex(langHex);
        std::string language(langBytes.begin(), langBytes.end());

        int viewDist     = std::stoi(viewDistStr);
        int chatVisOrd   = std::stoi(chatVisStr);
        bool chatColors  = std::stoi(chatColorsStr) != 0;
        int modelCust    = std::stoi(modelCustStr);
        int mainHandOrd  = std::stoi(mainHandStr);
        bool textFilter  = std::stoi(textFilterStr) != 0;
        bool allowsList  = std::stoi(allowsListingStr) != 0;
        int particleOrd  = std::stoi(particleStr);
        size_t readable  = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly, field-by-field in order.
        PacketBuffer enc;
        try {
            enc.writeString(language, 16);              // writeUtf(language) maxLen 16
            enc.writeByte((uint8_t)(viewDist & 0xff));  // writeByte: low 8 bits
            enc.writeVarInt(chatVisOrd);                // writeEnum(chatVisibility)
            enc.writeBool(chatColors);                  // writeBoolean(chatColors)
            enc.writeByte((uint8_t)(modelCust & 0xff)); // writeByte: low 8 bits
            enc.writeVarInt(mainHandOrd);               // writeEnum(mainHand)
            enc.writeBool(textFilter);                  // writeBoolean(textFilteringEnabled)
            enc.writeBool(allowsList);                  // writeBoolean(allowsListing)
            enc.writeVarInt(particleOrd);               // writeEnum(particleStatus)
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

        // (b) DECODE the Java bytes back through PacketBuffer; require every
        //     field round-trips. viewDistance via signed byte (readByte),
        //     modelCustomisation via unsigned byte (readUnsignedByte).
        try {
            PacketBuffer dec(unhex(expHex));
            std::string gotLang = dec.readString(16);
            int gotViewDist     = (int)(int8_t)dec.readByte();   // signed byte
            int gotChatVisOrd   = dec.readVarInt();
            bool gotChatColors  = dec.readBool();
            int gotModelCust    = (int)dec.readByte();           // unsigned byte (uint8_t)
            int gotMainHandOrd  = dec.readVarInt();
            bool gotTextFilter  = dec.readBool();
            bool gotAllowsList  = dec.readBool();
            int gotParticleOrd  = dec.readVarInt();

            int expViewDist  = (int)(int8_t)(viewDist & 0xff);   // signed-byte round-trip
            int expModelCust = modelCust & 0xff;                 // unsigned-byte round-trip

            if (gotLang != language)              { std::cerr << "DEC-LANG "      << name << "\n"; ok = false; }
            if (gotViewDist != expViewDist)       { std::cerr << "DEC-VIEWDIST "  << name << " got " << gotViewDist  << " want " << expViewDist  << "\n"; ok = false; }
            if (gotChatVisOrd != chatVisOrd)      { std::cerr << "DEC-CHATVIS "   << name << "\n"; ok = false; }
            if (gotChatColors != chatColors)      { std::cerr << "DEC-CHATCOLOR " << name << "\n"; ok = false; }
            if (gotModelCust != expModelCust)     { std::cerr << "DEC-MODELCUST " << name << " got " << gotModelCust << " want " << expModelCust << "\n"; ok = false; }
            if (gotMainHandOrd != mainHandOrd)    { std::cerr << "DEC-MAINHAND "  << name << "\n"; ok = false; }
            if (gotTextFilter != textFilter)      { std::cerr << "DEC-TEXTFILT "  << name << "\n"; ok = false; }
            if (gotAllowsList != allowsList)      { std::cerr << "DEC-ALLOWLIST " << name << "\n"; ok = false; }
            if (gotParticleOrd != particleOrd)    { std::cerr << "DEC-PARTICLE "  << name << "\n"; ok = false; }
            if (dec.remaining() != 0)             { std::cerr << "DEC-TRAILING "  << name << " remaining " << dec.remaining() << "\n"; ok = false; }
        } catch (const std::exception& e) {
            std::cerr << "DECODE-EXCEPTION " << name << ": " << e.what() << "\n";
            ok = false;
        }

        if (!ok) ++mismatches;
    }

    std::cout << "PktClientInformationSbParity cases=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
