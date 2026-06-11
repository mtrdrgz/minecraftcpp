// Byte-exact parity gate for
// net.minecraft.network.protocol.game.ClientboundRecipeBookRemovePacket.
//
// Real codec (ClientboundRecipeBookRemovePacket.STREAM_CODEC, 26.1.2) is
//   StreamCodec.composite(
//       RecipeDisplayId.STREAM_CODEC.apply(ByteBufCodecs.list()),
//       ClientboundRecipeBookRemovePacket::recipes,
//       ClientboundRecipeBookRemovePacket::new)
// over a plain ByteBuf, so the wire is just the body, no packet-id prefix.
//
// RecipeDisplayId is `record RecipeDisplayId(int index)` whose STREAM_CODEC is
//   StreamCodec.composite(ByteBufCodecs.VAR_INT, RecipeDisplayId::index, RecipeDisplayId::new)
// -> a single plain VarInt (LEB128, NO zig-zag; negatives encode to 5 bytes).
//
// ByteBufCodecs.list() -> collection(ArrayList::new, elementCodec):
//   encode = writeCount(size) [= VarInt(size)] then each element.encode.
//
// So the whole packet body is exactly:
//   VarInt(recipes.size())  then for each:  VarInt(recipe.index())
// which the certified mc::net::PacketBuffer (the FriendlyByteBuf port) implements
// 1:1 via writeVarInt. VarInt.write is plain LEB128 (PacketBuffer.writeVarInt is the
// certified 1:1 port).
//
// This test rebuilds each ground-truth row through PacketBuffer in the exact codec
// order and requires the produced bytes (and size) to match the GT hex, then decodes
// the GT bytes back through PacketBuffer and requires every field to round-trip
// identically.
//
//   pkt_recipe_book_remove_cb_parity --cases mcpp/build/pkt_recipe_book_remove.tsv

#include "../../PacketBuffer.h"

#include <cstdint>
#include <cstdio>
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

// Parse a comma-joined decimal int32 list, or "-" for an empty list.
std::vector<int32_t> parseIds(const std::string& field) {
    std::vector<int32_t> ids;
    if (field == "-" || field.empty()) return ids;
    size_t i = 0;
    while (i < field.size()) {
        size_t comma = field.find(',', i);
        std::string tok = (comma == std::string::npos)
                              ? field.substr(i)
                              : field.substr(i, comma - i);
        ids.push_back((int32_t)std::stoll(tok));
        if (comma == std::string::npos) break;
        i = comma + 1;
    }
    return ids;
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
        // ENC \t name \t count \t idsCsv \t readableBytes \t wireHex
        if (f.size() != 6) {
            std::fprintf(stderr, "malformed row (cols=%zu): %s\n", f.size(), line.c_str());
            mismatches++;
            cases++;
            continue;
        }

        const std::string& name = f[1];
        int32_t  expectCount    = (int32_t)std::stoll(f[2]);
        std::vector<int32_t> ids = parseIds(f[3]);
        size_t   expectReadable = (size_t)std::stoull(f[4]);
        std::string expectHex   = f[5];

        cases++;

        if ((int32_t)ids.size() != expectCount) {
            std::fprintf(stderr,
                "MISMATCH(parse) %s: parsed %zu ids, count column says %d\n",
                name.c_str(), ids.size(), expectCount);
            mismatches++;
            continue;
        }

        // --- Encode through PacketBuffer in exact codec order. ---
        mc::net::PacketBuffer buf;
        buf.writeVarInt((int32_t)ids.size());     // ByteBufCodecs.writeCount -> VarInt(size)
        for (int32_t v : ids) buf.writeVarInt(v);  // RecipeDisplayId index -> VarInt

        std::string gotHex = toHexLower(buf.data());
        if (gotHex != expectHex || buf.data().size() != expectReadable) {
            std::fprintf(stderr,
                "MISMATCH(enc) %s: bytes=%zu/%zu\n  got  %s\n  want %s\n",
                name.c_str(), buf.data().size(), expectReadable,
                gotHex.c_str(), expectHex.c_str());
            mismatches++;
            continue;
        }

        // --- Decode the expected bytes back and require fields round-trip. ---
        std::vector<uint8_t> raw = fromHex(expectHex);
        mc::net::PacketBuffer rd(raw);
        bool rtOk = true;
        int32_t rCount = rd.readVarInt();
        if (rCount != (int32_t)ids.size()) rtOk = false;
        for (size_t i = 0; rtOk && i < ids.size(); i++) {
            int32_t rIdx = rd.readVarInt();
            if (rIdx != ids[i]) rtOk = false;
        }
        rtOk = rtOk && (rd.remaining() == 0);
        if (!rtOk) {
            std::fprintf(stderr,
                "MISMATCH(dec) %s: rCount=%d expected=%zu rem=%zu\n",
                name.c_str(), rCount, ids.size(), rd.remaining());
            mismatches++;
            continue;
        }
    }

    std::printf("PktRecipeBookRemoveParity checks=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
