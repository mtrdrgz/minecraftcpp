#pragma once

// Minimal data-driven block-tag resolver: loads data/minecraft/tags/block/*.json
// and resolves "#tag" references recursively into flat block-id sets. This is the
// same data the game loads; used by block-survival rules (canSurvive) and the
// block_predicate_filter placement modifier.

#include <map>
#include <set>
#include <string>
#include <vector>

namespace mc::block {

class BlockTags {
public:
    // Load every tag file under `dir` (e.g. .../data/minecraft/tags/block).
    static BlockTags loadFromDirectory(const std::string& dir);

    // Is `block` ("minecraft:dirt") a member of `tag` ("minecraft:supports_vegetation")?
    bool isInTag(const std::string& block, const std::string& tag) const;

    const std::set<std::string>& members(const std::string& tag) const;
    std::vector<std::string> tagNames() const; // all loaded tag ids, sorted
    std::size_t tagCount() const { return m_raw.size(); }

private:
    // raw[tag] = list of entries (block ids or "#tag" refs)
    std::map<std::string, std::vector<std::string>> m_raw;
    // resolved[tag] = flat block-id set (memoised)
    mutable std::map<std::string, std::set<std::string>> m_resolved;

    const std::set<std::string>& resolve(const std::string& tag, std::set<std::string>& visiting) const;
};

} // namespace mc::block
