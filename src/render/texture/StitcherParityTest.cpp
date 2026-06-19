// Parity gate for mc::render::texture::Stitcher (atlas bin-packer) vs the real
// net.minecraft.client.renderer.texture.Stitcher. Rebuilds each case (register in the same
// order), stitches, and checks the atlas extent (getWidth/getHeight) + every sprite's (x,y)
// origin (compared by name, so gather order is irrelevant).
//
//   stitcher_parity --cases mcpp/build/stitcher.tsv

#include "Stitcher.h"

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace tx = mc::render::texture;

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
    if (casesPath.empty()) { std::cerr << "usage: stitcher_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, checks = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << why << "\n";
    };

    // current case state
    int maxW = 0, maxH = 0, mip = 0, aniso = 0;
    std::vector<tx::StitcherEntry> entries;
    std::map<std::string, std::pair<int, int>> expPlacements;  // "ns:path" -> (x,y)
    int expW = 0, expH = 0;

    auto runCase = [&]() {
        ++cases;
        tx::Stitcher st(maxW, maxH, mip, aniso);
        for (size_t i = 0; i < entries.size(); ++i)
            st.registerSprite(static_cast<int>(i), entries[i].width, entries[i].height);
        bool ok = st.stitch(entries);
        if (expW == -1 && expH == -1) {
            // vanilla threw StitcherException; we expect stitch()==false too.
            ++checks;
            if (ok) fail("expected no-fit but stitched");
            return;
        }
        if (!ok) { ++checks; fail("stitch failed but GT fit"); return; }
        ++checks;
        if (st.getWidth() != expW || st.getHeight() != expH)
            fail("atlas extent got=" + std::to_string(st.getWidth()) + "x" + std::to_string(st.getHeight()) +
                 " exp=" + std::to_string(expW) + "x" + std::to_string(expH));
        std::vector<tx::Placement> places;
        st.gatherSprites(places);
        std::map<std::string, std::pair<int, int>> gotPlacements;
        for (const auto& p : places) {
            const auto& e = entries[p.entryIdx];
            gotPlacements[e.ns + ":" + e.path] = {p.x, p.y};
        }
        if (gotPlacements.size() != expPlacements.size())
            fail("placement count got=" + std::to_string(gotPlacements.size()) +
                 " exp=" + std::to_string(expPlacements.size()));
        for (const auto& [name, xy] : expPlacements) {
            ++checks;
            auto it = gotPlacements.find(name);
            if (it == gotPlacements.end()) { fail("missing placement " + name); continue; }
            if (it->second != xy)
                fail("placement " + name + " got=(" + std::to_string(it->second.first) + "," +
                     std::to_string(it->second.second) + ") exp=(" + std::to_string(xy.first) + "," +
                     std::to_string(xy.second) + ")");
        }
    };

    std::string line;
    bool haveCase = false;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        if (t == "CASE") {
            maxW = std::stoi(p[1]); maxH = std::stoi(p[2]); mip = std::stoi(p[3]); aniso = std::stoi(p[4]);
            entries.clear();
            expPlacements.clear();
            expW = expH = 0;
            haveCase = true;
        } else if (t == "SP") {
            entries.push_back(tx::StitcherEntry{std::stoi(p[3]), std::stoi(p[4]), p[1], p[2]});
        } else if (t == "RES") {
            expW = std::stoi(p[1]); expH = std::stoi(p[2]);
        } else if (t == "PL") {
            expPlacements[p[1] + ":" + p[2]] = {std::stoi(p[3]), std::stoi(p[4])};
        } else if (t == "END") {
            if (haveCase) runCase();
            haveCase = false;
        }
    }

    std::cout << "Stitcher cases=" << cases << " checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
