#include "AssetPack.h"
#include "resource_ids.h"
#include <windows.h>

namespace mc {

namespace {
    const uint8_t* s_data = nullptr;
    uint32_t       s_size = 0;
}

bool AssetPack::init() {
    HMODULE hmod = GetModuleHandleW(nullptr);
    HRSRC   hres = FindResourceW(hmod, MAKEINTRESOURCEW(IDR_ASSETS_BIN), RT_RCDATA);
    if (!hres) return false;
    HGLOBAL hg = LoadResource(hmod, hres);
    if (!hg) return false;
    s_data = static_cast<const uint8_t*>(LockResource(hg));
    s_size = SizeofResource(hmod, hres);
    return s_data != nullptr && s_size > 0;
}

void AssetPack::shutdown() {
    s_data = nullptr;
    s_size = 0;
}

std::span<const uint8_t> AssetPack::data() {
    return {s_data, s_size};
}

} // namespace mc
