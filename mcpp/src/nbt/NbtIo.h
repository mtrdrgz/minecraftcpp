#pragma once
#include "Tag.h"
#include <bit>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace mc::nbt {

// Reads/writes Minecraft's binary NBT format (big-endian, as Java DataInputStream)
// Reference: net.minecraft.nbt.NbtIo + net.minecraft.nbt.StreamTagVisitor

// Byte-swap helper shared by the reader and writer; NBT is big-endian like Java's
// DataInput/DataOutputStream. std::byteswap is C++23.
template<class T>
inline T swapEndian(T v) {
    if constexpr (sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8) {
        return static_cast<T>(std::byteswap(v));
    } else {
        return v;
    }
}

class NbtReader {
public:
    explicit NbtReader(std::span<const uint8_t> data) : m_data(data), m_pos(0) {}

    // Read a root named compound (top-level of .nbt files, level.dat payload after gzip)
    std::optional<NbtCompound> readRootCompound();

    // Read from gzip-compressed data (level.dat etc.)
    static std::optional<NbtCompound> readGzip(std::span<const uint8_t> compressed);
    // Read from zlib-compressed data (chunk data in region files)
    static std::optional<NbtCompound> readZlib(std::span<const uint8_t> compressed);

    size_t pos() const { return m_pos; }
    bool   eof() const { return m_pos >= m_data.size(); }

private:
    NbtTag  readTag(TagType type);
    NbtTag  readNamedTag(std::string& outName);
    NbtCompound readCompound();
    NbtList     readList();

    int8_t   readByte()   { return (int8_t)read<uint8_t>(); }
    int16_t  readShort()  { return (int16_t)readBE<uint16_t>(); }
    int32_t  readInt()    { return (int32_t)readBE<uint32_t>(); }
    int64_t  readLong()   { return (int64_t)readBE<uint64_t>(); }
    float    readFloat()  { uint32_t v = readBE<uint32_t>(); float f; std::memcpy(&f, &v, 4); return f; }
    double   readDouble() { uint64_t v = readBE<uint64_t>(); double d; std::memcpy(&d, &v, 8); return d; }
    std::string readString();  // 2-byte length + UTF-8 bytes

    template<class T>
    T read() {
        if (m_pos + sizeof(T) > m_data.size())
            throw std::runtime_error("NBT read past end");
        T v;
        std::memcpy(&v, m_data.data() + m_pos, sizeof(T));
        m_pos += sizeof(T);
        return v;
    }
    template<class T>
    T readBE() {
        return swapEndian(read<T>());
    }

    std::span<const uint8_t> m_data;
    size_t                   m_pos;
};

class NbtWriter {
public:
    // Writes a root named compound to a byte vector
    static std::vector<uint8_t> writeRootCompound(std::string_view name, const NbtCompound&);
    // Writes with gzip compression
    static std::vector<uint8_t> writeGzip(std::string_view name, const NbtCompound&);
    // Writes with zlib compression
    static std::vector<uint8_t> writeZlib(std::string_view name, const NbtCompound&);

private:
    std::vector<uint8_t> m_buf;
    void writeTag(const NbtTag&);
    void writeCompound(const NbtCompound&);
    void writeList(const NbtList&);
    void writeByte(int8_t v)   { m_buf.push_back((uint8_t)v); }
    void writeShort(int16_t v) { writeBE<uint16_t>((uint16_t)v); }
    void writeInt(int32_t v)   { writeBE<uint32_t>((uint32_t)v); }
    void writeLong(int64_t v)  { writeBE<uint64_t>((uint64_t)v); }
    void writeFloat(float v)   { uint32_t u; std::memcpy(&u, &v, 4); writeBE<uint32_t>(u); }
    void writeDouble(double v) { uint64_t u; std::memcpy(&u, &v, 8); writeBE<uint64_t>(u); }
    void writeString(std::string_view s);
    template<class T>
    void writeBE(T v) {
        v = swapEndian(v);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        m_buf.insert(m_buf.end(), p, p + sizeof(T));
    }
};

} // namespace mc::nbt
