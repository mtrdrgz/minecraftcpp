#include "AssetManager.h"
#include "AssetPack.h"
#include <string>

namespace mc {

// assets.bin is the custom "MCAS" pack written by tools/asset_packer:
//   [4B] magic 0x4D434153 ("MCAS")  [4B] version  [4B] entry_count
//   per entry: [2B] path_len [N] path [4B] data_offset [4B] data_size
//   [data section: raw bytes; data_offset is relative to its start]
namespace {
    constexpr uint32_t MCAS_MAGIC = 0x4D434153u;

    uint16_t rdU16(const uint8_t* p) {
        return (uint16_t)(p[0] | (p[1] << 8));
    }
    uint32_t rdU32(const uint8_t* p) {
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
}

AssetManager& AssetManager::instance() {
    static AssetManager s;
    return s;
}

std::vector<uint8_t> AssetManager::readRaw(std::string_view path) {
    auto pack = AssetPack::data();
    const size_t total = pack.size();
    if (total < 12) return {};
    const uint8_t* base = pack.data();

    if (rdU32(base) != MCAS_MAGIC) return {};
    const uint32_t count = rdU32(base + 8);

    // Walk the whole entry table: this both locates our entry and leaves `pos`
    // at the start of the data section (data_offset is relative to there).
    size_t pos = 12;
    bool found = false;
    uint32_t hitOffset = 0, hitSize = 0;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 2 > total) return {};
        uint16_t plen = rdU16(base + pos);
        pos += 2;
        if (pos + plen + 8 > total) return {};
        std::string_view entry(reinterpret_cast<const char*>(base + pos), plen);
        pos += plen;
        uint32_t off = rdU32(base + pos); pos += 4;
        uint32_t size = rdU32(base + pos); pos += 4;
        if (!found && entry == path) {
            found = true;
            hitOffset = off;
            hitSize = size;
        }
    }
    if (!found) return {};

    const size_t dataStart = pos;
    if (dataStart + hitOffset + hitSize > total) return {};
    const uint8_t* start = base + dataStart + hitOffset;
    return std::vector<uint8_t>(start, start + hitSize);
}

std::vector<std::string> AssetManager::list(std::string_view prefix) {
    std::vector<std::string> out;
    auto pack = AssetPack::data();
    const size_t total = pack.size();
    if (total < 12) return out;
    const uint8_t* base = pack.data();

    if (rdU32(base) != MCAS_MAGIC) return out;
    const uint32_t count = rdU32(base + 8);

    size_t pos = 12;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 2 > total) return {};
        uint16_t plen = rdU16(base + pos);
        pos += 2;
        if (pos + plen + 8 > total) return {};
        std::string_view entry(reinterpret_cast<const char*>(base + pos), plen);
        pos += plen;
        pos += 8;
        if (entry.starts_with(prefix)) {
            out.emplace_back(entry);
        }
    }
    return out;
}

} // namespace mc
