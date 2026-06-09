#include "BlockTags.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace mc::block {
namespace {

std::string normalizeId(std::string id) {
    if (id.find(':') == std::string::npos) {
        id = "minecraft:" + id;
    }
    return id;
}

std::string stemFromPath(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t begin = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    const std::size_t end = dot == std::string::npos || dot < begin ? path.size() : dot;
    return path.substr(begin, end - begin);
}

std::vector<std::string> parseTagJson(const std::string& text) {
    const nlohmann::json j = nlohmann::json::parse(text);
    std::vector<std::string> entries;
    for (const auto& v : j.at("values")) {
        // entry is either "minecraft:x" / "#minecraft:tag" or {"id":..,"required":..}
        std::string id = v.is_string() ? v.get<std::string>() : v.at("id").get<std::string>();
        if (!id.empty() && id.front() == '#') {
            entries.push_back("#" + normalizeId(id.substr(1)));
        } else {
            entries.push_back(normalizeId(id));
        }
    }
    return entries;
}

} // namespace

BlockTags BlockTags::loadFromDirectory(const std::string& dir) {
    namespace fs = std::filesystem;
    if (!fs::is_directory(dir)) {
        throw std::runtime_error("block tag directory not found: " + dir);
    }

    // Parse every tags/block/*.json into raw entries (resolve() expands #tag refs
    // lazily). Required by canSurvive / RuleTest / placement predicates. (Had been
    // stubbed out to skip parsing on the Singleplayer startup path; decoration parity
    // needs the real tags, and parsing a few hundred tiny JSONs is negligible.)
    BlockTags out;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        const std::string path = entry.path().string();
        if (path.size() < 5 || path.substr(path.size() - 5) != ".json") continue;
        std::ifstream f(path, std::ios::binary);
        if (!f) continue;
        std::ostringstream ss;
        ss << f.rdbuf();
        try {
            out.m_raw.emplace(normalizeId(stemFromPath(path)), parseTagJson(ss.str()));
        } catch (const std::exception&) {
            // skip malformed tag JSON
        }
    }
    return out;
}

BlockTags BlockTags::loadFromJsonEntries(const std::vector<std::pair<std::string, std::string>>& entries) {
    (void)entries;
    return BlockTags{};
}

const std::set<std::string>& BlockTags::resolve(const std::string& tag, std::set<std::string>& visiting) const {
    auto cached = m_resolved.find(tag);
    if (cached != m_resolved.end()) {
        return cached->second;
    }
    std::set<std::string> out;
    if (visiting.count(tag) == 0) {
        visiting.insert(tag);
        auto it = m_raw.find(tag);
        if (it != m_raw.end()) {
            for (const std::string& e : it->second) {
                if (!e.empty() && e.front() == '#') {
                    const std::set<std::string>& sub = resolve(e.substr(1), visiting);
                    out.insert(sub.begin(), sub.end());
                } else {
                    out.insert(e);
                }
            }
        }
        visiting.erase(tag);
    }
    return m_resolved.emplace(tag, std::move(out)).first->second;
}

const std::set<std::string>& BlockTags::members(const std::string& tag) const {
    const std::string t = normalizeId(tag);
    auto cached = m_resolved.find(t);
    if (cached != m_resolved.end()) {
        return cached->second;
    }
    std::set<std::string> visiting;
    return resolve(t, visiting);
}

bool BlockTags::isInTag(const std::string& block, const std::string& tag) const {
    return members(tag).count(normalizeId(block)) != 0;
}

std::vector<std::string> BlockTags::tagNames() const {
    std::vector<std::string> names;
    names.reserve(m_raw.size());
    for (const auto& [name, _] : m_raw) {
        names.push_back(name);
    }
    return names; // m_raw is a std::map, already sorted by key
}

} // namespace mc::block
