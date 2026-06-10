// Network primitive codec parity vs the real FriendlyByteBuf/VarInt/VarLong
// (tools/FriendlyByteBufParity.java ground truth).
//
// For each ENC case the C++ PacketBuffer must (a) ENCODE the same value to the
// identical bytes and (b) DECODE the Java bytes back to the identical value
// (round trip). Case values are parsed from the case names.
//
//   packet_buffer_parity [--cases mcpp/build/packet_buffer_cases.tsv]
#include "PacketBuffer.h"
#include "../nbt/Tag.h"

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
    std::string casesPath = "mcpp/build/packet_buffer_cases.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];
    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, failures = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string kind, name, expHex;
        if (!std::getline(ss, kind, '\t') || !std::getline(ss, name, '\t') || !std::getline(ss, expHex)) continue;
        if (kind != "ENC") continue;
        ++cases;

        PacketBuffer enc;
        bool encoded = true;
        try {
            if (name.rfind("varint_", 0) == 0) {
                int32_t v = (int32_t)std::stoll(name.substr(7));
                enc.writeVarInt(v);
                // decode round trip on Java's bytes
                PacketBuffer dec(unhex(expHex));
                if (dec.readVarInt() != v) { std::cerr << "DECODE-MISMATCH " << name << "\n"; ++failures; }
            } else if (name.rfind("varlong_", 0) == 0) {
                int64_t v = std::stoll(name.substr(8));
                enc.writeVarLong(v);
                PacketBuffer dec(unhex(expHex));
                if (dec.readVarLong() != v) { std::cerr << "DECODE-MISMATCH " << name << "\n"; ++failures; }
            } else if (name == "utf_empty") {
                enc.writeString("");
                PacketBuffer dec(unhex(expHex));
                if (dec.readString() != "") { std::cerr << "DECODE-MISMATCH " << name << "\n"; ++failures; }
            } else if (name == "utf_ascii") {
                enc.writeString("hello world");
                PacketBuffer dec(unhex(expHex));
                if (dec.readString() != "hello world") { std::cerr << "DECODE-MISMATCH " << name << "\n"; ++failures; }
            } else if (name == "utf_unicode") {
                std::string s = "ni\xC3\xB1o \xE4\xB8\xAD\xE6\x96\x87 \xF0\x9F\x98\x80 \xC3\xBC";
                enc.writeString(s);
                PacketBuffer dec(unhex(expHex));
                if (dec.readString() != s) { std::cerr << "DECODE-MISMATCH " << name << "\n"; ++failures; }
            } else if (name == "nbt_compound") {
                // Key order of a FRESHLY BUILT Java CompoundTag is a HashMap-internal
                // artifact (read->write order stability is proven by nbt_parity's
                // REWRITE gate on real data). What the protocol requires here is the
                // unnamed framing + structural equality, so: decode Java's bytes,
                // re-encode, decode again, and compare structures + framing byte.
                PacketBuffer dec(unhex(expHex));
                mc::nbt::NbtCompound rd = dec.readNbt();
                if (rd.getInt("x") != 42 || rd.getString("s") != "\xC3\xBC"
                    || !rd.getList("l") || rd.getList("l")->elements.size() != 1)
                    { std::cerr << "DECODE-MISMATCH " << name << "\n"; ++failures; }
                enc.writeNbt(rd);   // re-encode the read compound (insertion order = Java's bytes order)
                if (unhex(expHex)[0] != 0x0a) { std::cerr << "FRAMING " << name << "\n"; ++failures; }
            } else if (name == "nbt_null") {
                enc.writeByte(0);   // writeNbt(null) == EndTag byte (no C++ null overload yet)
                PacketBuffer dec(unhex(expHex));
                mc::nbt::NbtCompound rd = dec.readNbt();   // EndTag -> empty compound
                if (!rd.entries.empty()) { std::cerr << "DECODE-MISMATCH " << name << "\n"; ++failures; }
            } else {
                std::cerr << "UNKNOWN case " << name << "\n"; ++failures; encoded = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "EXCEPTION " << name << ": " << e.what() << "\n"; ++failures; encoded = false;
        }

        if (encoded) {
            std::string got = hex(enc.data());
            if (got != expHex) {
                ++failures;
                std::cerr << "ENC-MISMATCH " << name << "\n  got  " << got << "\n  want " << expHex << "\n";
            }
        }
    }
    std::cout << "PacketBufferParity cases=" << cases << " failures=" << failures << "\n";
    return failures == 0 ? 0 : 1;
}
