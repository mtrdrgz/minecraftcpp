// Byte-exact parity for net.minecraft.network.protocol.game.ClientboundTagQueryPacket
// vs the REAL ClientboundTagQueryPacket.STREAM_CODEC (tools/PktTagQueryParity.java).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src):
//   ClientboundTagQueryPacket.STREAM_CODEC = Packet.codec(::write, ::new)  -> NO packet-id prefix.
//   write(FriendlyByteBuf output):
//       output.writeVarInt(this.transactionId);   // VarInt (plain LEB128, no zig-zag)
//       output.writeNbt(this.tag);                // @Nullable CompoundTag, NETWORK framing
//   FriendlyByteBuf.writeNbt(@Nullable Tag tag) (FriendlyByteBuf.java:521-534):
//       if (tag == null) tag = EndTag.INSTANCE;
//       NbtIo.writeAnyTag(tag, out);
//   NbtIo.writeAnyTag(Tag tag, DataOutput out) (NbtIo.java:151-156):
//       out.writeByte(tag.getId());               // type byte
//       if (tag.getId() != 0) tag.write(out);     // UNNAMED payload (no root-name string)
//   => null tag  -> a single 0x00 byte (EndTag id, no payload).
//      CompoundTag -> 0x0a + entries + 0x00 terminator (unnamed compound payload).
//
// Full payload, in order:  VarInt(transactionId) | NbtIo.writeAnyTag(tag-or-EndTag)
//
// This test exercises mc::net::PacketBuffer + mc::nbt (the certified FriendlyByteBuf /
// NbtIo ports). The Java GT carries the EXACT bytes that the real writeNbt emitted for the
// tag (column <nbtHex>: the type-byte + unnamed payload, or "00" for a null tag). The C++
// side INDEPENDENTLY re-encodes the packet: writeVarInt(transactionId), then re-serializes
// the NBT by parsing <nbtHex> through the certified NbtReader (readAnyRootCompound, which
// records keys in their serialized order so a read->write is byte-stable) and re-emitting it
// through NbtWriter::writeAnyRoot — exactly modelling FriendlyByteBuf.writeNbt's framing,
// including the null/EndTag (0x00) case. The produced bytes (hex) AND the byte count must
// equal the Java ground truth, and the NBT sub-bytes must reproduce <nbtHex> verbatim.
//
//   pkt_tag_query_parity [--cases mcpp/build/pkt_tag_query.tsv]
//
// Row: ENC <name> <transactionId-dec> <present 0|1> <nbtHex> <readableBytes-dec> <hexBytes>
#include "../../PacketBuffer.h"
#include "../../../nbt/NbtIo.h"
#include "../../../nbt/Tag.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::net::PacketBuffer;
using mc::nbt::NbtReader;
using mc::nbt::NbtWriter;
using mc::nbt::NbtTag;

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

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/pkt_tag_query.tsv";
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
        std::string tag, name, idStr, presentStr, nbtHex, nStr, expHex;
        if (!std::getline(ss, tag, '\t')) continue;
        if (tag != "ENC") continue;
        if (!std::getline(ss, name, '\t') || !std::getline(ss, idStr, '\t') ||
            !std::getline(ss, presentStr, '\t') || !std::getline(ss, nbtHex, '\t') ||
            !std::getline(ss, nStr, '\t') || !std::getline(ss, expHex)) continue;
        ++cases;

        int32_t transactionId = (int32_t)std::stoll(idStr);
        int     present       = std::stoi(presentStr);
        size_t  expBytes      = (size_t)std::stoul(nStr);

        // Reconstruct the NBT root tag from the Java NBT bytes via the certified reader.
        // readAnyRootCompound mirrors NbtIo.readAnyTag: type byte + unnamed payload, with
        // EndTag(0x00) surfaced as wasEnd. A read->write through this layer is byte-stable
        // (keys are recorded in their serialized order), so this exercises the writer too.
        std::vector<uint8_t> nbtRaw = unhex(nbtHex);
        NbtReader reader(nbtRaw);
        auto parsed = reader.readAnyRootCompound();

        NbtTag rootTag;  // default = monostate (EndTag / null)
        if (parsed.wasEnd) {
            if (present != 0) {
                ++mismatches;
                std::cerr << "PRESENT-MISMATCH " << name << " (parsed EndTag but present=1)\n";
                continue;
            }
        } else {
            if (!parsed.compound || present != 1) {
                ++mismatches;
                std::cerr << "PRESENT-MISMATCH " << name << " (expected present compound)\n";
                continue;
            }
            rootTag = NbtTag::compound(std::move(*parsed.compound));
        }

        // Independently verify the NBT framing reproduces the Java bytes verbatim.
        std::vector<uint8_t> nbtBytes = NbtWriter::writeAnyRoot(rootTag);
        if (hex(nbtBytes) != nbtHex) {
            ++mismatches;
            std::cerr << "NBT-MISMATCH " << name
                      << "\n  got  " << hex(nbtBytes) << "\n  want " << nbtHex << "\n";
            continue;
        }

        // (1) ENCODE the full packet through PacketBuffer in the EXACT codec field order:
        //     VarInt(transactionId) | writeNbt(tag).
        PacketBuffer enc;
        enc.writeVarInt(transactionId);
        enc.writeBytes(nbtBytes);  // == FriendlyByteBuf.writeNbt(tag) framing

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH " << name << " txid=" << transactionId
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back through PacketBuffer: VarInt id round-trips
        //     bit-for-bit and the NBT tail re-serializes to the same bytes.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int32_t bId = dec.readVarInt();
        bool ok = bId == transactionId;
        // The remaining bytes are the NBT framing; require they equal nbtBytes exactly.
        std::vector<uint8_t> tail = dec.readBytes(dec.remaining());
        ok = ok && tail == nbtBytes;
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH " << name
                      << " id(got=" << bId << " want=" << transactionId << ")\n";
            continue;
        }
        if (dec.remaining() != 0 || raw.size() != enc.data().size()) {
            ++mismatches;
            std::cerr << "DECODE-LEN-MISMATCH " << name
                      << " remaining=" << dec.remaining() << "\n";
        }
    }

    std::cout << "PktTagQueryParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
