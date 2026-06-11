// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundSetJigsawBlockPacket.
//
// Real codec order (ServerboundSetJigsawBlockPacket.write, 26.1.2):
//   writeBlockPos(pos)                  -> writeLong(pos.asLong())  big-endian 8-byte long
//   writeIdentifier(name)               -> writeUtf(name.toString())   = VarInt(byteLen)+UTF-8
//   writeIdentifier(target)             -> writeUtf(target.toString())
//   writeIdentifier(pool)               -> writeUtf(pool.toString())
//   writeUtf(finalState)                -> VarInt(byteLen)+UTF-8
//   writeUtf(joint.getSerializedName()) -> "rollable" (ROLLABLE) | "aligned" (ALIGNED)
//   writeVarInt(selectionPriority)
//   writeVarInt(placementPriority)
//
// writeIdentifier is just writeUtf of the "namespace:path" string, so on the wire there
// is nothing Identifier-specific — every field reduces to long/string/varInt, all of
// which the certified mc::net::PacketBuffer (the FriendlyByteBuf port) implements 1:1.
//
// This test rebuilds each ground-truth row through PacketBuffer in the exact codec order
// and requires the produced bytes (and readableBytes) to match the GT hex, then decodes
// the GT bytes back through PacketBuffer and requires every field to round-trip identically.
//
//   pkt_set_jigsaw_block_sb_parity --cases mcpp/build/pkt_set_jigsaw_block_sb.tsv

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

// Decode hex into a raw UTF-8 string (used for identifier/finalState fields).
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
        // ENC \t name \t posLong \t nameHex \t targetHex \t poolHex \t finalStateHex
        //     \t jointName \t selectionPriority \t placementPriority \t readableBytes \t hexBytes
        if (f.size() < 12) {
            std::fprintf(stderr, "malformed row (cols=%zu): %s\n", f.size(), line.c_str());
            mismatches++;
            cases++;
            continue;
        }

        const std::string& caseName = f[1];
        int64_t posLong = (int64_t)std::stoll(f[2]);
        std::string idName = hexToStr(f[3]);
        std::string idTarget = hexToStr(f[4]);
        std::string idPool = hexToStr(f[5]);
        std::string finalState = hexToStr(f[6]);
        std::string jointName = f[7];
        int32_t selectionPriority = (int32_t)std::stol(f[8]);
        int32_t placementPriority = (int32_t)std::stol(f[9]);
        size_t expectReadable = (size_t)std::stoull(f[10]);
        std::string expectHex = f[11];

        cases++;

        // --- Encode through PacketBuffer in exact codec order. ---
        mc::net::PacketBuffer buf;
        buf.writeLong(posLong);          // writeBlockPos -> writeLong(asLong)
        buf.writeString(idName);         // writeIdentifier(name)   -> writeUtf(toString)
        buf.writeString(idTarget);       // writeIdentifier(target) -> writeUtf(toString)
        buf.writeString(idPool);         // writeIdentifier(pool)   -> writeUtf(toString)
        buf.writeString(finalState);     // writeUtf(finalState)
        buf.writeString(jointName);      // writeUtf(joint.getSerializedName())
        buf.writeVarInt(selectionPriority);
        buf.writeVarInt(placementPriority);

        std::string gotHex = toHexLower(buf.data());
        if (gotHex != expectHex || buf.data().size() != expectReadable) {
            std::fprintf(stderr,
                "MISMATCH(enc) %s: bytes=%zu/%zu\n  got  %s\n  want %s\n",
                caseName.c_str(), buf.data().size(), expectReadable,
                gotHex.c_str(), expectHex.c_str());
            mismatches++;
            continue;
        }

        // --- Decode the expected bytes back and require fields round-trip. ---
        std::vector<uint8_t> raw = fromHex(expectHex);
        mc::net::PacketBuffer rd(raw);
        int64_t rPos = rd.readLong();
        std::string rName = rd.readString();
        std::string rTarget = rd.readString();
        std::string rPool = rd.readString();
        std::string rFinal = rd.readString();
        std::string rJoint = rd.readString();
        int32_t rSel = rd.readVarInt();
        int32_t rPlace = rd.readVarInt();

        bool rtOk = (rPos == posLong)
                 && (rName == idName)
                 && (rTarget == idTarget)
                 && (rPool == idPool)
                 && (rFinal == finalState)
                 && (rJoint == jointName)
                 && (rSel == selectionPriority)
                 && (rPlace == placementPriority)
                 && (rd.remaining() == 0);
        if (!rtOk) {
            std::fprintf(stderr,
                "MISMATCH(dec) %s: pos=%lld/%lld nameEq=%d targetEq=%d poolEq=%d "
                "finalEq=%d jointEq=%d sel=%d/%d place=%d/%d rem=%zu\n",
                caseName.c_str(), (long long)rPos, (long long)posLong,
                (int)(rName == idName), (int)(rTarget == idTarget),
                (int)(rPool == idPool), (int)(rFinal == finalState),
                (int)(rJoint == jointName), rSel, selectionPriority,
                rPlace, placementPriority, rd.remaining());
            mismatches++;
            continue;
        }
    }

    std::printf("PktSetJigsawBlockSbParity cases=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
