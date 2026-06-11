// Byte-exact parity for net.minecraft.network.protocol.game.ServerboundChatSessionUpdatePacket
// vs the REAL ServerboundChatSessionUpdatePacket.STREAM_CODEC (tools/PktChatSessionUpdateParity.java).
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src):
//   record ServerboundChatSessionUpdatePacket(RemoteChatSession.Data chatSession)
//   STREAM_CODEC = Packet.codec(write, ::new)  over a PLAIN FriendlyByteBuf
//        -> body only, NO packet-id prefix, NO registry state.
//   write (ServerboundChatSessionUpdatePacket.java:18-20):
//        RemoteChatSession.Data.write(output, this.chatSession);
//   RemoteChatSession.Data.write (RemoteChatSession.java:32-35):
//        output.writeUUID(data.sessionId);          == writeLong(msb) + writeLong(lsb)
//        data.profilePublicKey.write(output);
//   ProfilePublicKey.Data.write (ProfilePublicKey.java:52-56):
//        output.writeInstant(this.expiresAt);        == writeLong(toEpochMilli())  (8 BE bytes)
//        output.writePublicKey(this.key);            == writeByteArray(key.getEncoded())
//        output.writeByteArray(this.keySignature);   == writeByteArray(sig)
//   writeUUID      (FriendlyByteBuf.java:498-501) = writeLong(msb)+writeLong(lsb)  (16 BE bytes)
//   writeInstant   (FriendlyByteBuf.java:608-610) = writeLong(epochMilli)           (8 BE bytes)
//   writeByteArray (FriendlyByteBuf.java:289-292) = VarInt(len) + raw bytes
//
//   So the body is exactly:
//       long  msb                                  (writeLong, 8 BE bytes)
//       long  lsb                                  (writeLong, 8 BE bytes)
//       long  instantMillis                        (writeLong, 8 BE bytes)
//       VarInt(keyLen)  keyBytes  (raw)            (X.509 encoded public key)
//       VarInt(sigLen)  sigBytes  (raw)            (key signature)
//
//   pkt_chat_session_update_sb_parity [--cases mcpp/build/pkt_chat_session_update.tsv]
//
// Row: ENC <msb-dec> <lsb-dec> <instantMillis-dec> <keyHex> <sigHex-or-_> <bytesN> <hexBytes>
//      sigHex token "_" means a zero-length signature.
#include "../../PacketBuffer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
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

// Decode a raw-hex byte-array field, with "_" meaning the empty array.
std::vector<uint8_t> fieldToBytes(const std::string& tok) {
    if (tok == "_") return std::vector<uint8_t>();
    return unhex(tok);
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
    std::string casesPath = "mcpp/build/pkt_chat_session_update.tsv";
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
        // ENC msb lsb instantMillis keyHex sigHex bytesN hexBytes  -> 8 columns.
        if (col.size() != 8) continue;
        if (col[0] != "ENC") continue;
        ++cases;

        int64_t msb    = (int64_t)std::stoll(col[1]);
        int64_t lsb    = (int64_t)std::stoll(col[2]);
        int64_t millis = (int64_t)std::stoll(col[3]);
        std::vector<uint8_t> keyBytes = fieldToBytes(col[4]);
        std::vector<uint8_t> sigBytes = fieldToBytes(col[5]);
        size_t expBytes = (size_t)std::stoull(col[6]);
        const std::string& expHex = col[7];

        // (1) ENCODE the body through PacketBuffer in EXACT field order:
        //     writeLong(msb), writeLong(lsb), writeLong(millis),
        //     writeByteArray(key) == writeVarInt(len)+raw, writeByteArray(sig).
        PacketBuffer enc;
        enc.writeLong(msb);
        enc.writeLong(lsb);
        enc.writeLong(millis);
        enc.writeVarInt((int32_t)keyBytes.size());
        enc.writeBytes(keyBytes);
        enc.writeVarInt((int32_t)sigBytes.size());
        enc.writeBytes(sigBytes);

        std::string got = hex(enc.data());
        if (got != expHex || enc.data().size() != expBytes) {
            ++mismatches;
            std::cerr << "ENC-MISMATCH msb=" << msb << " lsb=" << lsb
                      << " (gotBytes=" << enc.data().size() << " wantBytes=" << expBytes << ")"
                      << "\n  got  " << got << "\n  want " << expHex << "\n";
            continue;
        }

        // (2) DECODE the Java bytes back through PacketBuffer and recover every field,
        //     with nothing left over.
        std::vector<uint8_t> raw = unhex(expHex);
        PacketBuffer dec(raw);
        int64_t bMsb = dec.readLong();
        int64_t bLsb = dec.readLong();
        int64_t bMillis = dec.readLong();
        int32_t keyLen = dec.readVarInt();
        std::vector<uint8_t> bKey = dec.readBytes((size_t)keyLen);
        int32_t sigLen = dec.readVarInt();
        std::vector<uint8_t> bSig = dec.readBytes((size_t)sigLen);

        bool ok = (bMsb == msb) && (bLsb == lsb) && (bMillis == millis)
                  && (bKey == keyBytes) && (bSig == sigBytes)
                  && (dec.remaining() == 0);
        if (!ok) {
            ++mismatches;
            std::cerr << "DECODE-MISMATCH msb=" << msb << " lsb=" << lsb
                      << " keyLen(got=" << keyLen << " want=" << keyBytes.size() << ")"
                      << " sigLen(got=" << sigLen << " want=" << sigBytes.size() << ")"
                      << " remaining=" << dec.remaining() << "\n";
            continue;
        }
    }

    std::cout << "PktChatSessionUpdateParity checks=" << cases
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
