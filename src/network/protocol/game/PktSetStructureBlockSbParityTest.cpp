// Byte-exact parity gate for net.minecraft.network.protocol.game.ServerboundSetStructureBlockPacket.
//
// Real codec order (ServerboundSetStructureBlockPacket.write, 26.1.2):
//   writeBlockPos(pos)      -> writeLong(pos.asLong())   (big-endian 8-byte long)
//   writeEnum(updateType)   -> writeVarInt(ordinal())    UPDATE_DATA=0 SAVE_AREA=1 LOAD_AREA=2 SCAN_AREA=3
//   writeEnum(mode)         -> writeVarInt(ordinal())    SAVE=0 LOAD=1 CORNER=2 DATA=3
//   writeUtf(name)          -> VarInt(utf8 byteLen)+UTF-8
//   writeByte(offset.getX()) writeByte(offset.getY()) writeByte(offset.getZ())   (low 8 bits)
//   writeByte(size.getX())   writeByte(size.getY())   writeByte(size.getZ())
//   writeEnum(mirror)       -> writeVarInt(ordinal())    NONE=0 LEFT_RIGHT=1 FRONT_BACK=2
//   writeEnum(rotation)     -> writeVarInt(ordinal())    NONE=0 CLOCKWISE_90=1 CLOCKWISE_180=2 COUNTERCLOCKWISE_90=3
//   writeUtf(data)          -> VarInt(utf8 byteLen)+UTF-8
//   writeFloat(integrity)   -> 4 big-endian bytes of floatToRawIntBits(integrity)
//   writeVarLong(seed)      -> LEB128, up to 10 bytes
//   writeByte(flags)        -> bit0=ignoreEntities bit1=showAir bit2=showBoundingBox bit3=strict
//
// Every field reduces to long/varInt/string/byte/float/varLong, each of which the
// certified mc::net::PacketBuffer (the FriendlyByteBuf port) implements 1:1.
//
// This test rebuilds each ground-truth row through PacketBuffer in the exact codec
// order and requires the produced bytes (and readableBytes) to match the GT hex,
// then decodes the GT bytes back through PacketBuffer and requires every field to
// round-trip identically.
//
//   pkt_set_structure_block_sb_parity --cases mcpp/build/pkt_set_structure_block_sb.tsv

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

// Decode hex into a raw UTF-8 string (used for the name/data fields).
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
        // ENC \t name \t posLong \t updateTypeOrd \t modeOrd \t nameHex
        //     \t offX \t offY \t offZ \t sizeX \t sizeY \t sizeZ \t mirrorOrd \t rotationOrd
        //     \t dataHex \t integrityBits \t seed \t flags \t readableBytes \t hexBytes
        if (f.size() < 20) {
            std::fprintf(stderr, "malformed row (cols=%zu): %s\n", f.size(), line.c_str());
            mismatches++;
            cases++;
            continue;
        }

        const std::string& caseName = f[1];
        int64_t posLong       = (int64_t)std::stoll(f[2]);
        int32_t updateTypeOrd = (int32_t)std::stol(f[3]);
        int32_t modeOrd       = (int32_t)std::stol(f[4]);
        std::string name      = hexToStr(f[5]);
        int32_t offX          = (int32_t)std::stol(f[6]);
        int32_t offY          = (int32_t)std::stol(f[7]);
        int32_t offZ          = (int32_t)std::stol(f[8]);
        int32_t sizeX         = (int32_t)std::stol(f[9]);
        int32_t sizeY         = (int32_t)std::stol(f[10]);
        int32_t sizeZ         = (int32_t)std::stol(f[11]);
        int32_t mirrorOrd     = (int32_t)std::stol(f[12]);
        int32_t rotationOrd   = (int32_t)std::stol(f[13]);
        std::string data      = hexToStr(f[14]);
        int32_t integrityBits = (int32_t)std::stol(f[15]);
        int64_t seed          = (int64_t)std::stoll(f[16]);
        int32_t flags         = (int32_t)std::stol(f[17]);
        size_t  expectReadable = (size_t)std::stoull(f[18]);
        std::string expectHex  = f[19];

        // integrity float == the bit pattern Float.floatToRawIntBits produced.
        float integrity;
        uint32_t ubits = (uint32_t)integrityBits;
        std::memcpy(&integrity, &ubits, 4);

        cases++;

        // --- Encode through PacketBuffer in exact codec order. ---
        mc::net::PacketBuffer buf;
        buf.writeLong(posLong);                 // writeBlockPos -> writeLong(asLong)
        buf.writeVarInt(updateTypeOrd);         // writeEnum(updateType)
        buf.writeVarInt(modeOrd);               // writeEnum(mode)
        buf.writeString(name);                  // writeUtf(name)
        buf.writeByte((uint8_t)(offX & 0xFF));  // writeByte(offset.getX())
        buf.writeByte((uint8_t)(offY & 0xFF));  // writeByte(offset.getY())
        buf.writeByte((uint8_t)(offZ & 0xFF));  // writeByte(offset.getZ())
        buf.writeByte((uint8_t)(sizeX & 0xFF)); // writeByte(size.getX())
        buf.writeByte((uint8_t)(sizeY & 0xFF)); // writeByte(size.getY())
        buf.writeByte((uint8_t)(sizeZ & 0xFF)); // writeByte(size.getZ())
        buf.writeVarInt(mirrorOrd);             // writeEnum(mirror)
        buf.writeVarInt(rotationOrd);           // writeEnum(rotation)
        buf.writeString(data);                  // writeUtf(data)
        buf.writeFloat(integrity);              // writeFloat(integrity)
        buf.writeVarLong(seed);                 // writeVarLong(seed)
        buf.writeByte((uint8_t)(flags & 0xFF)); // writeByte(flags)

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
        int64_t rPos      = rd.readLong();
        int32_t rUpdate   = rd.readVarInt();
        int32_t rMode     = rd.readVarInt();
        std::string rName = rd.readString();
        int8_t rOffX      = (int8_t)rd.readByte();
        int8_t rOffY      = (int8_t)rd.readByte();
        int8_t rOffZ      = (int8_t)rd.readByte();
        int8_t rSizeX     = (int8_t)rd.readByte();
        int8_t rSizeY     = (int8_t)rd.readByte();
        int8_t rSizeZ     = (int8_t)rd.readByte();
        int32_t rMirror   = rd.readVarInt();
        int32_t rRotation = rd.readVarInt();
        std::string rData = rd.readString();
        float rIntegrity  = rd.readFloat();
        int64_t rSeed     = rd.readVarLong();
        int32_t rFlags    = (int32_t)rd.readByte();

        uint32_t rIntBits;
        std::memcpy(&rIntBits, &rIntegrity, 4);

        bool rtOk = (rPos == posLong)
                 && (rUpdate == updateTypeOrd)
                 && (rMode == modeOrd)
                 && (rName == name)
                 && (rOffX == (int8_t)(offX & 0xFF))
                 && (rOffY == (int8_t)(offY & 0xFF))
                 && (rOffZ == (int8_t)(offZ & 0xFF))
                 && (rSizeX == (int8_t)(sizeX & 0xFF))
                 && (rSizeY == (int8_t)(sizeY & 0xFF))
                 && (rSizeZ == (int8_t)(sizeZ & 0xFF))
                 && (rMirror == mirrorOrd)
                 && (rRotation == rotationOrd)
                 && (rData == data)
                 && (rIntBits == ubits)
                 && (rSeed == seed)
                 && (rFlags == (flags & 0xFF))
                 && (rd.remaining() == 0);
        if (!rtOk) {
            std::fprintf(stderr,
                "MISMATCH(dec) %s: pos=%lld/%lld upd=%d/%d mode=%d/%d nameEq=%d "
                "mir=%d/%d rot=%d/%d dataEq=%d intBits=%08x/%08x seed=%lld/%lld "
                "flags=%d/%d rem=%zu\n",
                caseName.c_str(), (long long)rPos, (long long)posLong,
                rUpdate, updateTypeOrd, rMode, modeOrd, (int)(rName == name),
                rMirror, mirrorOrd, rRotation, rotationOrd, (int)(rData == data),
                rIntBits, ubits, (long long)rSeed, (long long)seed,
                rFlags, (flags & 0xFF), rd.remaining());
            mismatches++;
            continue;
        }
    }

    std::printf("PktSetStructureBlockSbParity cases=%d mismatches=%d\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
