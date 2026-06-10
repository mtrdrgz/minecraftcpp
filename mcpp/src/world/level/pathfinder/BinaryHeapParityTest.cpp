// Bit-exact parity gate for net.minecraft.world.level.pathfinder.BinaryHeap.
//
// Reads the TSV emitted by BinaryHeapParity.java. Each row is one scenario:
//   SCN  <id>  <script>  <popOrder>  <finalHeapIds>  <finalIdxMap>
// The C++ port replays <script> VERBATIM against mc::pathfinder::BinaryHeap and
// checks that the resulting pop/peek order, final heap-array order, and per-node
// heapIdx map match the Java ground truth EXACTLY. Float keys are exchanged as
// raw IEEE-754 bits (%08x) so the min-heap comparison is bit-identical.
//
// Replay guards mirror the GT VERBATIM:
//   * C/R on a node with heapIdx < 0 (not in heap) is a no-op (op still present
//     in the script, but no heap method is invoked) — BinaryHeap.java guards the
//     caller, not the method, so the GT and C++ both skip identically.
//   * P/K on an empty heap is a no-op (no id recorded).

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "world/level/pathfinder/BinaryHeap.h"

using mc::pathfinder::BinaryHeap;
using mc::pathfinder::HeapNode;

static float bf(const std::string& s) {
    return std::bit_cast<float>(static_cast<std::uint32_t>(std::stoul(s, nullptr, 16)));
}

static std::vector<std::string> splitTab(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

static std::vector<std::string> splitWs(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string tok;
    while (ss >> tok) out.push_back(tok);
    return out;
}

// Replay one scenario; produce popOrder, finalHeapIds, finalIdxMap as strings
// matching the GT serialization exactly.
struct Replay {
    std::string popOrder;
    std::string finalHeapIds;
    std::string finalIdxMap;
};

static Replay replay(const std::vector<std::string>& tk) {
    BinaryHeap heap;
    // Owned nodes, keyed by id. node lifetime spans the whole scenario.
    std::unordered_map<int, HeapNode*> nodes;
    std::map<int, HeapNode*> createdSorted;  // id -> node, sorted by id for idx map
    std::vector<HeapNode*> owned;

    std::vector<std::string> pops;

    for (std::size_t i = 0; i < tk.size();) {
        const std::string& op = tk[i];
        if (op == "I") {
            int id = std::stoi(tk[i + 1]);
            float cost = bf(tk[i + 2]);
            HeapNode* n = new HeapNode(id, cost);
            owned.push_back(n);
            nodes[id] = n;
            createdSorted[id] = n;
            heap.insert(n);
            i += 3;
        } else if (op == "C") {
            int id = std::stoi(tk[i + 1]);
            float cost = bf(tk[i + 2]);
            auto it = nodes.find(id);
            if (it != nodes.end() && it->second->heapIdx >= 0) {
                heap.changeCost(it->second, cost);
            }
            i += 3;
        } else if (op == "R") {
            int id = std::stoi(tk[i + 1]);
            auto it = nodes.find(id);
            if (it != nodes.end() && it->second->heapIdx >= 0) {
                heap.remove(it->second);
            }
            i += 2;
        } else if (op == "P") {
            if (!heap.isEmpty()) {
                HeapNode* p = heap.pop();
                pops.push_back(std::to_string(p->id));
            }
            i += 1;
        } else if (op == "K") {
            if (!heap.isEmpty()) {
                HeapNode* p = heap.peek();
                pops.push_back("k" + std::to_string(p->id));
            }
            i += 1;
        } else {
            // unknown token — skip defensively
            i += 1;
        }
    }

    Replay r;
    // popOrder
    {
        std::string s;
        for (std::size_t k = 0; k < pops.size(); ++k) {
            if (k) s += ' ';
            s += pops[k];
        }
        r.popOrder = s;
    }
    // finalHeapIds (getHeap order)
    {
        auto h = heap.getHeap();
        std::string s;
        for (std::size_t k = 0; k < h.size(); ++k) {
            if (k) s += ' ';
            s += std::to_string(h[k]->id);
        }
        r.finalHeapIds = s;
    }
    // finalIdxMap "<id>:<heapIdx>" sorted by id
    {
        std::string s;
        bool first = true;
        for (auto& e : createdSorted) {
            if (!first) s += ' ';
            first = false;
            s += std::to_string(e.first);
            s += ':';
            s += std::to_string(e.second->heapIdx);
        }
        r.finalIdxMap = s;
    }

    for (HeapNode* n : owned) delete n;
    return r;
}

int main(int argc, char** argv) {
    std::string tsv;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) tsv = argv[++i];
    }
    if (tsv.empty()) { std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]); return 2; }

    std::ifstream in(tsv);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", tsv.c_str()); return 2; }

    long cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        auto t = splitTab(line);
        if (t.empty() || t[0] != "SCN") continue;
        // SCN  id  script  popOrder  finalHeapIds  finalIdxMap
        // Trailing fields may be empty strings (e.g. empty heap) — pad to 6.
        while (t.size() < 6) t.push_back("");
        const std::string& id = t[1];
        const std::string& script = t[2];
        const std::string& expPop = t[3];
        const std::string& expHeap = t[4];
        const std::string& expIdx = t[5];

        auto ops = splitWs(script);
        Replay r = replay(ops);

        ++cases;
        bool ok = (r.popOrder == expPop) && (r.finalHeapIds == expHeap) && (r.finalIdxMap == expIdx);
        if (!ok) {
            ++mismatches;
            if (mismatches <= 20) {
                std::fprintf(stderr, "SCN %s mismatch:\n", id.c_str());
                if (r.popOrder != expPop)
                    std::fprintf(stderr, "  pop  exp[%s] got[%s]\n", expPop.c_str(), r.popOrder.c_str());
                if (r.finalHeapIds != expHeap)
                    std::fprintf(stderr, "  heap exp[%s] got[%s]\n", expHeap.c_str(), r.finalHeapIds.c_str());
                if (r.finalIdxMap != expIdx)
                    std::fprintf(stderr, "  idx  exp[%s] got[%s]\n", expIdx.c_str(), r.finalIdxMap.c_str());
            }
        }
    }

    std::printf("BinaryHeap cases=%ld mismatches=%ld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
