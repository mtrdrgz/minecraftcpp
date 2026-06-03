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

} // namespace

BlockTags BlockTags::loadFromDirectory(const std::string& dir) {
    namespace fs = std::filesystem;
    BlockTags tags;
    if (!fs::is_directory(dir)) {
        throw std::runtime_error("block tag directory not found: " + dir);
    }
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        std::ifstream in(entry.path());
        std::stringstream ss;
        ss << in.rdbuf();
        const nlohmann::json j = nlohmann::json::parse(ss.str());
        const std::string tagName = "minecraft:" + entry.path().stem().string();
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
        tags.m_raw[tagName] = std::move(entries);
    }
    return tags;
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
