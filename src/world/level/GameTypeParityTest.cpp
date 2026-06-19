// Parity test for mc::world::level::GameType (world/level/GameType.h) vs Java ground truth.
// Reads the TSV emitted by GameTypeParity.java and compares value-for-value, bit-exact.
//
//   game_type_parity --cases <game_type.tsv>
#include "world/level/GameType.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using mc::world::level::GameType;
namespace gt = mc::world::level;

static std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Map a Java enum name() identifier (or "null") to an optional GameType.
static std::optional<GameType> byEnumName(const std::string& s) {
    for (int32_t i = 0; i < gt::GAME_TYPE_COUNT; ++i) {
        auto v = static_cast<GameType>(i);
        if (gt::gameTypeEnumName(v) == s) return v;
    }
    return std::nullopt;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: game_type_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CONST") {
            // CONST <ordinal> <name()> <getId> <getName> <getSerializedName>
            //       <isCreative> <isSurvival> <isBlockPlacingRestricted>
            ++cases;
            int ord = std::stoi(p[1]);
            auto v = static_cast<GameType>(ord);
            bool ok = true;
            std::ostringstream why;
            if (p[2] != std::string(gt::gameTypeEnumName(v))) { ok = false; why << " name(j=" << p[2] << ",c=" << gt::gameTypeEnumName(v) << ")"; }
            if (std::stoi(p[3]) != gt::gameTypeGetId(v)) { ok = false; why << " id(j=" << p[3] << ",c=" << gt::gameTypeGetId(v) << ")"; }
            if (p[4] != std::string(gt::gameTypeGetName(v))) { ok = false; why << " getName(j=" << p[4] << ",c=" << gt::gameTypeGetName(v) << ")"; }
            if (p[5] != std::string(gt::gameTypeGetSerializedName(v))) { ok = false; why << " ser(j=" << p[5] << ",c=" << gt::gameTypeGetSerializedName(v) << ")"; }
            if ((std::stoi(p[6]) != 0) != gt::gameTypeIsCreative(v)) { ok = false; why << " isCreative(j=" << p[6] << ")"; }
            if ((std::stoi(p[7]) != 0) != gt::gameTypeIsSurvival(v)) { ok = false; why << " isSurvival(j=" << p[7] << ")"; }
            if ((std::stoi(p[8]) != 0) != gt::gameTypeIsBlockPlacingRestricted(v)) { ok = false; why << " isBlockPlacingRestricted(j=" << p[8] << ")"; }
            if (!ok) { ++mism; std::cerr << "CONST mismatch ord=" << ord << why.str() << "\n"; }

        } else if (tag == "COUNT") {
            // COUNT <values().length>
            ++cases;
            int count = std::stoi(p[1]);
            if (count != gt::GAME_TYPE_COUNT) {
                ++mism;
                std::cerr << "COUNT mismatch java=" << count << " cpp=" << gt::GAME_TYPE_COUNT << "\n";
            }

        } else if (tag == "BYID") {
            // BYID <id> <byId(id).name()>
            ++cases;
            int id = std::stoi(p[1]);
            std::string got = std::string(gt::gameTypeEnumName(gt::gameTypeById(id)));
            if (got != p[2]) {
                ++mism;
                std::cerr << "BYID mismatch id=" << id << " java=" << p[2] << " cpp=" << got << "\n";
            }

        } else if (tag == "BYNAME") {
            // BYNAME <input> <byName(input).name()>  (default SURVIVAL)
            ++cases;
            std::string got = std::string(gt::gameTypeEnumName(gt::gameTypeByName(p[1])));
            if (got != p[2]) {
                ++mism;
                std::cerr << "BYNAME mismatch input='" << p[1] << "' java=" << p[2] << " cpp=" << got << "\n";
            }

        } else if (tag == "BYNAME2") {
            // BYNAME2 <input> <defaultOrdinalOr-1> <result.name()|null>
            ++cases;
            const std::string& input = p[1];
            int defOrd = std::stoi(p[2]);
            std::optional<GameType> got;
            if (defOrd == -1) {
                // null default -> CODEC.byName(input) (may be null)
                got = gt::gameTypeCodecByName(input);
            } else {
                got = gt::gameTypeByName(input, static_cast<GameType>(defOrd));
            }
            std::string gotStr = got ? std::string(gt::gameTypeEnumName(*got)) : "null";
            if (gotStr != p[3]) {
                ++mism;
                std::cerr << "BYNAME2 mismatch input='" << input << "' def=" << defOrd
                          << " java=" << p[3] << " cpp=" << gotStr << "\n";
            }

        } else if (tag == "NULID") {
            // NULID <ordinalOr-1> <getNullableId>
            ++cases;
            int ord = std::stoi(p[1]);
            std::optional<GameType> v = (ord == -1) ? std::nullopt
                                                    : std::optional<GameType>(static_cast<GameType>(ord));
            int got = gt::gameTypeGetNullableId(v);
            int want = std::stoi(p[2]);
            if (got != want) {
                ++mism;
                std::cerr << "NULID mismatch ord=" << ord << " java=" << want << " cpp=" << got << "\n";
            }

        } else if (tag == "BYNULID") {
            // BYNULID <id> <byNullableId(id).name()|null>
            ++cases;
            int id = std::stoi(p[1]);
            std::optional<GameType> r = gt::gameTypeByNullableId(id);
            std::string got = r ? std::string(gt::gameTypeEnumName(*r)) : "null";
            if (got != p[2]) {
                ++mism;
                std::cerr << "BYNULID mismatch id=" << id << " java=" << p[2] << " cpp=" << got << "\n";
            }

        } else if (tag == "VALIDID") {
            // VALIDID <id> <isValidId>
            ++cases;
            int id = std::stoi(p[1]);
            bool got = gt::gameTypeIsValidId(id);
            bool want = std::stoi(p[2]) != 0;
            if (got != want) {
                ++mism;
                std::cerr << "VALIDID mismatch id=" << id << " java=" << want << " cpp=" << got << "\n";
            }
        } else if (tag == "ABIL") {
            // ABIL <gtId> <startFlying> <mayfly> <instabuild> <invulnerable> <flying> <mayBuild>
            ++cases;
            GameType v = gt::gameTypeById(std::stoi(p[1]));
            gt::Abilities a;
            a.flying = std::stoi(p[2]) != 0;
            gt::gameTypeUpdatePlayerAbilities(v, a);
            bool ok = (a.mayfly == (std::stoi(p[3]) != 0)) && (a.instabuild == (std::stoi(p[4]) != 0)) &&
                      (a.invulnerable == (std::stoi(p[5]) != 0)) && (a.flying == (std::stoi(p[6]) != 0)) &&
                      (a.mayBuild == (std::stoi(p[7]) != 0));
            if (!ok) {
                ++mism;
                std::cerr << "ABIL mismatch gt=" << p[1] << " sf=" << p[2] << " got mayfly=" << a.mayfly
                          << " insta=" << a.instabuild << " invuln=" << a.invulnerable << " fly=" << a.flying
                          << " build=" << a.mayBuild << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "GameType cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
