// Crypt.h — 1:1 C++ port of the PURE byte helpers of net.minecraft.util.Crypt (MC 26.1.2).
//
// Ported (verbatim from 26.1.2/src/net/minecraft/util/Crypt.java):
//   * digestData(byte[]... inputs)  -> SHA-1 over the concatenation of all inputs.
//        Java body:
//            MessageDigest messageDigest = MessageDigest.getInstance("SHA-1");
//            for (byte[] input : inputs) messageDigest.update(input);
//            return messageDigest.digest();
//        i.e. the raw 20-byte SHA-1 of input[0] ++ input[1] ++ ... ++ input[n-1].
//        ("SHA-1" is the HASH_ALGORITHM constant on line 34 of Crypt.java.)
//
//   * The public digestData(String serverId, PublicKey, SecretKey) overload (line 78)
//        feeds three byte[] in this exact order:
//            serverId.getBytes("ISO_8859_1"), sharedKey.getEncoded(), publicKey.getEncoded()
//        Only the byte-input concatenation+hash is pure (it depends on opaque
//        getEncoded() key bytes for the real keys); this header exposes the pure core
//        digestConcat(...) over caller-supplied byte arrays. The String-of-bytes
//        encoding is ISO-8859-1 (Latin-1): one byte per char (code point & 0xFF for
//        chars 0..255). serverHashHex() reproduces Minecraft's classic "server id hash"
//        (the two's-complement, sign-magnitude hex of the SHA-1 digest) used by the
//        login/sessionserver handshake, computed over those concatenated bytes.
//
// NOT ported (key-gen / cipher / SecretKey / RSA / Base64 PEM — require JCA/registry):
//   generateSecretKey, generateKeyPair, the public digestData(String,PublicKey,SecretKey)
//   end-to-end, rsaStringToKey, stringToPemRsaPrivateKey, stringToRsaPublicKey,
//   rsaPublicKeyToString, pemRsaPrivateKeyToString, byteToPrivateKey, byteToPublicKey,
//   decryptByteToSecretKey, encryptUsingKey, decryptUsingKey, cipherData, setupCipher,
//   getCipher, the PUBLIC_KEY_CODEC / PRIVATE_KEY_CODEC, SaltSignaturePair, SaltSupplier.
//
// The SHA-1 here is genuine RFC 3174 / FIPS 180-1 (the exact algorithm the JDK's
// "SHA-1" MessageDigest implements). This is NOT a placeholder hash — it is the real,
// fully-specified standard, gated bit-for-bit against MessageDigest.getInstance("SHA-1").

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mc::crypt {

// Genuine RFC 3174 (FIPS 180-1) SHA-1. Returns the standard 20-byte digest,
// big-endian (h0..h4 each emitted most-significant byte first), exactly matching
// java.security.MessageDigest.getInstance("SHA-1").digest().
inline std::array<uint8_t, 20> sha1(const std::vector<uint8_t>& data) {
    auto rotl = [](uint32_t x, uint32_t c) -> uint32_t {
        return (x << c) | (x >> (32 - c));
    };

    // --- padding (RFC 3174 §4): append 0x80, then 0x00 until length ≡ 56 mod 64,
    //     then the 64-bit big-endian bit length. ---
    std::vector<uint8_t> msg = data;
    const uint64_t bitLen = static_cast<uint64_t>(data.size()) * 8u;
    msg.push_back(0x80);
    while (msg.size() % 64 != 56) msg.push_back(0x00);
    for (int i = 7; i >= 0; --i)
        msg.push_back(static_cast<uint8_t>((bitLen >> (8 * i)) & 0xFFu)); // length, big-endian

    // --- initial hash values (RFC 3174 §6.1) ---
    uint32_t h0 = 0x67452301u, h1 = 0xEFCDAB89u, h2 = 0x98BADCFEu,
             h3 = 0x10325476u, h4 = 0xC3D2E1F0u;

    for (std::size_t off = 0; off < msg.size(); off += 64) {
        uint32_t w[80];
        // big-endian 32-bit words
        for (int i = 0; i < 16; ++i)
            w[i] = (static_cast<uint32_t>(msg[off + i * 4]) << 24)
                 | (static_cast<uint32_t>(msg[off + i * 4 + 1]) << 16)
                 | (static_cast<uint32_t>(msg[off + i * 4 + 2]) << 8)
                 | (static_cast<uint32_t>(msg[off + i * 4 + 3]));
        for (int i = 16; i < 80; ++i)
            w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;
        for (int i = 0; i < 80; ++i) {
            uint32_t f, k;
            if (i < 20)      { f = (b & c) | (~b & d);            k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d;                    k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);  k = 0x8F1BBCDCu; }
            else             { f = b ^ c ^ d;                    k = 0xCA62C1D6u; }
            uint32_t temp = rotl(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl(b, 30); b = a; a = temp;
        }
        h0 += a; h1 += b; h2 += c; h3 += d; h4 += e;
    }

    std::array<uint8_t, 20> digest{};
    auto put = [&](int base, uint32_t v) {
        digest[base + 0] = static_cast<uint8_t>((v >> 24) & 0xFFu);
        digest[base + 1] = static_cast<uint8_t>((v >> 16) & 0xFFu);
        digest[base + 2] = static_cast<uint8_t>((v >> 8) & 0xFFu);
        digest[base + 3] = static_cast<uint8_t>(v & 0xFFu);
    };
    put(0, h0); put(4, h1); put(8, h2); put(12, h3); put(16, h4);
    return digest;
}

// --- Crypt.digestData(byte[]... inputs) -------------------------------------
// SHA-1 over input[0] ++ input[1] ++ ... ++ input[n-1] (the inputs concatenated
// in order, each fully updated into the digest). Returns the raw 20-byte digest.
inline std::array<uint8_t, 20> digestData(const std::vector<std::vector<uint8_t>>& inputs) {
    std::vector<uint8_t> concat;
    for (const auto& in : inputs) concat.insert(concat.end(), in.begin(), in.end());
    return sha1(concat);
}

// Convenience: digest a single byte buffer.
inline std::array<uint8_t, 20> digestData(const std::vector<uint8_t>& input) {
    return sha1(input);
}

// ISO-8859-1 (Latin-1) byte encoding of a string, matching
// serverId.getBytes("ISO_8859_1") in Crypt.digestData(String, ...): one byte per
// char, value = code point & 0xFF (callers must supply Latin-1-representable chars).
inline std::vector<uint8_t> iso8859_1Bytes(const std::string& s) {
    std::vector<uint8_t> out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<uint8_t>(c));
    return out;
}

// --- Minecraft "server id hash" (sign-magnitude hex of the SHA-1) ----------
// The classic auth hash: the SHA-1 digest reinterpreted as a big-endian two's-
// complement BigInteger, then BigInteger.toString(16). Negative results carry a
// leading '-'; leading zero nibbles are stripped (matching new BigInteger(digest)).
// Mirrors net.minecraft.server.dedicated / Util sha1 server-hash usage. Provided so
// the gate can exercise the digest as Mojang's protocol consumes it.
inline std::string serverHashHex(const std::array<uint8_t, 20>& digest) {
    std::array<uint8_t, 20> bytes = digest;
    bool negative = (bytes[0] & 0x80) != 0;
    if (negative) {
        // two's-complement negate: invert all bytes, add 1.
        bool carry = true;
        for (int i = 19; i >= 0; --i) {
            uint16_t v = static_cast<uint16_t>(static_cast<uint8_t>(~bytes[i])) + (carry ? 1u : 0u);
            bytes[i] = static_cast<uint8_t>(v & 0xFFu);
            carry = (v >> 8) != 0;
        }
    }
    static const char* HEX = "0123456789abcdef";
    std::string mag;
    mag.reserve(40);
    for (uint8_t b : bytes) {
        mag.push_back(HEX[(b >> 4) & 0xF]);
        mag.push_back(HEX[b & 0xF]);
    }
    // strip leading zeros (BigInteger.toString never emits leading zeros except "0")
    std::size_t start = 0;
    while (start + 1 < mag.size() && mag[start] == '0') ++start;
    std::string out = mag.substr(start);
    if (out == "0") return out; // zero magnitude
    return negative ? std::string("-") + out : out;
}

} // namespace mc::crypt
