// Parity test for the PURE room-connectivity graph in
//   net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//     .RoomDefinition (setConnection / updateOpenings / findSource / isSpecial /
//      countOpenings) + the deterministic generateRoomGraph grid build  (26.1.2).
//
// Ground truth: tools/OceanMonumentRoomGraphParity.java drives the REAL private
// RoomDefinition via reflection, builds the deterministic 5x3x5 monument room
// graph, applies a fixed (RNG-free) prune script, and emits a TSV. This test
// rebuilds the same graph from OceanMonumentRoomGraph.h, applies the identical
// scripted ops, and compares every row. All values are pure ints/booleans, so the
// gate is exact (decimal compare).
//
//   ocean_monument_room_graph_parity --cases mcpp/build/ocean_monument_room_graph.tsv

#include "OceanMonumentRoomGraph.h"

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace mc::levelgen::structure::oceanmonumentgraph;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return std::stoi(s); }

// from3DDataValue inverse table (Direction.from3DDataValue) — maps a 3D-data index
// back to the Direction enum value (here data == ordinal, but kept explicit).
Direction from3DDataValue(int v) {
    switch (v) {
        case 0: return Direction::DOWN;
        case 1: return Direction::UP;
        case 2: return Direction::NORTH;
        case 3: return Direction::SOUTH;
        case 4: return Direction::WEST;
        case 5: return Direction::EAST;
    }
    return Direction::DOWN;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: ocean_monument_room_graph_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    // ── Rebuild the deterministic graph; node ids are creation order, which the GT
    // mirrors (same construction order) so nodeId<->node correspondence matches. ──
    BuiltGraph built = buildDeterministicRoomGraph();
    RoomGraph& g = built.g;
    auto& rooms = g.rooms;

    // The scripted prune sequence MUST be byte-identical to the Java GT's `closes`.
    struct Close { int gridIndex; int f; };
    const std::array<Close, 12> closes = {{
        {getRoomIndex(0, 0, 0), 5},
        {getRoomIndex(1, 0, 0), 5},
        {getRoomIndex(2, 0, 0), 3},
        {getRoomIndex(0, 0, 0), 3},
        {getRoomIndex(3, 0, 1), 1},
        {getRoomIndex(2, 1, 0), 2},
        {getRoomIndex(4, 0, 0), 0},
        {getRoomIndex(1, 1, 1), 4},
        {getRoomIndex(2, 0, 1), 1},
        {getRoomIndex(0, 1, 0), 3},
        {getRoomIndex(3, 2, 0), 0},
        {getRoomIndex(1, 0, 2), 5},
    }};

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << tag << " " << detail << "\n";
    };

    // We replay the TSV in order. Static rows (INDEX/SPEC/CONN/OPEN0/FS0) are read
    // and checked against the post-build state. Then PRUNE rows drive the scripted
    // close sequence (and we check `accepted`). Then OPEN1/FS1 re-check final state.
    // FS0/FS1 mutate scanIndex; we apply the identical findSource calls so state
    // stays in lockstep with the GT.
    int fs0Scan = 1;   // GT: emitFindSource("FS0", 1)
    int pruneStep = 0;
    int pruneScan = 2; // GT: scan starts at 2 for the prune loop
    int fs1Scan = -1;  // captured: GT uses `scan` (post-prune) as FS1 start

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTabs(line);
        const std::string& tag = p[0];

        if (tag == "INDEX") {
            int id = toi(p[1]);
            ++total;
            if (rooms[id].index != toi(p[2]))
                fail("INDEX", "id=" + p[1] + " exp=" + p[2] + " got=" + std::to_string(rooms[id].index));
        } else if (tag == "SPEC") {
            int id = toi(p[1]);
            ++total;
            int got = rooms[id].isSpecial() ? 1 : 0;
            if (got != toi(p[2]))
                fail("SPEC", "id=" + p[1] + " exp=" + p[2] + " got=" + std::to_string(got));
        } else if (tag == "CONN") {
            int id = toi(p[1]);
            for (int i = 0; i < 6; ++i) {
                ++total;
                if (rooms[id].connections[i] != toi(p[2 + i]))
                    fail("CONN", "id=" + p[1] + " i=" + std::to_string(i) + " exp=" + p[2 + i] +
                                     " got=" + std::to_string(rooms[id].connections[i]));
            }
        } else if (tag == "OPEN0" || tag == "OPEN1") {
            int id = toi(p[1]);
            for (int i = 0; i < 6; ++i) {
                ++total;
                int got = rooms[id].hasOpening[i] ? 1 : 0;
                if (got != toi(p[2 + i]))
                    fail(tag, "id=" + p[1] + " i=" + std::to_string(i) + " exp=" + p[2 + i] +
                                  " got=" + std::to_string(got));
            }
            ++total;
            int gotCount = rooms[id].countOpenings();
            if (gotCount != toi(p[8]))
                fail(tag + ".count", "id=" + p[1] + " exp=" + p[8] + " got=" + std::to_string(gotCount));
        } else if (tag == "FS0") {
            int id = toi(p[1]);
            ++total;
            int got = g.findSource(id, fs0Scan++) ? 1 : 0;
            if (got != toi(p[2]))
                fail("FS0", "id=" + p[1] + " exp=" + p[2] + " got=" + std::to_string(got));
        } else if (tag == "PRUNE") {
            // Replay the close attempt exactly like the GT prune body.
            const Close& cl = closes[pruneStep];
            int defId = g.grid[cl.gridIndex];
            int f = cl.f;
            int accepted = 0;
            RoomDefinition& def = rooms[defId];
            if (def.hasOpening[f]) {
                int connId = def.connections[f];
                int of = oppositeIndex(from3DDataValue(f));
                def.hasOpening[f] = false;
                rooms[connId].hasOpening[of] = false;
                bool a = g.findSource(defId, pruneScan++);
                bool b = g.findSource(connId, pruneScan++);
                if (a && b) {
                    accepted = 1;
                } else {
                    def.hasOpening[f] = true;
                    rooms[connId].hasOpening[of] = true;
                }
            }
            ++total;
            // GT row: PRUNE <step> <nodeId> <f> <accepted>
            if (toi(p[1]) != pruneStep)
                fail("PRUNE.step", "exp=" + p[1] + " got=" + std::to_string(pruneStep));
            if (toi(p[2]) != defId)
                fail("PRUNE.node", "step=" + std::to_string(pruneStep) + " exp=" + p[2] +
                                       " got=" + std::to_string(defId));
            if (toi(p[3]) != f)
                fail("PRUNE.f", "step=" + std::to_string(pruneStep) + " exp=" + p[3] +
                                    " got=" + std::to_string(f));
            if (toi(p[4]) != accepted)
                fail("PRUNE.accepted", "step=" + std::to_string(pruneStep) + " exp=" + p[4] +
                                           " got=" + std::to_string(accepted));
            ++pruneStep;
            if (pruneStep == static_cast<int>(closes.size()))
                fs1Scan = pruneScan; // GT: emitFindSource("FS1", scan) with the post-prune scan
        } else if (tag == "FS1") {
            int id = toi(p[1]);
            ++total;
            int got = g.findSource(id, fs1Scan++) ? 1 : 0;
            if (got != toi(p[2]))
                fail("FS1", "id=" + p[1] + " exp=" + p[2] + " got=" + std::to_string(got));
        }
        // Unknown tags (e.g. stray bootstrap log lines) are skipped.
    }

    std::cout << "OceanMonumentRoomGraph checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
