// Parity gate for net.minecraft...Block.updateFromNeighbourShapes — the per-block updateShape
// connection recompute (runs at the placeInWorld knownShape=false tail and on every neighbour
// update). Ground truth: tools/BlockUpdateShapeParity.java drives the REAL
// updateFromNeighbourShapes over real states x controlled 3x3x3 neighbourhoods and emits
// (centreStateId, 26 neighbour ids, outStateId). We replay the SAME neighbourhood and compare.
//
// Certified FAMILY-BY-FAMILY (updateShape declaring class). Unported families are counted as
// `todo` (a printed worklist), NEVER silently passed (RULE #0): a family is only compared once
// its updateShape is ported here.
//
//   block_update_shape_parity [--cases mcpp/build/block_update_shape.tsv]
//                             [--states mcpp/src/assets/block_states.json]

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::vector<std::string> splitTab(const std::string& s) {
    std::vector<std::string> o; std::string c; std::istringstream ss(s);
    while (std::getline(ss, c, '\t')) o.push_back(c);
    if (!o.empty() && !o.back().empty() && o.back().back() == '\r') o.back().pop_back();
    return o;
}

// ── state table (block_states.json) + (name,props)->id reverse index for setValue. ──
std::vector<std::string> g_name, g_props;
std::unordered_map<std::string, int> g_index;  // "name\x1fprops" -> id

std::string getProp(const std::string& props, const std::string& key) {
    std::istringstream ss(props); std::string p;
    while (std::getline(ss, p, ',')) {
        auto eq = p.find('=');
        if (eq != std::string::npos && p.substr(0, eq) == key) return p.substr(eq + 1);
    }
    return "";
}
// state.setValue(key, val): rebuild the props string (state-definition order preserved) and look
// up the resulting state id. Returns -1 if the (name,props') combination is not a real state.
int setProp(int stateId, const std::string& key, const std::string& val) {
    const std::string& props = g_props[stateId];
    std::string out; std::istringstream ss(props); std::string p; bool found = false;
    while (std::getline(ss, p, ',')) {
        auto eq = p.find('=');
        if (!out.empty()) out += ',';
        if (eq != std::string::npos && p.substr(0, eq) == key) { out += key + "=" + val; found = true; }
        else out += p;
    }
    if (!found) return -1;
    auto it = g_index.find(g_name[stateId] + "\x1f" + out);
    return it == g_index.end() ? -1 : it->second;
}

// ── directions (mc Direction order: DOWN,UP,NORTH,SOUTH,WEST,EAST). ──
enum Dir { DOWN = 0, UP, NORTH, SOUTH, WEST, EAST };
const int DX[6] = {0, 0, 0, 0, -1, 1};
const int DY[6] = {-1, 1, 0, 0, 0, 0};
const int DZ[6] = {0, 0, -1, 1, 0, 0};
const int UPDATE_SHAPE_ORDER[6] = {WEST, EAST, NORTH, SOUTH, DOWN, UP};
int opposite(int d) { static const int O[6] = {UP, DOWN, SOUTH, NORTH, EAST, WEST}; return O[d]; }
bool isHorizontal(int d) { return d >= NORTH; }
// getCounterClockWise (around Y): NORTH->WEST->SOUTH->EAST->NORTH.
int counterClockWise(int d) {
    switch (d) { case NORTH: return WEST; case WEST: return SOUTH; case SOUTH: return EAST;
                 case EAST: return NORTH; default: return d; }
}
int dirFromName(const std::string& s) {
    if (s == "down") return DOWN; if (s == "up") return UP; if (s == "north") return NORTH;
    if (s == "south") return SOUTH; if (s == "west") return WEST; return EAST;
}

// neighbourhood: offset (dx,dy,dz) in [-1,1]^3 -> stateId; air (0) outside.
struct Level {
    const std::array<int, 27>* cells;  // indexed (dx+1)*9+(dy+1)*3+(dz+1)
    int at(int dx, int dy, int dz) const {
        if (dx < -1 || dx > 1 || dy < -1 || dy > 1 || dz < -1 || dz > 1) return 0;  // air
        return (*cells)[(dx + 1) * 9 + (dy + 1) * 3 + (dz + 1)];
    }
    int rel(int d) const { return at(DX[d], DY[d], DZ[d]); }  // getBlockState(pos.relative(d))
};

bool isStairsName(const std::string& n) {
    return n.size() >= 6 && n.compare(n.size() - 6, 6, "stairs") == 0;
}

// StairBlock.getStairsShape :129-156 — recompute SHAPE from the stair in front/behind.
std::string getStairsShape(int stateId, const Level& level) {
    int facing = dirFromName(getProp(g_props[stateId], "facing"));
    std::string half = getProp(g_props[stateId], "half");
    auto canTakeShape = [&](int nDir) {
        int nb = level.rel(nDir);
        return !isStairsName(g_name[nb]) || dirFromName(getProp(g_props[nb], "facing")) != facing
            || getProp(g_props[nb], "half") != half;
    };
    int behind = level.rel(facing);
    if (isStairsName(g_name[behind]) && getProp(g_props[behind], "half") == half) {
        int bf = dirFromName(getProp(g_props[behind], "facing"));
        bool bfAxisV = (bf == DOWN || bf == UP), fAxisV = false;  // facing is horizontal
        auto axisOf = [](int d){ return d <= UP ? 1 : (d <= SOUTH ? 2 : 0); };
        if (axisOf(bf) != axisOf(facing) && canTakeShape(opposite(bf)))
            return bf == counterClockWise(facing) ? "outer_left" : "outer_right";
    }
    int front = level.rel(opposite(facing));
    if (isStairsName(g_name[front]) && getProp(g_props[front], "half") == half) {
        int ff = dirFromName(getProp(g_props[front], "facing"));
        auto axisOf = [](int d){ return d <= UP ? 1 : (d <= SOUTH ? 2 : 0); };
        if (axisOf(ff) != axisOf(facing) && canTakeShape(ff))
            return ff == counterClockWise(facing) ? "inner_left" : "inner_right";
    }
    return "straight";
}

// updateShape dispatch (keyed by the updateShape declaring class). Returns the new state id, or
// -2 if the family is not yet ported (-> todo). A ported family that leaves the state unchanged
// returns stateId.
const std::set<std::string> PORTED = { "StairBlock" };

int updateShapeOne(const std::string& fam, int stateId, int dir, int /*neighbourId*/, const Level& level) {
    if (fam == "StairBlock") {
        // StairBlock.updateShape :111-128 — horizontal dir recomputes SHAPE; vertical unchanged.
        if (isHorizontal(dir)) {
            int ns = setProp(stateId, "shape", getStairsShape(stateId, level));
            return ns < 0 ? stateId : ns;
        }
        return stateId;
    }
    return -2;  // unported
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/block_update_shape.tsv";
    std::string statesPath = "mcpp/src/assets/block_states.json";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
        else if (a == "--states" && i + 1 < argc) statesPath = argv[++i];
    }

    {
        std::ifstream f(statesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << statesPath << "\n"; return 2; }
        nlohmann::json j; f >> j;
        auto arr = j.at("states");
        g_name.resize(arr.size()); g_props.resize(arr.size());
        for (auto& s : arr) {
            std::size_t id = s.at("id").get<std::size_t>();
            g_name[id] = s.at("name").get<std::string>();
            g_props[id] = s.value("props", std::string());
        }
        for (std::size_t id = 0; id < g_name.size(); ++id)
            g_index[g_name[id] + "\x1f" + g_props[id]] = (int)id;
    }

    // GT: OFFSETS (fixed cell order), U scenarios, FAM (block -> updateShape declaring class).
    std::vector<std::array<int,3>> offs;
    std::map<std::string, std::string> updFam;  // block name -> updateShape declaring class
    struct Scn { int centre; std::vector<int> nbr; int out; };
    std::vector<Scn> scns;
    {
        std::ifstream f(casesPath, std::ios::binary);
        if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        while (std::getline(f, line)) {
            auto c = splitTab(line);
            if (c.empty()) continue;
            if (c[0] == "OFFSETS") {
                int n = std::stoi(c[1]);
                for (int i = 0; i < n; ++i) {
                    std::size_t o = 2 + i * 3;
                    offs.push_back({ std::stoi(c[o]), std::stoi(c[o+1]), std::stoi(c[o+2]) });
                }
            } else if (c[0] == "U") {
                Scn s; s.centre = std::stoi(c[1]);
                int n = (int)offs.size();
                for (int i = 0; i < n; ++i) s.nbr.push_back(std::stoi(c[2 + i]));
                s.out = std::stoi(c[2 + n]);
                scns.push_back(std::move(s));
            } else if (c[0] == "FAM" && c.size() >= 3) {
                std::string key = c[1]; auto col = key.find(':'); if (col != std::string::npos) key = key.substr(col + 1);
                updFam[key] = c[2];
            }
        }
    }

    long cert = 0, mis = 0, todo = 0, skipped = 0;
    std::map<std::string, long> todoFam, misFam;
    int shown = 0;
    for (const Scn& s : scns) {
        if (s.out < 0) { ++skipped; continue; }  // GT proxy couldn't serve (outId=-1)
        const std::string& cfam = updFam.count(g_name[s.centre]) ? updFam[g_name[s.centre]] : "?";
        if (!PORTED.count(cfam)) { ++todo; todoFam[cfam]++; continue; }

        // build the neighbourhood cell array (dx+1)*9+(dy+1)*3+(dz+1).
        std::array<int,27> cells; cells.fill(0);
        for (std::size_t i = 0; i < offs.size(); ++i)
            cells[(offs[i][0]+1)*9 + (offs[i][1]+1)*3 + (offs[i][2]+1)] = s.nbr[i];
        Level level{ &cells };

        int cur = s.centre;
        for (int oi = 0; oi < 6; ++oi) {
            int dir = UPDATE_SHAPE_ORDER[oi];
            const std::string& fam = updFam.count(g_name[cur]) ? updFam[g_name[cur]] : "?";
            int r = updateShapeOne(fam, cur, dir, level.rel(dir), level);
            if (r == -2) { r = cur; }  // a (mid-loop) unported family acts as no-op
            cur = r;
        }
        if (cur == s.out) ++cert;
        else {
            ++mis; misFam[cfam]++;
            if (shown++ < 16) std::cerr << "mismatch centre=" << g_name[s.centre] << "[" << g_props[s.centre]
                << "] got=" << g_name[cur] << "[" << g_props[cur] << "] want=" << g_name[s.out]
                << "[" << g_props[s.out] << "]\n";
        }
    }

    std::cout << "BlockUpdateShape certified=" << cert << " mismatches=" << mis
              << " todo=" << todo << " skipped=" << skipped << "\n";
    std::vector<std::pair<std::string,long>> tv(todoFam.begin(), todoFam.end());
    std::sort(tv.begin(), tv.end(), [](auto&a,auto&b){ return a.second > b.second; });
    std::cout << "unported updateShape families (scenarios): ";
    for (std::size_t i = 0; i < tv.size() && i < 18; ++i) std::cout << tv[i].first << "=" << tv[i].second << " ";
    std::cout << "\n";
    if (mis > 0) {
        std::vector<std::pair<std::string,long>> mv(misFam.begin(), misFam.end());
        std::sort(mv.begin(), mv.end(), [](auto&a,auto&b){ return a.second > b.second; });
        std::cout << "mismatch families: ";
        for (std::size_t i = 0; i < mv.size() && i < 18; ++i) std::cout << mv[i].first << "=" << mv[i].second << " ";
        std::cout << "\n";
    }
    return mis > 0 ? 1 : 0;
}
