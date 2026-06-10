// Parity gate for resolvedmodel:: parent-chain resolution vs the real ResolvedModel.findTop*
// (findTopAmbientOcclusion / findTopGuiLight / findTopGeometry / findTopTextureSlots). Rebuilds
// the serialized chain, runs the walks, and checks ao/guiLight/geometry-id + each slot lookup.
//
//   resolved_model_parity --cases mcpp/build/resolved_model.tsv

#include "ResolvedModelResolve.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace rm = mc::render::model::resolvedmodel;
namespace ts = mc::render::model::texslots;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
struct ExpLk { std::string name; bool present; std::string sprite; bool ft; };
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: resolved_model_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, checks = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << why << "\n";
    };

    std::vector<rm::ModelNode> chain;
    rm::ModelNode cur;
    bool inNode = false;
    int expAo = 0, expGl = 0, expGeom = -1;
    std::vector<ExpLk> lks;

    auto runCase = [&]() {
        ++cases;
        bool ao = rm::findTopAmbientOcclusion(chain);
        rm::GuiLight gl = rm::findTopGuiLight(chain);
        int geom = rm::findTopGeometry(chain);
        auto slots = rm::findTopTextureSlots(chain);
        ++checks;
        if ((ao ? 1 : 0) != expAo) fail("ao got=" + std::to_string(ao));
        if (static_cast<int>(gl) != expGl) fail("guiLight got=" + std::to_string(static_cast<int>(gl)));
        if (geom != expGeom) fail("geometry got=" + std::to_string(geom));
        for (const ExpLk& e : lks) {
            ++checks;
            auto got = ts::Resolver::getMaterial(slots, e.name);
            if (got.has_value() != e.present) fail("lk present " + e.name);
            else if (got.has_value() && (got->sprite != e.sprite || got->forceTranslucent != e.ft))
                fail("lk material " + e.name + " got=" + got->sprite);
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        if (t == "CASE") {
            chain.clear();
            lks.clear();
            inNode = false;
        } else if (t == "NODE") {
            cur = rm::ModelNode{};
            inNode = true;
            if (std::stoi(p[1]) != 0) cur.ambientOcclusion = (std::stoi(p[2]) != 0);
            if (std::stoi(p[3]) != 0) cur.guiLight = static_cast<rm::GuiLight>(std::stoi(p[4]));
            if (std::stoi(p[5]) != 0) cur.geometryId = std::stoi(p[6]);
        } else if (t == "V" && inNode) {
            cur.textureSlots.addTexture(p[1], ts::Material{p[2], std::stoi(p[3]) != 0});
        } else if (t == "R" && inNode) {
            cur.textureSlots.addReference(p[1], p[2]);
        } else if (t == "ENDNODE") {
            chain.push_back(cur);
            inNode = false;
        } else if (t == "RESULT") {
            expAo = std::stoi(p[1]);
            expGl = std::stoi(p[2]);
            expGeom = std::stoi(p[3]);
        } else if (t == "LK") {
            lks.push_back(ExpLk{p[1], std::stoi(p[2]) != 0, p[3], std::stoi(p[4]) != 0});
        } else if (t == "END") {
            runCase();
        }
    }

    std::cout << "ResolvedModelResolve cases=" << cases << " checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
