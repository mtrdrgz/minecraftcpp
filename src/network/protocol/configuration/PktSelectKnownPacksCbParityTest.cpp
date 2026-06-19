// Byte-exact parity gate for
// net.minecraft.network.protocol.configuration.ClientboundSelectKnownPacks.
//
// Real codec order (ClientboundSelectKnownPacks.STREAM_CODEC, 26.1.2):
//   record(List<KnownPack> knownPacks)
//   StreamCodec.composite( KnownPack.STREAM_CODEC.apply(ByteBufCodecs.list()), ... )
//
// ByteBufCodecs.list() == collection(ArrayList::new, KnownPack.STREAM_CODEC)
// (ByteBufCodecs.java:406-412 -> 413-435):
//   writeCount(size)  -> VarInt(size)            (ByteBufCodecs.java:399-405)
//   for each element  -> KnownPack.STREAM_CODEC.encode(...)
// (The clientbound list() has no max-size cap, unlike the serverbound list(64); the
//  *encoded bytes are identical* — the cap only bounds the decode side.)
//
// KnownPack.STREAM_CODEC (KnownPack.java:9-11) == StreamCodec.composite over:
//   ByteBufCodecs.STRING_UTF8 -> namespace
//   ByteBufCodecs.STRING_UTF8 -> id
//   ByteBufCodecs.STRING_UTF8 -> version
// STRING_UTF8 == stringUtf8(32767) -> Utf8String.write (Utf8String.java:35-55):
//   VarInt(utf8 byteLen) then the raw UTF-8 bytes.
//
// So the body is exactly: VarInt(numPacks) then per pack three Utf8 strings
// (VarInt(byteLen)+UTF-8) in namespace,id,version order. Every field reduces to
// VarInt / Utf8 string, each of which the certified mc::net::PacketBuffer (the
// FriendlyByteBuf port) implements 1:1.
//
// This test rebuilds each ground-truth row through PacketBuffer in the exact codec
// order and requires the produced bytes (and readableBytes) to match the GT hex,
// then decodes the GT bytes back through PacketBuffer and requires every field to
// round-trip identically.
//
//   pkt_select_known_packs_cb_parity --cases mcpp/build/pkt_select_known_packs_cb.tsv

#include "../../PacketBuffer.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string toHexLower(const std::vector<uint8_t>& bytes) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    out.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        out.push_back(digits[(b >> 4) & 0xF]);
        out.push_back(digits[b & 0xF]);
    }
    return out;
}

std::vector<uint8_t> fromHex(const std::string& hex) {
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        int hi = nibble(hex[i]);
        int lo = nibble(hex[i + 1]);
        if (hi < 0 || lo < 0) continue;
        out.push_back((uint8_t)((hi << 4) | lo));
    }
    return out;
}

// Decode hex into a raw UTF-8 string (used for each KnownPack field).
std::string hexToStr(const std::string& hex) {
    std::vector<uint8_t> b = fromHex(hex);
    return std::string(b.begin(), b.end());
}

// Split a tab-separated line into fields (keeps empty fields, strips a trailing CR).
std::vector<std::string> splitTab(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') {
            out.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur.push_back(c);
        }
    }
    out.push_back(cur);
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (casesPath.empty()) {
        std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
        return 2;
    }

    std::ifstream in(casesPath, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open cases file: %s\n", casesPath.c_str());
        return 2;
    }

    int cases = 0;
    int mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> f = splitTab(line);
        if (f.empty() || f[0] != "ENC") continue;
        // ENC \t numPacks \t [nsHex \t idHex \t verHex]... \t readableBytes \t hexBytes
        if (f.size() < 4) {
            std::fprintf(stderr, "malformed row (cols=%zu): %s\n", f.size(), line.c_str());
            mismatches++;
            cases++;
            continue;
        }

        int32_t numPacks = (int32_t)std::stol(f[1]);
        // Expected column count: 1 (ENC) + 1 (numPacks) + 3*numPacks + 1 (readable) + 1 (hex).
        size_t expectCols = 2 + (size_t)numPacks * 3 + 2;
        if (numPacks < 0 || f.size() != expectCols) {
            std::fprintf(stderr, "malformed row (cols=%zu expected=%zu numPacks=%d): %s\n",
                         f.size(), expectCols, numPacks, line.c_str());
            mismatches++;
            cases++;
            continue;
        }

        // Parse the per-pack (namespace, id, version) UTF-8 strings.
        std::vector<std::string> ns(numPacks), id(numPacks), ver(numPacks);
        for (int32_t p = 0; p < numPacks; p++) {
            ns[p]  = hexToStr(f[2 + p * 3 + 0]);
            id[p]  = hexToStr(f[2 + p * 3 + 1]);
            ver[p] = hexToStr(f[2 + p * 3 + 2]);
        }
        size_t  expectReadable = (size_t)std::stoull(f[2 + numPacks * 3]);
        std::string expectHex  = f[2 + numPacks * 3 + 1];

        cases++;

        // --- Encode through PacketBuffer in exact codec order. ---
        mc::net::PacketBuffer buf;
        buf.writeVarInt(numPacks);                 // writeCount(size) -> VarInt(size)
        for (int32_t p = 0; p < numPacks; p++) {
            buf.writeString(ns[p]);                // STRING_UTF8 namespace
            buf.writeString(id[p]);                // STRING_UTF8 id
            buf.writeString(ver[p]);               // STRING_UTF8 version
        }

        std::string gotHex = toHexLower(buf.data());
        if (gotHex != expectHex || buf.data().size() != expectReadable) {
            std::fprintf(stderr,
                "MISMATCH(enc) numPacks=%d: bytes=%zu/%zu\n  got  %s\n  want %s\n",
                numPacks, buf.data().size(), expectReadable,
                gotHex.c_str(), expectHex.c_str());
            mismatches++;
            continue;
        }

        // --- Decode the expected bytes back and require fields round-trip. ---
        std::vector<uint8_t> raw = fromHex(expectHex);
        mc::net::PacketBuffer rd(raw);
        int32_t rNum = rd.readVarInt();
        bool rtOk = (rNum == numPacks);
        for (int32_t p = 0; p < numPacks && rtOk; p++) {
            std::string rNs  = rd.readString();
            std::string rId  = rd.readString();
            std::string rVer = rd.readString();
            if (rNs != ns[p] || rId != id[p] || rVer != ver[p]) {
                rtOk = false;
            }
        }
        rtOk = rtOk && (rd.remaining() == 0);
        if (!rtOk) {
            std::fprintf(stderr,
                "MISMATCH(dec) numPacks=%d: rNum=%d rem=%zu\n",
                numPacks, rNum, rd.remaining());
            mismatches++;
            continue;
        }
    }

    std::printf("PktSelectKnownPacksCbParity checks=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
