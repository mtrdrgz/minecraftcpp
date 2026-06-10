// Regenerates mcpp/src/assets/block_states.json from the REAL 26.1.2 block-state
// registry ground truth (tools/BlockStateRegistryParity.java output): every state
// in vanilla id order with its canonical property string, the block's true
// default-state flag, the vanilla-derived flags (is_air / is_opaque=canOcclude /
// is_solid=blocksMotion / is_fluid), and the texture references carried over by
// block NAME from the previous file (rendering fallback until the model pipeline
// replaces the atlas path).
//
//   block_states_regen [--cases mcpp/build/block_state_registry.tsv]
//                      [--old mcpp/src/assets/block_states.json]
//                      [--out mcpp/src/assets/block_states.json]
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/block_state_registry.tsv";
    std::string oldPath = "mcpp/src/assets/block_states.json";
    std::string outPath = "mcpp/src/assets/block_states.json";
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];
        if (std::string(argv[i]) == "--old") oldPath = argv[i + 1];
        if (std::string(argv[i]) == "--out") outPath = argv[i + 1];
    }

    // texture carry-over by name from the previous table
    struct Tex { std::string top, side, bot; };
    std::map<std::string, Tex> texByName;
    {
        std::ifstream jf(oldPath, std::ios::binary);
        if (jf) {
            nlohmann::json old;
            jf >> old;
            for (const auto& s : old.at("states")) {
                const std::string n = s.at("name").get<std::string>();
                if (!texByName.count(n)) {
                    texByName[n] = { s.value("tex_top", ""), s.value("tex_side", ""), s.value("tex_bot", "") };
                }
            }
        }
    }

    struct Row { long long id; std::string name, props; bool air, occ, mot, flu; };
    std::vector<Row> rows;
    std::map<std::string, long long> defaults;   // unnamespaced name -> default state id
    long long total = -1;
    {
        std::ifstream f(casesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream ss(line);
            std::string kind; std::getline(ss, kind, '\t');
            auto strip = [](std::string s) { return s.rfind("minecraft:", 0) == 0 ? s.substr(10) : s; };
            if (kind == "STATE") {
                std::string id, key, props, a, o, m, fl;
                std::getline(ss, id, '\t'); std::getline(ss, key, '\t'); std::getline(ss, props, '\t');
                std::getline(ss, a, '\t'); std::getline(ss, o, '\t'); std::getline(ss, m, '\t'); std::getline(ss, fl, '\t');
                rows.push_back({ std::stoll(id), strip(key), props == "-" ? "" : props,
                                 a == "1", o == "1", m == "1", fl == "1" });
            } else if (kind == "DEFAULT") {
                std::string key, id;
                std::getline(ss, key, '\t'); std::getline(ss, id, '\t');
                defaults[strip(key)] = std::stoll(id);
            } else if (kind == "TOTAL") {
                std::string t; std::getline(ss, t, '\t');
                total = std::stoll(t);
            }
        }
    }
    if (total != (long long)rows.size()) {
        std::cerr << "TOTAL " << total << " != rows " << rows.size() << "\n";
        return 1;
    }

    // verify ids are dense and ordered 0..n-1
    for (size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].id != (long long)i) { std::cerr << "non-dense id at " << i << "\n"; return 1; }
    }

    std::ofstream out(outPath, std::ios::binary);
    out << "{\"total\":" << rows.size() << ",\"states\":[";
    int missingTex = 0;
    std::set<std::string> missingTexNames;
    for (size_t i = 0; i < rows.size(); ++i) {
        const Row& r = rows[i];
        const Tex* t = texByName.count(r.name) ? &texByName[r.name] : nullptr;
        if (!t && !r.air) { ++missingTex; missingTexNames.insert(r.name); }
        auto dit = defaults.find(r.name);
        const bool isDefault = dit != defaults.end() && dit->second == r.id;
        if (i) out << ',';
        out << "{\"id\":" << r.id
            << ",\"name\":\"" << r.name << '"'
            << ",\"props\":\"" << r.props << '"'
            << (isDefault ? ",\"default\":true" : "")
            << ",\"is_air\":" << (r.air ? "true" : "false")
            << ",\"is_opaque\":" << (r.occ ? "true" : "false")
            << ",\"is_solid\":" << (r.mot ? "true" : "false")
            << ",\"is_fluid\":" << (r.flu ? "true" : "false")
            << ",\"tex_top\":\"" << (t ? t->top : "") << '"'
            << ",\"tex_side\":\"" << (t ? t->side : "") << '"'
            << ",\"tex_bot\":\"" << (t ? t->bot : "") << "\"}";
    }
    out << "]}";
    std::cout << "wrote " << outPath << ": " << rows.size() << " states, "
              << defaults.size() << " defaults, " << missingTexNames.size()
              << " block names without carried textures (" << missingTex << " states)\n";
    return 0;
}
