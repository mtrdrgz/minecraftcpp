#include "PacketBuffer.h"
#include "../nbt/NbtIo.h"
#include <stdexcept>
#include <cstring>

namespace mc::net {

uint8_t PacketBuffer::readByte() {
    if (m_readPos >= m_data.size()) throw std::runtime_error("PacketBuffer: read past end");
    return m_data[m_readPos++];
}

void PacketBuffer::writeVarInt(int32_t value) {
    uint32_t v = (uint32_t)value;
    while (true) {
        if ((v & ~0x7F) == 0) { writeByte((uint8_t)v); return; }
        writeByte((uint8_t)((v & 0x7F) | 0x80));
        v >>= 7;
    }
}

void PacketBuffer::writeVarLong(int64_t value) {
    uint64_t v = (uint64_t)value;
    while (true) {
        if ((v & ~0x7FULL) == 0) { writeByte((uint8_t)v); return; }
        writeByte((uint8_t)((v & 0x7F) | 0x80));
        v >>= 7;
    }
}

int32_t PacketBuffer::readVarInt() {
    // VarInt.java:25-37 — up to five bytes; OR before the bound check like Java.
    int32_t result = 0;
    int bytes = 0;
    while (true) {
        uint8_t b = readByte();
        result |= (int32_t)(b & 0x7F) << (bytes++ * 7);
        if (bytes > 5) throw std::runtime_error("VarInt too big");
        if (!(b & 0x80)) return result;
    }
}

int64_t PacketBuffer::readVarLong() {
    // VarLong.java:25-37 — up to TEN bytes are legal (every negative long encodes
    // to 10); the byte is OR-ed before the bound check, exactly like Java.
    int64_t result = 0;
    int bytes = 0;
    while (true) {
        uint8_t b = readByte();
        result |= (int64_t)(b & 0x7F) << (bytes++ * 7);
        if (bytes > 10) throw std::runtime_error("VarLong too big");
        if (!(b & 0x80)) return result;
    }
}

// UTF-16 code-unit length of a UTF-8 string, with REPLACEMENT semantics for
// malformed sequences (each bad byte -> one U+FFFD = 1 unit), matching how a
// Java String built by netty's UTF-8 CharsetDecoder (REPLACE) measures .length().
// If `replaced` is non-null, the replacement-decoded UTF-8 is written there.
static size_t utf16LengthReplacing(std::string_view s, std::string* replaced) {
    const uint8_t* b = (const uint8_t*)s.data();
    size_t n = s.size(), units = 0;
    for (size_t i = 0; i < n; ) {
        uint8_t a = b[i];
        uint32_t cp; size_t adv;
        if (a < 0x80) { cp = a; adv = 1; }
        else if ((a & 0xE0) == 0xC0 && i + 1 < n && (b[i+1] & 0xC0) == 0x80 && a >= 0xC2) {
            cp = ((a & 0x1F) << 6) | (b[i+1] & 0x3F); adv = 2;
        } else if ((a & 0xF0) == 0xE0 && i + 2 < n && (b[i+1] & 0xC0) == 0x80 && (b[i+2] & 0xC0) == 0x80) {
            cp = ((a & 0x0F) << 12) | ((b[i+1] & 0x3F) << 6) | (b[i+2] & 0x3F); adv = 3;
            if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) { cp = 0xFFFD; adv = 1; }
        } else if ((a & 0xF8) == 0xF0 && i + 3 < n && (b[i+1] & 0xC0) == 0x80 && (b[i+2] & 0xC0) == 0x80 && (b[i+3] & 0xC0) == 0x80) {
            cp = ((a & 0x07) << 18) | ((b[i+1] & 0x3F) << 12) | ((b[i+2] & 0x3F) << 6) | (b[i+3] & 0x3F); adv = 4;
            if (cp < 0x10000 || cp > 0x10FFFF) { cp = 0xFFFD; adv = 1; }
        } else { cp = 0xFFFD; adv = 1; }
        units += cp >= 0x10000 ? 2 : 1;
        if (replaced) {
            if (cp < 0x80) replaced->push_back((char)cp);
            else if (cp < 0x800) { replaced->push_back((char)(0xC0 | (cp >> 6))); replaced->push_back((char)(0x80 | (cp & 0x3F))); }
            else if (cp < 0x10000) { replaced->push_back((char)(0xE0 | (cp >> 12))); replaced->push_back((char)(0x80 | ((cp >> 6) & 0x3F))); replaced->push_back((char)(0x80 | (cp & 0x3F))); }
            else { replaced->push_back((char)(0xF0 | (cp >> 18))); replaced->push_back((char)(0x80 | ((cp >> 12) & 0x3F))); replaced->push_back((char)(0x80 | ((cp >> 6) & 0x3F))); replaced->push_back((char)(0x80 | (cp & 0x3F))); }
        }
        i += adv;
    }
    return units;
}

void PacketBuffer::writeString(std::string_view s, int maxLen) {
    // Utf8String.write: value.length() (UTF-16 units) vs maxLength, then the
    // encoded byte count vs utf8MaxBytes(maxLength) = maxLength * 3.
    size_t chars = utf16LengthReplacing(s, nullptr);
    if ((int64_t)chars > maxLen)
        throw std::runtime_error("String too big (was " + std::to_string(chars) + " characters, max " + std::to_string(maxLen) + ")");
    if ((int64_t)s.size() > (int64_t)maxLen * 3)
        throw std::runtime_error("String too big (encoded)");
    writeVarInt((int32_t)s.size());
    m_data.insert(m_data.end(), (const uint8_t*)s.data(), (const uint8_t*)s.data() + s.size());
}

std::string PacketBuffer::readString(int maxLen) {
    // Utf8String.read: buffer length vs utf8MaxBytes(maxLength)=maxLength*3, then
    // decode with REPLACEMENT, then decoded .length() (UTF-16 units) vs maxLength.
    int32_t len = readVarInt();
    if ((int64_t)len > (int64_t)maxLen * 3)
        throw std::runtime_error("The received encoded string buffer length is longer than maximum allowed (" + std::to_string(len) + ")");
    if (len < 0) throw std::runtime_error("The received encoded string buffer length is less than zero!");
    if (m_readPos + (size_t)len > m_data.size()) throw std::runtime_error("Not enough bytes in buffer");
    std::string_view raw((const char*)m_data.data() + m_readPos, (size_t)len);
    m_readPos += len;
    std::string out;
    out.reserve(raw.size());
    size_t chars = utf16LengthReplacing(raw, &out);
    if ((int64_t)chars > maxLen)
        throw std::runtime_error("The received string length is longer than maximum allowed (" + std::to_string(chars) + ")");
    return out;
}

std::vector<uint8_t> PacketBuffer::readBytes(size_t count) {
    if (m_readPos + count > m_data.size())
        throw std::runtime_error("readBytes past end");
    std::vector<uint8_t> out(m_data.begin() + m_readPos,
                              m_data.begin() + m_readPos + count);
    m_readPos += count;
    return out;
}

PacketBuffer PacketBuffer::slice(size_t offset, size_t len) const {
    if (offset + len > m_data.size()) throw std::runtime_error("slice out of bounds");
    return PacketBuffer(std::span<const uint8_t>(m_data.data() + offset, len));
}

void PacketBuffer::writeNbt(const nbt::NbtCompound& c) {
    // FriendlyByteBuf.writeNbt -> NbtIo.writeAnyTag: type byte + UNNAMED payload
    // (the network framing since 1.20.2 — NO root name string).
    auto bytes = nbt::NbtWriter::writeAnyRootCompound(c);
    writeBytes(bytes);
}

nbt::NbtCompound PacketBuffer::readNbt() {
    // FriendlyByteBuf.readNbt -> NbtIo.readAnyTag: unnamed; EndTag (0x00) = null,
    // surfaced here as an empty compound (callers needing tri-state use readNbtOpt).
    size_t start = m_readPos;
    auto span = std::span<const uint8_t>(m_data.data() + start, m_data.size() - start);
    nbt::NbtReader reader(span);
    auto result = reader.readAnyRootCompound();
    m_readPos += reader.pos();
    if (result.wasEnd) return nbt::NbtCompound{};
    if (!result.compound) throw std::runtime_error("Failed to read NBT from packet");
    return std::move(*result.compound);
}

} // namespace mc::net
