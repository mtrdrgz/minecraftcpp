//
// asset_packer — offline tool
// Usage: asset_packer <assets_dir> <src_assets_dir> <output.bin> [data_minecraft_dir]
//
// Writes a small runtime-only MCAS pack. Deliberately excludes audio, lang files,
// models, entity textures, etc. until the C++ client actually uses them.
//

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

static constexpr uint32_t MAGIC   = 0x4D434153u;
static constexpr uint32_t VERSION = 1u;

struct Entry {
    std::string path;
    std::string hash; // Used for index files, empty for src_assets
    std::vector<uint8_t> data;
};

static bool startsWith(std::string_view s, std::string_view prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

static bool endsWith(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

static bool shouldPackIndexedAsset(const std::string& path) {
    // Currently used by runtime:
    // - block textures for the terrain atlas fallback
    // - HUD sprites loaded by Gui
    // - title panorama loaded by PanoramaRenderer
    // - sound events (.ogg) streamed by SoundManager::getOrLoadSound, plus the
    //   sounds.json registry (the launcher asset index ships sounds OUTSIDE the
    //   client.jar, so dropping them here would silently mute the runtime)
    if (startsWith(path, "minecraft/textures/block/") && endsWith(path, ".png")) return true;
    if (startsWith(path, "minecraft/textures/gui/sprites/hud/") && endsWith(path, ".png")) return true;
    if (startsWith(path, "minecraft/textures/gui/title/background/") && endsWith(path, ".png")) return true;
    if (startsWith(path, "minecraft/sounds/") && endsWith(path, ".ogg")) return true;
    if (path == "minecraft/sounds.json") return true;
    return false;
}

static bool isExcludedFile(const fs::path& file, const fs::path& excludeAbs) {
    if (file.filename() == "assets.bin") return true;
    if (excludeAbs.empty()) return false;
    std::error_code ec;
    fs::path abs = fs::absolute(file, ec);
    if (!ec && abs == excludeAbs) return true;
    ec.clear();
    if (fs::exists(file, ec) && fs::exists(excludeAbs, ec) && fs::equivalent(file, excludeAbs, ec)) return true;
    return false;
}

static void addDirectory(std::vector<Entry>& entries,
                         const fs::path& root,
                         const std::string& prefix,
                         bool recursive = true,
                         const fs::path& exclude = {}) {
    if (!fs::exists(root)) {
        return;
    }

    std::error_code ec;
    const fs::path excludeAbs = exclude.empty() ? fs::path{} : fs::absolute(exclude, ec);

    std::vector<fs::path> files;
    if (recursive) {
        for (auto& e : fs::recursive_directory_iterator(root)) {
            if (e.is_regular_file() && !isExcludedFile(e.path(), excludeAbs)) {
                files.push_back(e.path());
            }
        }
    } else {
        for (auto& e : fs::directory_iterator(root)) {
            if (e.is_regular_file() && !isExcludedFile(e.path(), excludeAbs)) {
                files.push_back(e.path());
            }
        }
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
        fs::path rel_path = fs::relative(file, root);
        std::string path_str = prefix;
        if (!path_str.empty() && path_str.back() != '/') {
            path_str.push_back('/');
        }
        path_str += rel_path.generic_string();

        std::ifstream f(file, std::ios::binary | std::ios::ate);
        auto sz = f.tellg();
        f.seekg(0);
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        f.read(reinterpret_cast<char*>(buf.data()), sz);
        entries.push_back({std::move(path_str), "", std::move(buf)});
    }
}

static void write_u16(std::ostream& out, uint16_t v) {
    out.write(reinterpret_cast<const char*>(&v), 2);
}
static void write_u32(std::ostream& out, uint32_t v) {
    out.write(reinterpret_cast<const char*>(&v), 4);
}

int main(int argc, char* argv[]) {
    if (argc != 4 && argc != 5) {
        std::cerr << "Usage: asset_packer <assets_dir> <src_assets_dir> <output.bin> [data_minecraft_dir]\n";
        return 1;
    }

    fs::path assets_dir = argv[1];
    fs::path src_assets_dir = argv[2];
    fs::path output_path = argv[3];
    fs::path data_minecraft_dir;
    if (argc == 5) {
        data_minecraft_dir = argv[4];
    }

    // Never run git-lfs from the build tool. CI and local setup scripts are
    // responsible for preparing any real assets before CMake invokes this target.
    // Running `git lfs pull` here made clean CI builds fail when the repository's
    // LFS quota was exhausted, even for compile-only validation.

    std::vector<Entry> entries;

    // 1. Read the asset index, but keep only assets the current renderer uses.
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

            int kept = 0;
            int skipped = 0;
            for (auto& [path, info] : idx["objects"].items()) {
                if (!shouldPackIndexedAsset(path)) {
                    ++skipped;
                    continue;
                }
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
                ++kept;
            }
            std::cout << "Runtime asset index filter: kept " << kept << ", skipped " << skipped << "\n";
        }
    }

    // 2. Add C++ runtime resources from src_assets_dir, but never embed assets.bin
    // into itself. That self-embedding was making each CI build grow massively.
    if (fs::exists(src_assets_dir)) {
        std::cout << "Packing from src_assets_dir: " << src_assets_dir << "\n";
        addDirectory(entries, src_assets_dir, "", true, output_path);
    }

    // 3. Add data-driven worldgen data needed by standalone terrain decoration:
    //    worldgen JSON, block + fluid tags, and the structure template NBTs
    //    (binary entries; the MCAS format stores raw bytes, so that is fine).
    if (!data_minecraft_dir.empty() && fs::exists(data_minecraft_dir)) {
        std::cout << "Packing worldgen data from: " << data_minecraft_dir << "\n";
        addDirectory(entries, data_minecraft_dir / "worldgen", "data/minecraft/worldgen");
        addDirectory(entries, data_minecraft_dir / "tags" / "block", "data/minecraft/tags/block", false);
        addDirectory(entries, data_minecraft_dir / "tags" / "fluid", "data/minecraft/tags/fluid", false);
        addDirectory(entries, data_minecraft_dir / "structure", "data/minecraft/structure");
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
    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
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
