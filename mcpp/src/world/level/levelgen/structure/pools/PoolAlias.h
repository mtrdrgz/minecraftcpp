#pragma once

// 1:1 C++ port of the jigsaw pool-alias system
//   net.minecraft.world.level.levelgen.structure.pools.alias.{PoolAliasLookup,
//     PoolAliasBinding, DirectPoolAlias, RandomPoolAlias, RandomGroupPoolAlias}
//
// PoolAliasLookup.create(bindings, pos, seed) (PoolAliasLookup.java:19-31):
//   if bindings empty -> identity. Else a POSITIONAL random is forked from
//   RandomSource.create(seed).forkPositional().at(pos) (NOTE: forkPositional()
//   consumes one nextLong() from the legacy seed source, then at(x,y,z) reseeds
//   by Mth.getSeed(x,y,z) ^ seed) and each binding is resolved into an
//   alias->target map IN LIST ORDER. This positional random is SEPARATE from the
//   worldgen random — it does NOT perturb the addPieces RNG sequence.
//
// Binding resolution (forEachResolved):
//   - direct        (DirectPoolAlias.java)      : put(alias, target). No RNG.
//   - random        (RandomPoolAlias.java)      : put(alias, targets.getRandomOrThrow(random)).
//   - random_group  (RandomGroupPoolAlias.java) : pick one group via
//       groups.getRandomOrThrow(random), then resolve EACH binding in that group.
//
// WeightedList.getRandomOrThrow (WeightedList.java:75-81): selection =
//   random.nextInt(totalWeight); the selector maps selection -> item. A cumulative
//   walk reproduces both the single nextInt draw and the selected item (Flat and
//   Compact selectors yield identical results for a given selection index).
//
// lookup application: poolAliasLookup.lookup(key) returns the mapped pool id, or
// the key itself when unmapped (JigsawPlacement applies it at the center pool and
// at every source/child jigsaw target pool — :71, :323, :374).

#include "../../RandomSource.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mc::levelgen::structure::pools::alias {

inline std::string normId(const std::string& s) {
    return s.find(':') == std::string::npos ? "minecraft:" + s : s;
}

struct Binding {
    enum Kind { DIRECT, RANDOM, RANDOM_GROUP };
    Kind kind = DIRECT;
    std::string alias;                                       // DIRECT, RANDOM
    std::string target;                                      // DIRECT
    std::vector<std::pair<std::string, int>> randomTargets;  // RANDOM: (target, weight)
    std::vector<std::pair<std::vector<Binding>, int>> groups; // RANDOM_GROUP: (bindings, weight)
};

inline Binding parseBinding(const nlohmann::json& j) {
    Binding b;
    std::string t = j.at("type").get<std::string>();
    if (t.rfind("minecraft:", 0) == 0) t = t.substr(10);
    if (t == "direct") {
        b.kind = Binding::DIRECT;
        b.alias = normId(j.at("alias").get<std::string>());
        b.target = normId(j.at("target").get<std::string>());
    } else if (t == "random") {
        b.kind = Binding::RANDOM;
        b.alias = normId(j.at("alias").get<std::string>());
        for (const auto& e : j.at("targets"))
            b.randomTargets.emplace_back(normId(e.at("data").get<std::string>()), e.at("weight").get<int>());
    } else if (t == "random_group") {
        b.kind = Binding::RANDOM_GROUP;
        for (const auto& g : j.at("groups")) {
            std::vector<Binding> sub;
            for (const auto& sb : g.at("data")) sub.push_back(parseBinding(sb));
            b.groups.emplace_back(std::move(sub), g.at("weight").get<int>());
        }
    } else {
        throw std::runtime_error("unported pool alias binding type: " + t);
    }
    return b;
}

inline std::vector<Binding> parseBindings(const nlohmann::json& arr) {
    std::vector<Binding> v;
    for (const auto& e : arr) v.push_back(parseBinding(e));
    return v;
}

// WeightedList.getRandomOrThrow: one nextInt(totalWeight) draw + cumulative walk.
template <class T>
const T& weightedPick(const std::vector<std::pair<T, int>>& items, mc::levelgen::RandomSource& random) {
    int total = 0;
    for (const auto& it : items) total += it.second;          // WeightedRandom.getTotalWeight
    int sel = random.nextInt(total);
    for (const auto& it : items) {
        if (sel < it.second) return it.first;
        sel -= it.second;
    }
    return items.back().first;                                // unreachable for total > 0
}

inline void forEachResolved(const Binding& b, mc::levelgen::RandomSource& random,
                            std::map<std::string, std::string>& out) {
    switch (b.kind) {
        case Binding::DIRECT:
            out[b.alias] = b.target;
            break;
        case Binding::RANDOM:
            out[b.alias] = weightedPick(b.randomTargets, random);
            break;
        case Binding::RANDOM_GROUP: {
            const std::vector<Binding>& group = weightedPick(b.groups, random);
            for (const Binding& sub : group) forEachResolved(sub, random, out);
            break;
        }
    }
}

inline std::map<std::string, std::string> createLookup(
        const std::vector<Binding>& bindings, int px, int py, int pz, int64_t seed) {
    std::map<std::string, std::string> out;
    if (bindings.empty()) return out;
    auto random = mc::levelgen::RandomSource::create(seed)->forkPositional()->at(px, py, pz);
    for (const Binding& b : bindings) forEachResolved(b, *random, out);
    return out;
}

inline std::string applyAlias(const std::map<std::string, std::string>& m, const std::string& key) {
    auto it = m.find(key);
    return it == m.end() ? key : it->second;
}

}  // namespace mc::levelgen::structure::pools::alias
