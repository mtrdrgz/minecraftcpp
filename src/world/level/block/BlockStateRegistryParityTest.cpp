// Block-state REGISTRY parity vs the real 26.1.2 Block.BLOCK_STATE_REGISTRY
// (tools/BlockStateRegistryParity.java ground truth).
//
// Compares the engine's block-state table (mcpp/src/assets/block_states.json,
// loaded exactly as the engine loads it) against the vanilla registry in ID
// order: per-id block name, plus the flags the engine trusts (is_air,
// is_opaque~canOcclude, is_solid~blocksMotion, is_fluid). Reports the first
// divergent id, per-field mismatch counts, and the state-count delta.
//
//   block_state_registry_parity [--cases mcpp/build/block_state_registry.tsv]
//                               [--states mcpp/src/assets/block_states.json]
#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    std::string casesPath = "build/block_state_registry.tsv";
    std::string statesPath = "src/assets/block_states.json";
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];
        if (std::string(argv[i]) == "--states") statesPath = argv[i + 1];
    }

    std::ifstream jf(statesPath, std::ios::binary);
    if (!jf) { std::cerr << "cannot open " << statesPath << "\n"; return 2; }
    nlohmann::json engine;
    jf >> engine;
    const auto& states = engine.at("states");

    struct Truth { std::string name; bool air, occlude, motion, fluid; };
    std::vector<Truth> truth;
    {
        std::ifstream f(casesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::istringstream ss(line);
            std::string kind; std::getline(ss, kind, '\t');
            if (kind != "STATE") continue;
            std::string id, key, props, a, o, m, fl;
            std::getline(ss, id, '\t'); std::getline(ss, key, '\t'); std::getline(ss, props, '\t');
            std::getline(ss, a, '\t'); std::getline(ss, o, '\t'); std::getline(ss, m, '\t'); std::getline(ss, fl, '\t');
            // strip "minecraft:" to match the engine's unnamespaced names
            if (key.rfind("minecraft:", 0) == 0) key = key.substr(10);
            truth.push_back({ key, a == "1", o == "1", m == "1", fl == "1" });
        }
    }

    const std::size_t engineN = states.size(), truthN = truth.size();
    std::size_t nameMis = 0, airMis = 0, occludeMis = 0, motionMis = 0, fluidMis = 0;
    long long firstDiv = -1;
    const std::size_t n = std::min(engineN, truthN);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& s = states[i];
        const Truth& t = truth[i];
        bool diverged = false;
        if (s.at("name").get<std::string>() != t.name) { ++nameMis; diverged = true; }
        if (s.at("is_air").get<bool>() != t.air) { ++airMis; diverged = true; }
        if (s.at("is_opaque").get<bool>() != t.occlude) { ++occludeMis; diverged = true; }
        if (s.at("is_solid").get<bool>() != t.motion) { ++motionMis; diverged = true; }
        if (s.at("is_fluid").get<bool>() != t.fluid) { ++fluidMis; diverged = true; }
        if (diverged && firstDiv < 0) {
            firstDiv = (long long)i;
            std::cerr << "FIRST-DIVERGENCE id=" << i
                      << " engine=" << s.at("name").get<std::string>()
                      << " truth=" << t.name << "\n";
        }
    }

    std::cout << "BlockStateRegistry engine_states=" << engineN
              << " truth_states=" << truthN
              << " missing=" << (truthN > engineN ? truthN - engineN : 0)
              << " name_mismatches=" << nameMis
              << " air_mismatches=" << airMis
              << " occlude_mismatches=" << occludeMis
              << " motion_mismatches=" << motionMis
              << " fluid_mismatches=" << fluidMis
              << " first_divergence=" << firstDiv << "\n";
    const bool pass = engineN == truthN && nameMis + airMis + occludeMis + motionMis + fluidMis == 0;
    return pass ? 0 : 1;
}
