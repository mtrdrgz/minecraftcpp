// Parity test for net.minecraft.util.Graph (depthFirstSearch).
// Ground truth: tools/GraphSortParity.java vs the real class. Each fixed int
// adjacency graph is replayed through util/Graph.h and compared exactly: the
// cycle-detection boolean, the reverse-topological order produced, and the
// discovered-vertex insertion order.
//
//   graph_sort_parity --cases mcpp/build/graph_sort.tsv
//
// TSV row formats (see the GT tool header):
//   DFS    graphSpec  root        cycle  order(csv)  discovered(csv)
//   SORT   graphSpec              cycle  order(csv)
//
// graphSpec = "V|E": V = ascending vertex list "v0,v1,..."; E = ";"-separated
// "src>dst0,dst1,..." (verts with no out-edges omitted). order/discovered csv use
// "EMPTY" for the empty list.

#include "Graph.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

std::vector<std::string> splitTab(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

std::vector<int> splitCsvInts(const std::string& field) {
    if (field == "EMPTY" || field.empty()) return {};
    std::vector<int> out;
    std::string it;
    std::istringstream ss(field);
    while (std::getline(ss, it, ',')) out.push_back(std::stoi(it));
    return out;
}

std::string csvInts(const std::vector<int>& xs) {
    if (xs.empty()) return "EMPTY";
    std::string out;
    for (size_t i = 0; i < xs.size(); ++i) {
        if (i) out.push_back(',');
        out += std::to_string(xs[i]);
    }
    return out;
}

// Parse a "V|E" graph spec into an ascending-keyed adjacency map. Every vertex in
// V is present as a key (possibly with an empty edge list); edges keep their
// serialised (ascending) order verbatim.
mc::util::graph::Edges<int> parseSpec(const std::string& spec) {
    mc::util::graph::Edges<int> edges;
    auto bar = spec.find('|');
    std::string vpart = spec.substr(0, bar);
    std::string epart = (bar == std::string::npos) ? "" : spec.substr(bar + 1);

    // vertices
    for (int v : splitCsvInts(vpart)) edges[v];  // ensure key exists, empty edge list

    // edges: ";"-separated "src>dst,dst,..."
    if (!epart.empty()) {
        std::string group;
        std::istringstream es(epart);
        while (std::getline(es, group, ';')) {
            if (group.empty()) continue;
            auto gt = group.find('>');
            int src = std::stoi(group.substr(0, gt));
            std::string dsts = group.substr(gt + 1);
            std::vector<int>& vec = edges[src];
            std::string d;
            std::istringstream ds(dsts);
            while (std::getline(ds, d, ',')) vec.push_back(std::stoi(d));
        }
    }
    return edges;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: graph_sort_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTab(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "DFS") {  // graphSpec root cycle order disc
            auto edges = parseSpec(p[1]);
            int root = std::stoi(p[2]);
            bool expectCycle = std::stoi(p[3]) != 0;
            std::string expectOrder = p[4];
            std::string expectDisc = p[5];

            std::unordered_set<int> discovered, visiting;
            std::vector<int> order;
            // Track discovered-insertion order to mirror Java's LinkedHashSet. A
            // vertex is added to `discovered` on the same step it is appended to the
            // order callback, so this list reproduces the LinkedHashSet iteration.
            std::vector<int> discOrder;
            auto cb = [&](const int& v) {
                order.push_back(v);
                discOrder.push_back(v);
            };
            bool cycle = mc::util::graph::depthFirstSearch<int>(
                edges, discovered, visiting, cb, root);

            bad = (cycle != expectCycle) || (csvInts(order) != expectOrder)
                  || (csvInts(discOrder) != expectDisc);
        } else if (tag == "SORT") {  // graphSpec cycle order
            auto edges = parseSpec(p[1]);
            bool expectCycle = std::stoi(p[2]) != 0;
            std::string expectOrder = p[3];

            std::unordered_set<int> discovered, visiting;
            std::vector<int> order;
            auto cb = [&](const int& v) { order.push_back(v); };
            bool cycle = false;
            for (const auto& kv : edges) {  // std::map -> ascending keys (== TreeMap)
                int v = kv.first;
                if (discovered.find(v) == discovered.end()
                    && mc::util::graph::depthFirstSearch<int>(
                           edges, discovered, visiting, cb, v)) {
                    cycle = true;
                    break;
                }
            }
            bad = (cycle != expectCycle) || (csvInts(order) != expectOrder);
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            ++mism;
            continue;
        }

        if (bad) {
            ++mism;
            if (mism <= 20) std::cerr << "MISMATCH [" << tag << "] line: " << line << "\n";
        }
    }

    std::cout << "Graph cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
