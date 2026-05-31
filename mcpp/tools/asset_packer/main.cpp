//
// asset_packer — offline tool
// Usage: asset_packer <assets_dir> <src_assets_dir> <output.bin>
//
// Reads assets/indexes/<id>.json, maps hash -> original path, and writes:
//
//   [4B]  magic   0x4D434153 ("MCAS")
//   [4B]  version 1
//   [4B]  entry_count
//   per entry:
//     [2B]  path_len
//     [N B] path (UTF-8, e.g. "minecraft/textures/block/stone.png")
//     [4B]  data_offset  (from start of data section)
//     [4B]  data_size
//   [data section: raw file bytes, concatenated]
//

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

static constexpr uint32_t MAGIC   = 0x4D434153u;
static constexpr uint32_t VERSION = 1u;

struct Entry {
    std::string path;
    std::string hash; // Used for index files, empty for src_assets
    std::vector<uint8_t> data;
};

static void write_u16(std::ostream& out, uint16_t v) {
    out.write(reinterpret_cast<const char*>(&v), 2);
}
static void write_u32(std::ostream& out, uint32_t v) {
    out.write(reinterpret_cast<const char*>(&v), 4);
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: asset_packer <assets_dir> <src_assets_dir> <output.bin>\n";
        return 1;
    }

    fs::path assets_dir = argv[1];
    fs::path src_assets_dir = argv[2];
    fs::path output_path = argv[3];

    std::vector<Entry> entries;

    // 1. Find the newest .json index in assets/indexes/
    fs::path indexes_dir = assets_dir / "indexes";
    if (fs::exists(indexes_dir)) {
        fs::path index_file;
        for (auto& e : fs::directory_iterator(indexes_dir)) {
            if (e.path().extension() == ".json") {
                if (index_file.empty() || fs::last_write_time(e) > fs::last_write_time(index_file))
                    index_file = e.path();
            }
        }
        if (!index_file.empty()) {
            std::cout << "Using index: " << index_file.filename() << "\n";
            std::ifstream idx_stream(index_file);
            json idx = json::parse(idx_stream);

            for (auto& [path, info] : idx["objects"].items()) {
                std::string hash = info["hash"].get<std::string>();
                fs::path src = assets_dir / "objects" / hash.substr(0, 2) / hash;
                if (!fs::exists(src)) {
                    std::cerr << "  MISSING: " << src << "\n";
                    continue;
                }
                std::ifstream f(src, std::ios::binary | std::ios::ate);
                auto sz = f.tellg();
                f.seekg(0);
                std::vector<uint8_t> buf(static_cast<size_t>(sz));
                f.read(reinterpret_cast<char*>(buf.data()), sz);
                entries.push_back({path, hash, std::move(buf)});
            }
        }
    }

    // 2. Add files from src_assets_dir (e.g. 26.1.2/src/assets)
    if (fs::exists(src_assets_dir)) {
        std::cout << "Packing from src_assets_dir: " << src_assets_dir << "\n";
        for (auto& e : fs::recursive_directory_iterator(src_assets_dir)) {
            if (e.is_regular_file()) {
                fs::path rel_path = fs::relative(e.path(), src_assets_dir);
                std::string path_str = rel_path.generic_string();
                
                std::ifstream f(e.path(), std::ios::binary | std::ios::ate);
                auto sz = f.tellg();
                f.seekg(0);
                std::vector<uint8_t> buf(static_cast<size_t>(sz));
                f.read(reinterpret_cast<char*>(buf.data()), sz);
                
                entries.push_back({path_str, "", std::move(buf)});
            }
        }
    }

    std::cout << "Packing " << entries.size() << " total assets...\n";

    // Calculate offsets
    uint32_t data_offset = 0;
    std::vector<uint32_t> offsets(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
        offsets[i] = data_offset;
        data_offset += static_cast<uint32_t>(entries[i].data.size());
    }

    // Write output
    fs::create_directories(output_path.parent_path());
    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "Cannot open output: " << output_path << "\n";
        return 1;
    }

    write_u32(out, MAGIC);
    write_u32(out, VERSION);
    write_u32(out, static_cast<uint32_t>(entries.size()));

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& path = entries[i].path;
        write_u16(out, static_cast<uint16_t>(path.size()));
        out.write(path.data(), path.size());
        write_u32(out, offsets[i]);
        write_u32(out, static_cast<uint32_t>(entries[i].data.size()));
    }

    for (auto& entry : entries) {
        if (!entry.data.empty()) {
            out.write(reinterpret_cast<const char*>(entry.data.data()), entry.data.size());
        }
    }

    auto total = fs::file_size(output_path);
    std::cout << "Written: " << output_path << " ("
              << (total / 1024 / 1024) << " MB)\n";
    return 0;
}
