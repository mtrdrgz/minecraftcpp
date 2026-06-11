// Byte-exact parity for net.minecraft.network.protocol.common.ClientboundCustomReportDetailsPacket
// vs the REAL ClientboundCustomReportDetailsPacket.STREAM_CODEC (tools/CustomReportDetailsParity.java).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src):
//   record ClientboundCustomReportDetailsPacket(Map<String,String> details)
//   STREAM_CODEC = StreamCodec.composite(DETAILS_STREAM_CODEC, ::details, ::new)
//        -> body only, NO packet-id prefix.
//   DETAILS_STREAM_CODEC = ByteBufCodecs.map(HashMap::new,
//                              ByteBufCodecs.stringUtf8(128),   // key,   maxLength 128
//                              ByteBufCodecs.stringUtf8(4096),  // value, maxLength 4096
//                              32)                              // MAX_DETAIL_COUNT
//   ByteBufCodecs.map.encode (ByteBufCodecs.java:460-466):
//       writeCount(size) == VarInt.write(size)   (plain LEB128, no zig-zag)
//       map.forEach((k,v) -> { stringUtf8.encode(k); stringUtf8.encode(v); });
//   stringUtf8(n).encode -> Utf8String.write (Utf8String.java:35-55):
//       VarInt.write(utf8ByteLen);  writeBytes(utf8Bytes);   == FriendlyByteBuf.writeUtf
//
//   So the body is exactly:
//       VarInt(size)
//       for each entry (map iteration order):
//           VarInt(utf8ByteLen(key))   key UTF-8 bytes
//           VarInt(utf8ByteLen(value)) value UTF-8 bytes
//
//   The Java GT constructs the packet from a LinkedHashMap so map.forEach() follows
//   INSERTION ORDER; the TSV lists entries in that same order, and this test replays
//   writeVarInt(size) + per-entry writeUtf(k)+writeUtf(v) through mc::net::PacketBuffer
//   (PacketBuffer::writeString IS FriendlyByteBuf.writeUtf: VarInt(byteLen)+UTF-8 bytes).
//
//   pkt_custom_report_details_cb_parity [--cases mcpp/build/pkt_custom_report_details.tsv]
//
// Row: ENC <name> <size> [<kHexUtf8> <vHexUtf8>]*size <bytesN> <hexBytes>
//      key/value carried as raw-UTF-8 hex; empty string emitted as the token "_".
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

// Decode a raw-UTF-8 hex field, with "_" meaning the empty string.
std::string fieldToUtf8(const std::string& tok) {
    if (tok == "_") return std::string();
    std::vector<uint8_t> b = unhex(tok);
    return std::string(b.begin(), b.end());
}

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_custom_report_details.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> col = splitTabs(line);
        // Layout: ENC name size [k v]*size bytesN hexBytes
        //         0    1    2    3 .. 3+2*size-1     -2     -1
        if (col.size() < 5) continue;
        if (col[0] != "ENC") continue;

        const std::string& name = col[1];
        int size = std::stoi(col[2]);

        size_t need = 3 + (size_t)2 * (size_t)size + 2; // entries + bytesN + hexBytes
        if (col.size() != need) {
            ++mismatches;
            std::cerr << "FIELD-COUNT-MISMATCH " << name << " cols=" << col.size()
                      << " want=" << need << " (size=" << size << ")\n";
            continue;
        }
        ++cases;

        std::vector<std::pair<std::string, std::string>> entries;
        entries.reserve((size_t)size);
        for (int e = 0; e < size; ++e) {
            const std::string& kTok = col[3 + 2 * (size_t)e];
            const std::string& vTok = col[3 + 2 * (size_t)e + 1];
            entries.emplace_back(fieldToUtf8(kTok), fieldToUtf8(vTok));
        }

        size_t expBytes = (size_t)std::stoul(col[col.size() - 2]);
        const std::string& expHex = col[col.size() - 1];

        // (1) ENCODE the body through PacketBuffer in EXACT field order:
        //     writeVarInt(size) then per entry writeUtf(key)+writeUtf(value).
        //     writeString(s, maxLen) == FriendlyByteBuf.writeUtf: VarInt(byteLen)+UTF-8 bytes.
        PacketBuffer enc;
        enc.writeVarInt((int32_t)size);
        for (const auto& [k, v] : entries) {
            enc.writeString(k, 128);
            enc.writeString(v, 4096);
        }

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back through PacketBuffer: VarInt(size) then size
        //     (readUtf key, readUtf value) pairs must reproduce our entries with nothing left.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t bSize = dec.readVarInt();
        bool ok = (bSize == size);
        if (ok) {
            for (int e = 0; e < size && ok; ++e) {
                std::string bk = dec.readString(128);
                std::string bv = dec.readString(4096);
                ok = ok && (bk == entries[(size_t)e].first) && (bv == entries[(size_t)e].second);
            }
        }
        ok = ok && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " size(got=" << bSize << " want=" << size << ")"
                      << " remaining=" << dec.remaining() << "\n";
            continue;
        }
    }

    std::cout << "PktCustomReportDetailsParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
