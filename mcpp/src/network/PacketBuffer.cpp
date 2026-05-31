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
    int32_t result = 0;
    int shift = 0;
    while (true) {
        uint8_t b = readByte();
        result |= (int32_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return result;
        shift += 7;
        if (shift >= 35) throw std::runtime_error("VarInt too long");
    }
}

int64_t PacketBuffer::readVarLong() {
    int64_t result = 0;
    int shift = 0;
    while (true) {
        uint8_t b = readByte();
        result |= (int64_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) return result;
        shift += 7;
        if (shift >= 63) throw std::runtime_error("VarLong too long");
    }
}

void PacketBuffer::writeString(std::string_view s, int maxLen) {
    if ((int)s.size() > maxLen) throw std::runtime_error("String too long");
    writeVarInt((int32_t)s.size());
    m_data.insert(m_data.end(), (const uint8_t*)s.data(), (const uint8_t*)s.data() + s.size());
}

std::string PacketBuffer::readString(int maxLen) {
    int32_t len = readVarInt();
    if (len > maxLen * 4) throw std::runtime_error("String too long: " + std::to_string(len));
    if (m_readPos + len > m_data.size()) throw std::runtime_error("String read past end");
    std::string s((const char*)m_data.data() + m_readPos, len);
    m_readPos += len;
    return s;
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
    auto bytes = nbt::NbtWriter::writeRootCompound("", c);
    writeBytes(bytes);
}

nbt::NbtCompound PacketBuffer::readNbt() {
    size_t start = m_readPos;
    auto span = std::span<const uint8_t>(m_data.data() + start, m_data.size() - start);
    nbt::NbtReader reader(span);
    auto result = reader.readRootCompound();
    m_readPos += reader.pos();
    if (!result) throw std::runtime_error("Failed to read NBT from packet");
    return std::move(*result);
}

} // namespace mc::net
