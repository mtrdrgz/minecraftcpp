// Parity gate for texslots::Resolver (model texture-slot '#ref' resolution) vs the real
// net.minecraft.client.resources.model.sprite.TextureSlots.Resolver.resolve + getMaterial.
// Rebuilds the serialized layer stack, resolves, and checks each lookup (present / sprite id /
// forceTranslucent). Sprite ids are normalized Identifier.toString() on both sides.
//
//   texture_slots_parity --cases mcpp/build/texture_slots.tsv

#include "TextureSlotsResolve.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ts = mc::render::model::texslots;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: texture_slots_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, lookups = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << why << "\n";
    };

    ts::Resolver resolver;
    ts::Data curLayer;
    bool haveLayer = false;
    std::unordered_map<std::string, ts::Material> resolved;
    bool resolvedDone = false;

    auto flushLayer = [&]() {
        if (haveLayer) { resolver.addLast(curLayer); curLayer = ts::Data{}; haveLayer = false; }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];

        if (t == "CASE") {
            resolver = ts::Resolver{};
            curLayer = ts::Data{};
            haveLayer = false;
            resolvedDone = false;
        } else if (t == "LAYER") {
            flushLayer();
            haveLayer = true;
        } else if (t == "V") {
            curLayer.addTexture(p[1], ts::Material{p[2], std::stoi(p[3]) != 0});
        } else if (t == "R") {
            curLayer.addReference(p[1], p[2]);
        } else if (t == "LOOKUPS") {
            flushLayer();
            resolved = resolver.resolve();
            resolvedDone = true;
        } else if (t == "LK") {
            ++lookups;
            const std::string& name = p[1];
            bool expPresent = std::stoi(p[2]) != 0;
            const std::string& expSprite = p[3];
            bool expFt = std::stoi(p[4]) != 0;
            auto got = ts::Resolver::getMaterial(resolved, name);
            if (got.has_value() != expPresent) {
                fail("present " + name + " got=" + std::to_string(got.has_value()));
            } else if (got.has_value()) {
                if (got->sprite != expSprite || got->forceTranslucent != expFt)
                    fail("material " + name + " got=" + got->sprite + "/" + std::to_string(got->forceTranslucent));
            }
        } else if (t == "END") {
            ++cases;
            (void)resolvedDone;
        }
    }

    std::cout << "TextureSlotsResolve cases=" << cases << " lookups=" << lookups << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
