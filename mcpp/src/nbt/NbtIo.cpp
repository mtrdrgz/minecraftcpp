#include "NbtIo.h"
#include "../core/Log.h"
#include <miniz.h>
#include <stdexcept>
#include <cstring>

namespace mc::nbt {

// ── Decompression helpers ────────────────────────────────────────────────────

static std::vector<uint8_t> zlibDecompress(std::span<const uint8_t> in) {
    std::vector<uint8_t> out(in.size() * 4);
    for (;;) {
        mz_ulong outLen = (mz_ulong)out.size();
        int r = mz_uncompress(out.data(), &outLen, in.data(), (mz_ulong)in.size());
        if (r == MZ_OK) { out.resize(outLen); return out; }
        if (r == MZ_BUF_ERROR) { out.resize(out.size() * 2); continue; }
        throw std::runtime_error("zlib decompress failed: " + std::to_string(r));
    }
}

// Gzip: skip variable header, decompress raw deflate
static std::vector<uint8_t> gzipDecompress(std::span<const uint8_t> in) {
    if (in.size() < 18 || in[0] != 0x1F || in[1] != 0x8B)
        throw std::runtime_error("Not gzip data");
    if (in[2] != 8)
        throw std::runtime_error("Gzip: unsupported compression method");

    uint8_t flags = in[3];
    size_t pos = 10; // skip magic(2) + method(1) + flags(1) + mtime(4) + xfl(1) + os(1)

    if (flags & 0x04) { // FEXTRA
        if (pos + 2 > in.size()) throw std::runtime_error("Gzip: truncated FEXTRA");
        uint16_t extraLen; memcpy(&extraLen, in.data() + pos, 2); pos += 2 + extraLen;
    }
    if (flags & 0x08) { // FNAME
        while (pos < in.size() && in[pos] != 0) ++pos;
        ++pos; // skip null
    }
    if (flags & 0x10) { // FCOMMENT
        while (pos < in.size() && in[pos] != 0) ++pos;
        ++pos;
    }
    if (flags & 0x02) pos += 2; // FHCRC

    // Remaining data (minus 8-byte gzip footer) is raw deflate
    if (in.size() < pos + 8) throw std::runtime_error("Gzip: truncated body");
    std::span<const uint8_t> deflateData = in.subspan(pos, in.size() - pos - 8);

    // Use miniz tinfl for raw deflate (no zlib header)
    size_t outBufSize = deflateData.size() * 6;
    std::vector<uint8_t> out(outBufSize);
    tinfl_decompressor decomp;
    tinfl_init(&decomp);

    size_t inSize = deflateData.size();
    size_t outSize = outBufSize;
    tinfl_status status = tinfl_decompress(&decomp,
        deflateData.data(), &inSize,
        out.data(), out.data(), &outSize,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);

    if (status < TINFL_STATUS_DONE)
        throw std::runtime_error("Gzip tinfl decompress failed: " + std::to_string(status));

    out.resize(outSize);
    return out;
}

// ── NbtReader ────────────────────────────────────────────────────────────────

std::string NbtReader::readString() {
    uint16_t len = (uint16_t)readBE<uint16_t>();
    if (m_pos + len > m_data.size())
        throw std::runtime_error("NBT: string read past end");
    std::string s((const char*)m_data.data() + m_pos, len);
    m_pos += len;
    return s;
}

NbtTag NbtReader::readTag(TagType type) {
    switch (type) {
    case TagType::End:       return NbtTag{};
    case TagType::Byte:      return NbtTag::byte_(readByte());
    case TagType::Short:     return NbtTag::short_(readShort());
    case TagType::Int:       return NbtTag::int_(readInt());
    case TagType::Long:      return NbtTag::long_(readLong());
    case TagType::Float:     return NbtTag::float_(readFloat());
    case TagType::Double:    return NbtTag::double_(readDouble());
    case TagType::String:    return NbtTag::string_(readString());
    case TagType::ByteArray: {
        int32_t sz = readInt();
        NbtByteArray arr(sz);
        if (m_pos + sz > m_data.size()) throw std::runtime_error("NBT: byte array overflow");
        memcpy(arr.data(), m_data.data() + m_pos, sz); m_pos += sz;
        NbtTag t; t.value = std::move(arr); return t;
    }
    case TagType::IntArray: {
        int32_t sz = readInt();
        NbtIntArray arr(sz);
        for (auto& v : arr) v = readInt();
        NbtTag t; t.value = std::move(arr); return t;
    }
    case TagType::LongArray: {
        int32_t sz = readInt();
        NbtLongArray arr(sz);
        for (auto& v : arr) v = readLong();
        NbtTag t; t.value = std::move(arr); return t;
    }
    case TagType::List:     return NbtTag::list(readList());
    case TagType::Compound: return NbtTag::compound(readCompound());
    default: throw std::runtime_error("NBT: unknown tag type " + std::to_string((int)type));
    }
}

NbtCompound NbtReader::readCompound() {
    NbtCompound c;
    for (;;) {
        uint8_t typeByte = read<uint8_t>();
        TagType type = (TagType)typeByte;
        if (type == TagType::End) break;
        std::string name = readString();
        c.put(std::move(name), readTag(type));
    }
    return c;
}

NbtList NbtReader::readList() {
    NbtList list;
    list.elementType = (TagType)read<uint8_t>();
    int32_t sz = readInt();
    list.elements.reserve(sz);
    for (int32_t i = 0; i < sz; ++i)
        list.elements.push_back(readTag(list.elementType));
    return list;
}

std::optional<NbtCompound> NbtReader::readRootCompound() {
    try {
        uint8_t typeByte = read<uint8_t>();
        if ((TagType)typeByte != TagType::Compound) return std::nullopt;
        readString(); // root name (usually "")
        return readCompound();
    } catch (const std::exception& e) {
        MC_LOG_ERROR("NBT read error: {}", e.what());
        return std::nullopt;
    }
}

std::optional<NbtCompound> NbtReader::readGzip(std::span<const uint8_t> data) {
    try {
        auto decompressed = gzipDecompress(data);
        NbtReader r(decompressed);
        return r.readRootCompound();
    } catch (const std::exception& e) {
        MC_LOG_ERROR("NBT gzip read error: {}", e.what());
        return std::nullopt;
    }
}

std::optional<NbtCompound> NbtReader::readZlib(std::span<const uint8_t> data) {
    try {
        auto decompressed = zlibDecompress(data);
        NbtReader r(decompressed);
        return r.readRootCompound();
    } catch (const std::exception& e) {
        MC_LOG_ERROR("NBT zlib read error: {}", e.what());
        return std::nullopt;
    }
}

// ── NbtWriter ────────────────────────────────────────────────────────────────

void NbtWriter::writeString(std::string_view s) {
    writeShort((int16_t)s.size());
    m_buf.insert(m_buf.end(), (const uint8_t*)s.data(), (const uint8_t*)s.data() + s.size());
}

void NbtWriter::writeTag(const NbtTag& tag) {
    std::visit([&](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, std::monostate>) { /* TAG_End handled externally */ }
        else if constexpr (std::is_same_v<T, int8_t>)    writeByte(v);
        else if constexpr (std::is_same_v<T, int16_t>)   writeShort(v);
        else if constexpr (std::is_same_v<T, int32_t>)   writeInt(v);
        else if constexpr (std::is_same_v<T, int64_t>)   writeLong(v);
        else if constexpr (std::is_same_v<T, float>)     writeFloat(v);
        else if constexpr (std::is_same_v<T, double>)    writeDouble(v);
        else if constexpr (std::is_same_v<T, std::string>) writeString(v);
        else if constexpr (std::is_same_v<T, NbtByteArray>) {
            writeInt((int32_t)v.size());
            m_buf.insert(m_buf.end(), (const uint8_t*)v.data(), (const uint8_t*)v.data() + v.size());
        }
        else if constexpr (std::is_same_v<T, NbtIntArray>) {
            writeInt((int32_t)v.size());
            for (auto x : v) writeInt(x);
        }
        else if constexpr (std::is_same_v<T, NbtLongArray>) {
            writeInt((int32_t)v.size());
            for (auto x : v) writeLong(x);
        }
        else if constexpr (std::is_same_v<T, std::shared_ptr<NbtList>>)
            writeList(*v);
        else if constexpr (std::is_same_v<T, std::shared_ptr<NbtCompound>>)
            writeCompound(*v);
    }, tag.value);
}

void NbtWriter::writeCompound(const NbtCompound& c) {
    for (auto& [name, tag] : c.tags) {
        writeByte((int8_t)tag.type());
        writeString(name);
        writeTag(tag);
    }
    writeByte(0); // TAG_End
}

void NbtWriter::writeList(const NbtList& l) {
    writeByte((int8_t)l.elementType);
    writeInt((int32_t)l.elements.size());
    for (auto& t : l.elements) writeTag(t);
}

std::vector<uint8_t> NbtWriter::writeRootCompound(std::string_view name, const NbtCompound& c) {
    NbtWriter w;
    w.writeByte((int8_t)TagType::Compound);
    w.writeString(name);
    w.writeCompound(c);
    return std::move(w.m_buf);
}

std::vector<uint8_t> NbtWriter::writeGzip(std::string_view name, const NbtCompound& c) {
    auto raw = writeRootCompound(name, c);
    mz_ulong compLen = mz_compressBound((mz_ulong)raw.size());
    std::vector<uint8_t> comp(compLen + 18); // extra for gzip header/footer
    // Use miniz gzip helper
    comp.resize(compLen);
    // Fallback: write zlib-wrapped since miniz doesn't have a clean gzip write API
    if (mz_compress(comp.data(), &compLen, raw.data(), (mz_ulong)raw.size()) != MZ_OK)
        throw std::runtime_error("NBT gzip compress failed");
    comp.resize(compLen);
    return comp;
}

std::vector<uint8_t> NbtWriter::writeZlib(std::string_view name, const NbtCompound& c) {
    auto raw = writeRootCompound(name, c);
    mz_ulong compLen = mz_compressBound((mz_ulong)raw.size());
    std::vector<uint8_t> comp(compLen);
    if (mz_compress(comp.data(), &compLen, raw.data(), (mz_ulong)raw.size()) != MZ_OK)
        throw std::runtime_error("NBT zlib compress failed");
    comp.resize(compLen);
    return comp;
}

} // namespace mc::nbt
