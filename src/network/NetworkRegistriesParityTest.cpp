// Parity gate for mc::net::NetworkRegistries: proves the committed engine asset
// mcpp/src/assets/network_registries.tsv carries every network-relevant BuiltInRegistries
// entry at its EXACT vanilla VarInt id (getId order). The engine loads the COMMITTED asset;
// --cases is a FRESH dump regenerated from the real jar by tools/RegistryNetworkIdDump.java.
// For every "E <registry> <id> <name>" ground-truth row we require:
//   engine.id(registry, name)   == id           (name -> wire id)
//   engine.name(registry, id)   == name          (wire id -> name, reverse)
// plus dense 0..count-1 load order. Any drift (jar version bump, reorder, missing entry)
// fails the gate, so a packet that later encodes VarInt(getId) via this table stays 1:1.
//
//   tools/run_groundtruth.ps1 -Tool RegistryNetworkIdDump -Out mcpp/build/registry_network_ids.tsv
//   network_registries_parity --cases mcpp/build/registry_network_ids.tsv
//
// The engine asset path defaults to mcpp/src/assets/network_registries.tsv (run from repo
// root, as the other gates do); override with --asset <path>.

#include "NetworkRegistries.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    std::string casesPath, assetPath = "mcpp/src/assets/network_registries.tsv";
    for (int i = 1; i + 1 < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases") casesPath = argv[++i];
        else if (a == "--asset") assetPath = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: --cases <gt.tsv> [--asset <path>]\n"; return 2; }

    mc::net::NetworkRegistries reg;
    if (!reg.loadFromFile(assetPath)) {
        std::cerr << "cannot open engine asset " << assetPath << "\n";
        return 2;
    }
    if (!reg.loadOrderOk()) {
        std::cerr << "engine asset is not densely ordered 0..count-1\n";
        return 1;
    }

    std::ifstream f(casesPath);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long cases = 0, mism = 0; int shown = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("E\t", 0) != 0) continue;
        std::stringstream ss(line);
        std::string tag, registry, idStr, name;
        std::getline(ss, tag, '\t');
        std::getline(ss, registry, '\t');
        std::getline(ss, idStr, '\t');
        std::getline(ss, name, '\t');
        if (registry.empty() || name.empty()) continue;
        ++cases;
        int32_t gid = (int32_t)std::stol(idStr);

        auto eid = reg.id(registry, name);
        auto enm = reg.name(registry, gid);
        bool ok = eid.has_value() && *eid == gid && enm.has_value() && *enm == name;
        if (!ok) {
            ++mism;
            if (shown++ < 25) {
                std::cerr << "MISMATCH " << registry << " name=" << name << " gtId=" << gid
                          << " engineId=" << (eid ? std::to_string(*eid) : "<none>")
                          << " engineName@gtId=" << (enm ? *enm : "<none>") << "\n";
            }
        }
    }
    std::cout << "NetworkRegistries cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
