#include "AssetPack.h"
#include "resource_ids.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#endif

namespace mc {

namespace {
    const uint8_t* s_data = nullptr;
    uint32_t       s_size = 0;

#if !defined(_WIN32)
    // On non-Windows we own the buffer (loaded from MCPP_ASSETS_BIN env or
    // assets.bin next to the executable). Keep it alive for the program lifetime.
    std::string s_ownedData;
#endif
}

bool AssetPack::init() {
#if defined(_WIN32)
    HMODULE hmod = GetModuleHandleW(nullptr);
    HRSRC   hres = FindResourceW(hmod, MAKEINTRESOURCEW(IDR_ASSETS_BIN), RT_RCDATA);
    if (!hres) return false;
    HGLOBAL hg = LoadResource(hmod, hres);
    if (!hg) return false;
    s_data = static_cast<const uint8_t*>(LockResource(hg));
    s_size = SizeofResource(hmod, hres);
    return s_data != nullptr && s_size > 0;
#else
    // Try env var first, then common paths.
    const char* envPath = std::getenv("MCPP_ASSETS_BIN");
    std::string path;
    if (envPath && *envPath) {
        path = envPath;
    } else {
        // Try relative paths that work from build/ and repo root.
        const char* candidates[] = {
            "src/assets/assets.bin",
            "../src/assets/assets.bin",
            "assets.bin",
            nullptr
        };
        for (int i = 0; candidates[i]; ++i) {
            std::ifstream probe(candidates[i], std::ios::binary);
            if (probe) { path = candidates[i]; break; }
        }
    }
    if (path.empty()) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    s_ownedData = ss.str();
    s_data = reinterpret_cast<const uint8_t*>(s_ownedData.data());
    s_size = static_cast<uint32_t>(s_ownedData.size());
    return s_data != nullptr && s_size > 0;
#endif
}

void AssetPack::shutdown() {
    s_data = nullptr;
    s_size = 0;
#if !defined(_WIN32)
    s_ownedData.clear();
    s_ownedData.shrink_to_fit();
#endif
}

std::span<const uint8_t> AssetPack::data() {
    return {s_data, s_size};
}

} // namespace mc
