// Parity test for the PURE room-type fitter predicates + claim-marking in
//   net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//     .MonumentRoomFitter implementations (FitDoubleXY/YZ/Z/X/Y, FitSimpleTop,
//      FitSimpleRoom): boolean fits(RoomDefinition) and the claimed[] effect of
//      OceanMonumentPiece create(Direction, RoomDefinition, RandomSource)  (26.1.2).
//
// Ground truth: tools/OceanMonumentRoomFitterParity.java drives the REAL private
// fitter classes (instantiated via reflection) on identically-built monument room
// graphs, exercising fits() over a battery of scripted graph states and snapshotting
// the claimed[] flags after each REAL create(). This test rebuilds the SAME graphs
// from OceanMonumentRoomFitter.h (+ OceanMonumentRoomGraph.h), applies the identical
// scripted mutations, recomputes fits/first/create, and compares every row. All
// values are pure booleans / integer bitsets, so the gate is exact.
//
//   ocean_monument_room_fitter_parity --cases mcpp/build/ocean_monument_room_fitter.tsv

#include "OceanMonumentRoomFitter.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::oceanmonumentfitter;
namespace omg = mc::levelgen::structure::oceanmonumentgraph;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return std::stoi(s); }
long long tol(const std::string& s) { return std::stoll(s); }

// 3D-data index helpers (mirror Java Direction.<DIR>.get3DDataValue()).
constexpr int DOWN = omg::get3DDataValue(omg::Direction::DOWN);
constexpr int UP = omg::get3DDataValue(omg::Direction::UP);
constexpr int NORTH = omg::get3DDataValue(omg::Direction::NORTH);
constexpr int SOUTH = omg::get3DDataValue(omg::Direction::SOUTH);
constexpr int WEST = omg::get3DDataValue(omg::Direction::WEST);
constexpr int EAST = omg::get3DDataValue(omg::Direction::EAST);

using omg::getRoomIndex;

// Build the pristine deterministic graph (scenario 0 base) into a RoomGraph.
omg::RoomGraph baseGraph() { return omg::buildDeterministicRoomGraph().g; }

// Reconstruct the exact graph state for a given FITS/FIRST scenario id, applying the
// SAME scripted mutations the Java GT applies (OceanMonumentRoomFitterParity.main).
omg::RoomGraph fitsScenarioGraph(int scenarioId) {
    omg::RoomGraph g = baseGraph();
    switch (scenarioId) {
        case 0:
            break;
        case 1: {
            // claim the whole middle (y=1) layer
            for (int x = 0; x < 5; ++x)
                for (int z = 0; z < 5; ++z) {
                    int id = g.grid[getRoomIndex(x, 1, z)];
                    if (id != -1) g.rooms[id].claimed = true;
                }
            break;
        }
        case 2: {
            // clear EVERY opening on the x=0..4, y=0, z=0 row
            for (int x = 0; x < 5; ++x) {
                int id = g.grid[getRoomIndex(x, 0, 0)];
                for (int i = 0; i < 6; ++i) g.rooms[id].hasOpening[i] = false;
            }
            break;
        }
        case 3: {
            // claim the EAST neighbour of the x=0..3, y=0, z=1 row
            for (int x = 0; x < 4; ++x) {
                int id = g.grid[getRoomIndex(x, 0, 1)];
                int east = g.rooms[id].connections[EAST];
                if (east != -1) g.rooms[east].claimed = true;
            }
            break;
        }
        case 4: {
            // clear ONLY the four horizontal openings (W/E/N/S), leaving UP open
            for (int x = 1; x < 4; ++x) {
                int id = g.grid[getRoomIndex(x, 0, 1)];
                g.rooms[id].hasOpening[WEST] = false;
                g.rooms[id].hasOpening[EAST] = false;
                g.rooms[id].hasOpening[NORTH] = false;
                g.rooms[id].hasOpening[SOUTH] = false;
            }
            break;
        }
        case 5: {
            // horizontals cleared AND UP neighbour claimed (UP opening stays open)
            for (int x = 1; x < 4; ++x) {
                int id = g.grid[getRoomIndex(x, 0, 1)];
                g.rooms[id].hasOpening[WEST] = false;
                g.rooms[id].hasOpening[EAST] = false;
                g.rooms[id].hasOpening[NORTH] = false;
                g.rooms[id].hasOpening[SOUTH] = false;
                int up = g.rooms[id].connections[UP];
                if (up != -1) g.rooms[up].claimed = true;
            }
            break;
        }
        case 6: {
            // ALL six openings cleared on the x=1..3, y=0, z=1 rooms
            for (int x = 1; x < 4; ++x) {
                int id = g.grid[getRoomIndex(x, 0, 1)];
                for (int i = 0; i < 6; ++i) g.rooms[id].hasOpening[i] = false;
            }
            break;
        }
        default:
            break;
    }
    return g;
}

// Build the graph + the (fitterIdx, defId) for a given CLAIM create battery entry,
// matching the Java GT's emitCreate / emitCreateOn call sequence in main().
struct CreateCase {
    omg::RoomGraph g;
    int fitterIdx;
    int defId;
};

CreateCase createCase(int createId) {
    omg::RoomGraph g = baseGraph();
    switch (createId) {
        case 0: return {g, 0, g.grid[getRoomIndex(0, 0, 0)]}; // DoubleXY
        case 1: return {g, 1, g.grid[getRoomIndex(1, 0, 1)]}; // DoubleYZ
        case 2: return {g, 2, g.grid[getRoomIndex(2, 0, 1)]}; // DoubleZ happy path
        case 3: {                                             // DoubleZ source-swap
            int def = g.grid[getRoomIndex(2, 0, 1)];
            int north = g.rooms[def].connections[NORTH];
            if (north != -1) g.rooms[north].claimed = true;
            return {g, 2, def};
        }
        case 4: return {g, 3, g.grid[getRoomIndex(0, 0, 0)]}; // DoubleX
        case 5: return {g, 4, g.grid[getRoomIndex(1, 0, 0)]}; // DoubleY
        case 6: {                                             // SimpleTop (fully closed)
            int def = g.grid[getRoomIndex(0, 0, 0)];
            for (int i = 0; i < 6; ++i) g.rooms[def].hasOpening[i] = false;
            return {g, 5, def};
        }
        case 7: return {g, 6, g.grid[getRoomIndex(3, 0, 0)]}; // Simple
        default: return {g, 6, g.grid[getRoomIndex(3, 0, 0)]};
    }
}

FitterKind kindOf(int probeIdx) { return PROBE_ORDER[probeIdx]; }

} // namespace

int main(int argc, char** argv) {
    (void)DOWN; // DOWN index defined for completeness; fitters never test the DOWN face
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: ocean_monument_room_fitter_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << tag << " " << detail << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "FITS") {
            // FITS <scenarioId> <fitterIdx> <nodeId> <0|1>
            if (p.size() < 5) continue;
            int scenario = toi(p[1]);
            int fitterIdx = toi(p[2]);
            int nodeId = toi(p[3]);
            int exp = toi(p[4]);
            omg::RoomGraph g = fitsScenarioGraph(scenario);
            int got = fits(kindOf(fitterIdx), g, nodeId) ? 1 : 0;
            ++total;
            if (got != exp)
                fail("FITS", "scn=" + p[1] + " fit=" + p[2] + " node=" + p[3] +
                                 " exp=" + p[4] + " got=" + std::to_string(got));
        } else if (tag == "FIRST") {
            // FIRST <scenarioId> <nodeId> <fitterIdx>
            if (p.size() < 4) continue;
            int scenario = toi(p[1]);
            int nodeId = toi(p[2]);
            int exp = toi(p[3]);
            omg::RoomGraph g = fitsScenarioGraph(scenario);
            int got = static_cast<int>(fitFirst(g, nodeId));
            ++total;
            if (got != exp)
                fail("FIRST", "scn=" + p[1] + " node=" + p[2] + " exp=" + p[3] +
                                  " got=" + std::to_string(got));
        } else if (tag == "CLAIM") {
            // CLAIM <createId> <fitterIdx> <defNodeId> <lo> <hi>
            if (p.size() < 6) continue;
            int createId = toi(p[1]);
            int fitterIdx = toi(p[2]);
            int defNode = toi(p[3]);
            long long expLo = tol(p[4]);
            long long expHi = tol(p[5]);
            CreateCase cc = createCase(createId);
            // sanity: the recomputed case must agree on fitter and def node.
            ++total;
            if (cc.fitterIdx != fitterIdx || cc.defId != defNode) {
                fail("CLAIM.setup", "createId=" + p[1] + " expFit=" + p[2] + " expDef=" + p[3] +
                                        " gotFit=" + std::to_string(cc.fitterIdx) +
                                        " gotDef=" + std::to_string(cc.defId));
            }
            create(kindOf(fitterIdx), cc.g, defNode);
            uint64_t lo = 0, hi = 0;
            for (int id = 0; id < static_cast<int>(cc.g.rooms.size()); ++id) {
                if (cc.g.rooms[id].claimed) {
                    if (id < 64) lo |= (uint64_t(1) << id);
                    else hi |= (uint64_t(1) << (id - 64));
                }
            }
            ++total;
            if (static_cast<long long>(lo) != expLo)
                fail("CLAIM.lo", "createId=" + p[1] + " exp=" + p[4] +
                                     " got=" + std::to_string(static_cast<long long>(lo)));
            ++total;
            if (static_cast<long long>(hi) != expHi)
                fail("CLAIM.hi", "createId=" + p[1] + " exp=" + p[5] +
                                     " got=" + std::to_string(static_cast<long long>(hi)));
        }
        // Unknown tags (stray bootstrap log lines) are skipped.
    }

    std::cout << "OceanMonumentRoomFitter checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
