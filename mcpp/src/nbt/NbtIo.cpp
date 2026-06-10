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

// java.io.DataInputStream.readUTF 1:1: the wire format is MODIFIED UTF-8
// (U+0000 as C0 80; surrogate halves as individual 3-byte sequences; no 4-byte
// forms). Internally we store real UTF-8, so decode MUTF-8 -> UTF-16 code units
// exactly like Java, then encode UTF-16 -> UTF-8 like String.getBytes(UTF_8)
// (surrogate pairs combine to one 4-byte char; an unpaired surrogate becomes '?',
// the UTF-8 CharsetEncoder's REPLACE substitution).
std::string NbtReader::readString() {
    uint16_t len = (uint16_t)readBE<uint16_t>();
    if (m_pos + len > m_data.size())
        throw std::runtime_error("NBT: string read past end");
    const uint8_t* b = m_data.data() + m_pos;
    m_pos += len;

    std::vector<uint16_t> u16;
    u16.reserve(len);
    for (size_t i = 0; i < len; ) {
        uint8_t a = b[i];
        if (a < 0x80) {                       // 0xxxxxxx (readUTF accepts raw 0x00)
            u16.push_back(a); i += 1;
        } else if ((a & 0xE0) == 0xC0) {      // 110xxxxx 10xxxxxx
            if (i + 1 >= len || (b[i + 1] & 0xC0) != 0x80)
                throw std::runtime_error("NBT: malformed MUTF-8 (2-byte)");
            u16.push_back((uint16_t)(((a & 0x1F) << 6) | (b[i + 1] & 0x3F))); i += 2;
        } else if ((a & 0xF0) == 0xE0) {      // 1110xxxx 10xxxxxx 10xxxxxx
            if (i + 2 >= len || (b[i + 1] & 0xC0) != 0x80 || (b[i + 2] & 0xC0) != 0x80)
                throw std::runtime_error("NBT: malformed MUTF-8 (3-byte)");
            u16.push_back((uint16_t)(((a & 0x0F) << 12) | ((b[i + 1] & 0x3F) << 6) | (b[i + 2] & 0x3F))); i += 3;
        } else {
            throw std::runtime_error("NBT: malformed MUTF-8 (lead byte)");
        }
    }

    std::string out;
    out.reserve(u16.size());
    for (size_t i = 0; i < u16.size(); ++i) {
        uint32_t cp = u16[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < u16.size()
            && u16[i + 1] >= 0xDC00 && u16[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (u16[i + 1] - 0xDC00);
            ++i;
        } else if (cp >= 0xD800 && cp <= 0xDFFF) {
            out.push_back('?');               // unpaired surrogate -> REPLACE
            continue;
        }
        if (cp < 0x80) out.push_back((char)cp);
        else if (cp < 0x800) {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            out.push_back((char)(0xF0 | (cp >> 18)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }
    return out;
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

// java.io.DataOutputStream.writeUTF 1:1: internal UTF-8 -> MODIFIED UTF-8 wire
// form (U+0000 -> C0 80; supplementary code points -> surrogate pair, each half a
// 3-byte sequence; u16 length prefix counts MUTF-8 bytes; >65535 throws like
// Java's UTFDataFormatException).
void NbtWriter::writeString(std::string_view s) {
    std::vector<uint8_t> m;
    m.reserve(s.size());
    const uint8_t* b = (const uint8_t*)s.data();
    size_t n = s.size();
    auto emitUnit = [&m](uint16_t u) {            // one UTF-16 code unit -> MUTF-8
        if (u >= 0x01 && u <= 0x7F) m.push_back((uint8_t)u);
        else if (u <= 0x7FF) {                    // includes 0 -> C0 80
            m.push_back((uint8_t)(0xC0 | (u >> 6)));
            m.push_back((uint8_t)(0x80 | (u & 0x3F)));
        } else {
            m.push_back((uint8_t)(0xE0 | (u >> 12)));
            m.push_back((uint8_t)(0x80 | ((u >> 6) & 0x3F)));
            m.push_back((uint8_t)(0x80 | (u & 0x3F)));
        }
    };
    for (size_t i = 0; i < n; ) {
        uint8_t a = b[i];
        uint32_t cp;
        if (a < 0x80) { cp = a; i += 1; }
        else if ((a & 0xE0) == 0xC0 && i + 1 < n) { cp = ((a & 0x1F) << 6) | (b[i+1] & 0x3F); i += 2; }
        else if ((a & 0xF0) == 0xE0 && i + 2 < n) { cp = ((a & 0x0F) << 12) | ((b[i+1] & 0x3F) << 6) | (b[i+2] & 0x3F); i += 3; }
        else if ((a & 0xF8) == 0xF0 && i + 3 < n) { cp = ((a & 0x07) << 18) | ((b[i+1] & 0x3F) << 12) | ((b[i+2] & 0x3F) << 6) | (b[i+3] & 0x3F); i += 4; }
        else throw std::runtime_error("NBT: invalid internal UTF-8 in writeString");
        if (cp >= 0x10000) {                      // supplementary -> surrogate pair
            cp -= 0x10000;
            emitUnit((uint16_t)(0xD800 + (cp >> 10)));
            emitUnit((uint16_t)(0xDC00 + (cp & 0x3FF)));
        } else {
            emitUnit((uint16_t)cp);
        }
    }
    if (m.size() > 0xFFFF) throw std::runtime_error("NBT: string too long for writeUTF");
    writeShort((int16_t)(uint16_t)m.size());
    m_buf.insert(m_buf.end(), m.begin(), m.end());
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
    // insertion order (see NbtCompound) — matches Java's read->write byte stability
    for (auto& [name, tag] : c.entries) {
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
    // REAL gzip framing (RFC 1952), as java.util.zip.GZIPOutputStream produces:
    // 10-byte header (magic 1F 8B, method 8, flags 0, mtime 0, XFL 0, OS 0),
    // raw deflate body, CRC32 + ISIZE footer (little-endian). The previous
    // implementation emitted a ZLIB stream labelled gzip — Java's GZIPInputStream
    // would reject it.
    auto raw = writeRootCompound(name, c);

    size_t deflateCap = (size_t)mz_compressBound((mz_ulong)raw.size()) + 64;
    std::vector<uint8_t> body(deflateCap);
    tdefl_compressor comp;
    // miniz default probe count for level 6 equivalent; raw deflate (no zlib header)
    tdefl_init(&comp, nullptr, nullptr, TDEFL_DEFAULT_MAX_PROBES);
    size_t inLen = raw.size(), outLen = deflateCap;
    tdefl_status st = tdefl_compress(&comp, raw.data(), &inLen, body.data(), &outLen, TDEFL_FINISH);
    if (st != TDEFL_STATUS_DONE)
        throw std::runtime_error("NBT gzip deflate failed");
    body.resize(outLen);

    std::vector<uint8_t> out;
    out.reserve(10 + body.size() + 8);
    const uint8_t header[10] = { 0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 0 };
    out.insert(out.end(), header, header + 10);
    out.insert(out.end(), body.begin(), body.end());
    uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, raw.data(), raw.size());
    uint32_t isize = (uint32_t)raw.size();
    for (int i = 0; i < 4; ++i) out.push_back((uint8_t)(crc >> (8 * i)));
    for (int i = 0; i < 4; ++i) out.push_back((uint8_t)(isize >> (8 * i)));
    return out;
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
