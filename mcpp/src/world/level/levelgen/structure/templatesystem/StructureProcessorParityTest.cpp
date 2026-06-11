// Parity gate for net.minecraft...templatesystem RuleTest.test(BlockState, RandomSource) —
// the input-predicate logic of RuleProcessor (the dominant StructureProcessor used by every
// jigsaw structure). Ground truth: tools/StructureProcessorParity.java drives the REAL
// RuleTest.test over the vanilla PROCESSOR_LIST registry. This port reproduces test() from the
// emitted rule config and compares over the same battery x seeds.
//
//   always_true        -> true
//   block_match        -> state.is(block)                = blockName(state) == block
//   blockstate_match   -> state == blockState            = plain state-id equality
//   tag_match          -> state.is(tag)                  = BlockTags.isInTag(block, tag)
//   random_block_match -> state.is(block) && random.nextFloat() < probability
//                         (random = RandomSource.create(seed); nextFloat ONLY drawn if block matches)
//
//   structure_processor_parity [--cases mcpp/build/structure_processor.tsv]
//                              [--states mcpp/src/assets/block_states.json]
//                              [--tags 26.1.2/data/minecraft/tags/block]

#include "../../RandomSource.h"
#include "../../../block/BlockTags.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> splitTab(const std::string& s) {
    std::vector<std::string> o; std::string c; std::istringstream ss(s);
    while (std::getline(ss, c, '\t')) o.push_back(c);
    if (!o.empty() && !o.back().empty() && o.back().back() == '\r') o.back().pop_back();
    return o;
}
std::string stripNs(const std::string& id) {
    auto c = id.find(':');
    return c == std::string::npos ? id : id.substr(c + 1);
}
struct Rule {
    enum Kind { ALWAYS_TRUE, BLOCK_MATCH, BLOCKSTATE_MATCH, TAG_MATCH, RANDOM_BLOCK_MATCH } kind;
    std::string blockShort;   // block_match / random_block_match (namespace-stripped)
    long stateId = -1;        // blockstate_match
    std::string tag;          // tag_match (full id)
    float prob = 0.f;         // random_block_match
};
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/structure_processor.tsv";
    std::string statesPath = "mcpp/src/assets/block_states.json";
    std::string tagsDir = "26.1.2/data/minecraft/tags/block";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--states" && i + 1 < argc) statesPath = argv[++i];
        else if (a == "--tags" && i + 1 < argc) tagsDir = argv[++i];
    }

    // state id -> short block name (block_states.json).
    std::vector<std::string> blockName;
    {
        std::ifstream f(statesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << statesPath << "\n"; return 2; }
        nlohmann::json j; f >> j;
        auto arr = j.at("states");
        blockName.resize(arr.size());
        for (auto& s : arr) blockName[s.at("id").get<std::size_t>()] = s.at("name").get<std::string>();
    }
    mc::block::BlockTags tags = mc::block::BlockTags::loadFromDirectory(tagsDir);

    auto ruleKey = [](const std::string& list, const std::string& p, const std::string& r) {
        return list + "\x01" + p + "\x01" + r;
    };
    std::map<std::string, Rule> rules;
    {
        std::ifstream f(casesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("RULE\t", 0) != 0) continue;
            auto c = splitTab(line);   // RULE list proc rule type [args]
            if (c.size() < 5) continue;
            Rule rl{};
            const std::string& t = c[4];
            if (t == "always_true") rl.kind = Rule::ALWAYS_TRUE;
            else if (t == "block_match") { rl.kind = Rule::BLOCK_MATCH; rl.blockShort = stripNs(c[5]); }
            else if (t == "blockstate_match") { rl.kind = Rule::BLOCKSTATE_MATCH; rl.stateId = std::stol(c[5]); }
            else if (t == "tag_match") { rl.kind = Rule::TAG_MATCH; rl.tag = c[5]; }
            else if (t == "random_block_match") { rl.kind = Rule::RANDOM_BLOCK_MATCH; rl.blockShort = stripNs(c[5]); rl.prob = std::stof(c[6]); }
            else continue;
            rules[ruleKey(c[1], c[2], c[3])] = rl;
        }
    }

    auto testRule = [&](const Rule& r, long stateId, int64_t seed) -> bool {
        switch (r.kind) {
            case Rule::ALWAYS_TRUE:      return true;
            case Rule::BLOCK_MATCH:      return (std::size_t)stateId < blockName.size() && blockName[stateId] == r.blockShort;
            case Rule::BLOCKSTATE_MATCH: return stateId == r.stateId;
            case Rule::TAG_MATCH:        return (std::size_t)stateId < blockName.size() &&
                                                tags.isInTag("minecraft:" + blockName[stateId], r.tag);
            case Rule::RANDOM_BLOCK_MATCH: {
                bool m = (std::size_t)stateId < blockName.size() && blockName[stateId] == r.blockShort;
                if (!m) return false;                          // short-circuit: no nextFloat draw
                auto rng = mc::levelgen::RandomSource::create(seed);
                return rng->nextFloat() < r.prob;
            }
        }
        return false;
    };

    // tag_match is EXCLUDED from the GT comparison: the GT drives the REAL RuleTest.test, but
    // VanillaRegistries.createLookup() does NOT bind datapack block-tags, so its state.is(tag)
    // returns false for everything (provably the broken side — e.g. it claims oak_door is NOT in
    // #doors). The C++ tag_match (= BlockTags.isInTag over the REAL tag files) is correct and its
    // membership is independently certified by block_tags_parity; tag_match's RuleTest logic is the
    // trivial composition isInTag(blockName(state), tag). So it is certified-by-composition.
    long checks = 0, mismatches = 0, tagSkipped = 0;
    int shown = 0;
    {
        std::ifstream f(casesPath, std::ios::binary);
        std::string line;
        while (std::getline(f, line)) {
            if (line.rfind("TEST\t", 0) != 0) continue;
            auto c = splitTab(line);   // TEST list proc rule stateId seed result
            if (c.size() < 7) continue;
            auto it = rules.find(ruleKey(c[1], c[2], c[3]));
            if (it == rules.end()) continue;   // rule type not covered (OTHER)
            if (it->second.kind == Rule::TAG_MATCH) { ++tagSkipped; continue; }
            long stateId = std::stol(c[4]);
            int64_t seed = std::stoll(c[5]);
            int want = std::stoi(c[6]);
            bool got = testRule(it->second, stateId, seed);
            ++checks;
            if ((int)got != want) {
                ++mismatches;
                if (shown++ < 16)
                    std::cerr << "mismatch list=" << c[1] << " r=" << c[3] << " state=" << stateId
                              << " seed=" << seed << " got=" << got << " want=" << want << "\n";
            }
        }
    }

    std::cout << "StructureProcessor RuleTest checks=" << checks << " mismatches=" << mismatches
              << " (rules=" << rules.size() << "; tag_match cases excluded from GT cmp="
              << tagSkipped << " — certified-by-composition via block_tags_parity)\n";
    return mismatches > 0 ? 1 : 0;
}
