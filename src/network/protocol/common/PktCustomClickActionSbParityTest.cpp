// Byte-exact parity gate for
// net.minecraft.network.protocol.common.ServerboundCustomClickActionPacket.
//
// Real record (26.1.2):
//   ServerboundCustomClickActionPacket(Identifier id, Optional<Tag> payload)
//   STREAM_CODEC = StreamCodec.composite(
//        Identifier.STREAM_CODEC, ::id,
//        UNTRUSTED_TAG_CODEC,      ::payload, ::new)
//   UNTRUSTED_TAG_CODEC = ByteBufCodecs.optionalTagCodec(...)
//                            .apply(ByteBufCodecs.lengthPrefixed(65536))
//
// Wire, in STREAM_CODEC field order (Packet.codec => body only, no packet-id prefix):
//   1) Identifier.STREAM_CODEC = STRING_UTF8.map(Identifier::toString):
//        Utf8String.write -> VarInt(utf8 byteLen) + UTF-8 bytes of "namespace:path".
//   2) payload via lengthPrefixed(65536) over optionalTagCodec:
//        inner = FriendlyByteBuf.writeNbt(tag-or-null):
//                  null -> single byte 0x00 (EndTag, == Java's null encoding)
//                  tag  -> type byte + UNNAMED network-NBT payload (NbtIo.writeAnyTag)
//        outer = VarInt(inner.length) + inner bytes.
//
// Each row carries the canonical id string and a deterministic TAG SPEC that this test
// rebuilds (identically to the Java GT generator) into a structurally identical Tag, then
// encodes through the certified mc::net::PacketBuffer + nbt::NbtWriter — NOT echoing the
// GT's NBT bytes. The produced bytes (and readableBytes) must match the GT hex exactly.
//
//   pkt_custom_click_action_sb_parity --cases mcpp/build/pkt_custom_click_action_sb.tsv

#include "../../PacketBuffer.h"
#include "../../../nbt/Tag.h"
#include "../../../nbt/NbtIo.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <optional>
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

std::string hexToStr(const std::string& hex) {
    std::vector<uint8_t> b = fromHex(hex);
    return std::string(b.begin(), b.end());
}

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

// Rebuild the same tag the Java generator built from a spec token.
// Returns nullopt for "none" (the absent payload -> FriendlyByteBuf.writeNbt(null) -> 0x00).
// Throws (returns sentinel) on malformed spec.
struct TagResult { bool ok = false; bool present = false; mc::nbt::NbtTag tag; };

TagResult buildTag(const std::string& spec) {
    using namespace mc::nbt;
    TagResult r;
    if (spec == "none") { r.ok = true; r.present = false; return r; }
    size_t c = spec.find(':');
    std::string kind = (c == std::string::npos) ? spec : spec.substr(0, c);
    std::string rest = (c == std::string::npos) ? "" : spec.substr(c + 1);

    auto setTag = [&](NbtTag t) { r.ok = true; r.present = true; r.tag = std::move(t); };

    if (kind == "str") {
        setTag(NbtTag::string_(hexToStr(rest)));
    } else if (kind == "i8") {
        setTag(NbtTag::byte_((int8_t)std::stoi(rest)));
    } else if (kind == "i16") {
        setTag(NbtTag::short_((int16_t)std::stoi(rest)));
    } else if (kind == "i32") {
        setTag(NbtTag::int_((int32_t)std::stoll(rest)));
    } else if (kind == "i64") {
        setTag(NbtTag::long_((int64_t)std::stoll(rest)));
    } else if (kind == "f32") {
        uint32_t bits = (uint32_t)std::stoll(rest);   // Java emits a signed int32 (may be negative)
        float f; std::memcpy(&f, &bits, 4);
        // FloatTag.valueOf(data): data == 0.0F ? ZERO(+0.0F) : new FloatTag(data).
        // In IEEE compare -0.0F == 0.0F is true, so valueOf(-0.0F) canonicalizes to +0.0F.
        if (f == 0.0f) f = 0.0f;
        setTag(NbtTag::float_(f));
    } else if (kind == "f64") {
        // Java emits a signed long (decimal, possibly negative): parse as signed then bitcast.
        int64_t sbits = (int64_t)std::stoll(rest);
        uint64_t bits = (uint64_t)sbits;
        double d; std::memcpy(&d, &bits, 8);
        // DoubleTag.valueOf(data): data == 0.0 ? ZERO(+0.0) : new DoubleTag(data).
        if (d == 0.0) d = 0.0;
        setTag(NbtTag::double_(d));
    } else if (kind == "cmp1") {
        size_t c2 = rest.find(':');
        if (c2 == std::string::npos) return r;  // malformed
        std::string keyHex = rest.substr(0, c2);
        int32_t v = (int32_t)std::stoll(rest.substr(c2 + 1));
        NbtCompound ct;
        ct.put(hexToStr(keyHex), NbtTag::int_(v));
        setTag(NbtTag::compound(std::move(ct)));
    } else if (kind == "lstI") {
        int n = std::stoi(rest);
        NbtList lt;
        // identifyRawElementType(): empty list -> End(0); non-empty homogeneous int -> Int(3).
        lt.elementType = (n == 0) ? TagType::End : TagType::Int;
        for (int i = 0; i < n; i++) lt.elements.push_back(NbtTag::int_(i));
        setTag(NbtTag::list(std::move(lt)));
    } else {
        return r;  // unknown kind -> ok=false
    }
    return r;
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
        // ENC \t idString \t tagSpec \t readableBytes \t hex
        if (f.size() != 5) {
            std::fprintf(stderr, "malformed row (cols=%zu): %s\n", f.size(), line.c_str());
            mismatches++; cases++;
            continue;
        }

        const std::string& idString = f[1];
        const std::string& spec     = f[2];
        size_t expectReadable       = (size_t)std::stoull(f[3]);
        const std::string& expectHex = f[4];

        cases++;

        TagResult tr = buildTag(spec);
        if (!tr.ok) {
            std::fprintf(stderr, "bad spec '%s'\n", spec.c_str());
            mismatches++;
            continue;
        }

        // --- Encode through PacketBuffer in exact codec order. ---
        mc::net::PacketBuffer buf;

        // 1) Identifier.STREAM_CODEC -> Utf8 string of "namespace:path".
        buf.writeString(idString);

        // 2) payload via lengthPrefixed(65536) over optionalTagCodec:
        //    inner = writeNbt(tag-or-null), outer = VarInt(inner.len) + inner.
        std::vector<uint8_t> inner;
        if (!tr.present) {
            inner = mc::nbt::NbtWriter::writeAnyRoot(mc::nbt::NbtTag{});  // null -> 0x00
        } else {
            inner = mc::nbt::NbtWriter::writeAnyRoot(tr.tag);
        }
        buf.writeVarInt((int32_t)inner.size());
        buf.writeBytes(inner);

        std::string gotHex = toHexLower(buf.data());
        if (gotHex != expectHex || buf.data().size() != expectReadable) {
            std::fprintf(stderr,
                "MISMATCH(enc) id='%s' spec='%s': bytes=%zu/%zu\n  got  %s\n  want %s\n",
                idString.c_str(), spec.c_str(), buf.data().size(), expectReadable,
                gotHex.c_str(), expectHex.c_str());
            mismatches++;
            continue;
        }

        // --- Decode the expected bytes back and require fields round-trip. ---
        std::vector<uint8_t> raw = fromHex(expectHex);
        mc::net::PacketBuffer rd(raw);
        std::string rId = rd.readString();
        int32_t nbtLen = rd.readVarInt();
        bool rtOk = (rId == idString) && (nbtLen == (int32_t)inner.size());
        if (rtOk) {
            std::vector<uint8_t> rNbt = rd.readBytes((size_t)nbtLen);
            rtOk = (rNbt == inner) && (rd.remaining() == 0);
        }
        if (!rtOk) {
            std::fprintf(stderr,
                "MISMATCH(dec) id='%s' spec='%s': rId='%s' nbtLen=%d rem=%zu\n",
                idString.c_str(), spec.c_str(), rId.c_str(), nbtLen, rd.remaining());
            mismatches++;
            continue;
        }
    }

    std::printf("PktCustomClickActionSbParity checks=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
