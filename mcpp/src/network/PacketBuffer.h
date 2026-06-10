#pragma once
#include <bit>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <span>
#include <stdexcept>
#include "../nbt/Tag.h"

namespace mc::net {

// Port of net.minecraft.network.FriendlyByteBuf
// Big-endian for fixed-width types, VarInt for MC-specific types
class PacketBuffer {
public:
    // --- Construction ---
    PacketBuffer() = default;
    explicit PacketBuffer(std::vector<uint8_t> data) : m_data(std::move(data)) {}
    explicit PacketBuffer(std::span<const uint8_t> data) : m_data(data.begin(), data.end()) {}

    // Writing
    void writeBool(bool v)       { writeByte((uint8_t)(v ? 1 : 0)); }
    void writeByte(uint8_t v)    { m_data.push_back(v); }
    void writeShort(int16_t v)   { writeBE<uint16_t>((uint16_t)v); }
    void writeInt(int32_t v)     { writeBE<uint32_t>((uint32_t)v); }
    void writeLong(int64_t v)    { writeBE<uint64_t>((uint64_t)v); }
    void writeFloat(float v)     { uint32_t u; std::memcpy(&u, &v, 4); writeBE<uint32_t>(u); }
    void writeDouble(double v)   { uint64_t u; std::memcpy(&u, &v, 8); writeBE<uint64_t>(u); }
    void writeVarInt(int32_t v);
    void writeVarLong(int64_t v);
    void writeString(std::string_view s, int maxLen = 32767);
    void writeBytes(std::span<const uint8_t> b) { m_data.insert(m_data.end(), b.begin(), b.end()); }
    void writeNbt(const nbt::NbtCompound& c);
    // FriendlyByteBuf.writeNbt(Tag): any root tag (type byte + unnamed payload). A
    // ComponentSerialization-encoded text component is a root StringTag; this is the form
    // play packets carrying a Component use.
    void writeNbt(const nbt::NbtTag& tag);

    // Writing a uuid (2 longs, big-endian)
    void writeUUID(uint64_t hi, uint64_t lo) { writeLong((int64_t)hi); writeLong((int64_t)lo); }

    // Reading
    bool     readBool()    { return readByte() != 0; }
    uint8_t  readByte();
    int16_t  readShort()   { return (int16_t)readBE<uint16_t>(); }
    int32_t  readInt()     { return (int32_t)readBE<uint32_t>(); }
    int64_t  readLong()    { return (int64_t)readBE<uint64_t>(); }
    float    readFloat()   { uint32_t u = readBE<uint32_t>(); float f; std::memcpy(&f, &u, 4); return f; }
    double   readDouble()  { uint64_t u = readBE<uint64_t>(); double d; std::memcpy(&d, &u, 8); return d; }
    int32_t  readVarInt();
    int64_t  readVarLong();
    std::string readString(int maxLen = 32767);
    std::vector<uint8_t> readBytes(size_t count);
    nbt::NbtCompound readNbt();
    void readUUID(uint64_t& hi, uint64_t& lo) { hi = (uint64_t)readLong(); lo = (uint64_t)readLong(); }

    // Access
    const std::vector<uint8_t>& data() const { return m_data; }
    size_t readPos()  const { return m_readPos; }
    size_t size()     const { return m_data.size(); }
    size_t remaining()const { return m_data.size() - m_readPos; }
    bool   eof()      const { return m_readPos >= m_data.size(); }
    void   skipBytes(size_t n) {
        if (m_readPos + n > m_data.size()) throw std::runtime_error("PacketBuffer skip past end");
        m_readPos += n;
    }

    // Get a sub-buffer slice for reading
    PacketBuffer slice(size_t offset, size_t len) const;
    std::span<const uint8_t> rawSpan() const { return m_data; }

private:
    std::vector<uint8_t> m_data;
    size_t               m_readPos = 0;

    template<class T>
    static T swapEndian(T v) {
        if constexpr (sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8) {
            return static_cast<T>(std::byteswap(v));
        } else {
            return v;
        }
    }

    template<class T>
    void writeBE(T v) {
        v = swapEndian(v);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        m_data.insert(m_data.end(), p, p + sizeof(T));
    }
    template<class T>
    T readBE() {
        if (m_readPos + sizeof(T) > m_data.size())
            throw std::runtime_error("PacketBuffer read past end");
        T v; std::memcpy(&v, m_data.data() + m_readPos, sizeof(T));
        m_readPos += sizeof(T);
        return swapEndian(v);
    }
};

} // namespace mc::net
