// Byte-exact parity for net.minecraft.network.protocol.game.ServerboundEditBookPacket
// against the REAL STREAM_CODEC (tools/PktEditBookParity.java ground truth).
//
// The packet is a record (int slot, List<String> pages, Optional<String> title).
// Its STREAM_CODEC is StreamCodec.composite over, IN ORDER:
//     ByteBufCodecs.VAR_INT                                          -> slot
//     ByteBufCodecs.stringUtf8(1024).apply(ByteBufCodecs.list(100))  -> pages
//     ByteBufCodecs.stringUtf8(32).apply(ByteBufCodecs::optional)    -> title
//
// Wire form, exactly (ByteBufCodecs.java):
//     writeVarInt(slot);
//     writeVarInt(pages.size());                 // collection codec: VarInt count
//     for page in pages: writeString(page, 1024) // Utf8String.write, char-limit 1024
//     writeBool(title.has_value());              // optional codec: bool flag
//     if title: writeString(*title, 32);         // Utf8String.write, char-limit 32
// No packet-type id is part of the codec bytes (framing lives outside the codec).
//
// We reuse the certified mc::net::PacketBuffer (the FriendlyByteBuf port):
// writeVarInt == VarInt.write, writeBool == writeBoolean, writeString(s, maxLen) ==
// Utf8String.write (VarInt byte-length prefix + UTF-8 bytes; maxLen caps both the
// UTF-16 char count and the byte length to maxLen*3).
//
// For each ENC case we (a) ENCODE and require the bytes match Java's hex exactly,
// (b) check the byte count == readableBytes(), and (c) DECODE the Java bytes back
// through PacketBuffer and require slot/pages/title round-trip identically.
//
//   pkt_edit_book_parity [--cases mcpp/build/pkt_edit_book.tsv]
#include "../../PacketBuffer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
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
std::string unhexStr(const std::string& s) {
    auto b = unhex(s);
    return std::string(b.begin(), b.end());
}
// Split on a single-char delimiter (no escaping; fields are hex/ints, so safe).
std::vector<std::string> split(const std::string& s, char d) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == d) { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/pkt_edit_book.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        // ENC \t name \t slot \t pagesHex(CSV; "-"=empty) \t titleHex("-"=absent, "+"<hex>=present) \t readableBytes \t hex
        std::istringstream ss(line);
        std::string kind, name, slotStr, pagesField, titleField, readableStr, expHex;
        if (!std::getline(ss, kind, '\t')) continue;
        if (kind != "ENC") continue;
        if (!std::getline(ss, name, '\t')) continue;
        if (!std::getline(ss, slotStr, '\t')) continue;
        if (!std::getline(ss, pagesField, '\t')) continue;
        if (!std::getline(ss, titleField, '\t')) continue;
        if (!std::getline(ss, readableStr, '\t')) continue;
        if (!std::getline(ss, expHex)) continue;
        ++cases;

        int32_t slot = (int32_t)std::stol(slotStr);

        std::vector<std::string> pages;
        if (pagesField != "-") {
            for (const auto& ph : split(pagesField, ','))
                pages.push_back(unhexStr(ph));
        }

        std::optional<std::string> title;
        if (titleField != "-") {
            // "+" marker prefix distinguishes a present-empty title from an absent one.
            std::string th = titleField;
            if (!th.empty() && th[0] == '+') th.erase(0, 1);
            title = unhexStr(th);
        }

        size_t readable = (size_t)std::stoull(readableStr);

        bool ok = true;

        // (a) ENCODE: reproduce the codec exactly.
        PacketBuffer enc;
        try {
            enc.writeVarInt(slot);
            enc.writeVarInt((int32_t)pages.size());
            for (const auto& p : pages) enc.writeString(p, 1024);
            enc.writeBool(title.has_value());
            if (title.has_value()) enc.writeString(*title, 32);
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

        // (b) DECODE the Java bytes -> slot/pages/title round trip.
        try {
            PacketBuffer dec(unhex(expHex));
            int32_t gotSlot = dec.readVarInt();
            int32_t pageCount = dec.readVarInt();
            std::vector<std::string> gotPages;
            for (int32_t i = 0; i < pageCount; ++i) gotPages.push_back(dec.readString(1024));
            std::optional<std::string> gotTitle;
            if (dec.readBool()) gotTitle = dec.readString(32);

            if (gotSlot != slot) {
                std::cerr << "DECODE-SLOT-MISMATCH " << name << " got " << gotSlot
                          << " want " << slot << "\n";
                ok = false;
            }
            if (gotPages != pages) {
                std::cerr << "DECODE-PAGES-MISMATCH " << name << "\n";
                ok = false;
            }
            if (gotTitle != title) {
                std::cerr << "DECODE-TITLE-MISMATCH " << name << "\n";
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

    std::cout << "PktEditBookParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
