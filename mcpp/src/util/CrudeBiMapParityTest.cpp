// Parity gate for the CERTIFIED C++ port of
// net.minecraft.util.CrudeIncrementalIntIdentityHashBiMap, which already lives in
// util/IdBiMap.h (mc::util::CrudeIncrementalIntIdentityHashBiMap). This test does
// NOT introduce a new port — it VERIFIES the existing engine header against an
// independent ground-truth battery produced by tools/CrudeBiMapParity.java driving
// the REAL net.minecraft class.
//
//   crude_bimap_parity --cases mcpp/build/crude_bimap.tsv
//
// The TSV is a flat, in-emission-order stream of tagged rows; emission order ==
// execution order. We replay each scenario row-by-row against a fresh C++ map
// (re-created on the NEW row with the exact create(initialCapacity) argument) and
// compare every observable result BIT-FOR-BIT. All outputs are small ints/bools, so
// we compare them as integers via std::bit_cast on int (a no-op here, but keeps the
// "compare bits, never float value" contract explicit).
//
// Row TAGs (see CrudeBiMapParity.java):
//   NEW   <scen> <initialCapacity>          create(initialCapacity); resets the map (input)
//   KEY   <scen> <opaqueId> <identityHash>  register a distinct key (input)
//   ADD   <scen> <opaqueId> <assignedId>    map.add(key); expected assigned id
//   MAP   <scen> <opaqueId> <explicitId>    map.addMapping(key, explicitId) (input)
//   GETID <scen> <opaqueId> <getId>         expected map.getId(key)
//   BYID  <scen> <queryId> <opaqueOrMinus1> expected opaque of map.byId(queryId)
//   CONT  <scen> <opaqueId> <0|1>           expected map.contains(key)
//   CONTI <scen> <queryId> <0|1>            expected map.contains(id)
//   SIZE  <scen> <size> <nextId>            expected size() and nextId field
//   CAP   <scen> <capacity>                 expected keys.length

#include "util/IdBiMap.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using mc::util::CrudeIncrementalIntIdentityHashBiMap;
using mc::util::IdKey;

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int toi(const std::string& s) { return std::stoi(s); }

// Bit-for-bit integer comparison (explicit per gate contract).
bool bitsEqual(int got, int exp) {
    return std::bit_cast<std::uint32_t>(got) == std::bit_cast<std::uint32_t>(exp);
}

struct Row {
    std::string tag;
    std::string scen;
    int a = 0;
    int b = 0;
    bool hasB = false;
};

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: crude_bimap_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    std::vector<Row> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.size() < 2) continue;
        Row r;
        r.tag = p[0];
        r.scen = p[1];
        if (p.size() >= 3) r.a = toi(p[2]);
        if (p.size() >= 4) {
            r.b = toi(p[3]);
            r.hasB = true;
        }
        rows.push_back(std::move(r));
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& what, const Row& r, long long got, long long exp) {
        ++mism;
        if (shown++ < 40)
            std::cerr << "MISMATCH " << what << " scen=" << r.scen << " a=" << r.a
                      << " got=" << got << " exp=" << exp << "\n";
    };

    std::unique_ptr<CrudeIncrementalIntIdentityHashBiMap> map;
    std::unordered_map<int, IdKey> keyByOpaque;  // opaqueId -> reconstructed IdKey

    for (const auto& r : rows) {
        if (r.tag == "NEW") {
            // r.a = initialCapacity argument to create(). Reset the live map.
            map = std::make_unique<CrudeIncrementalIntIdentityHashBiMap>(
                CrudeIncrementalIntIdentityHashBiMap::create(r.a));
            keyByOpaque.clear();
            continue;
        }
        if (!map) continue;  // defensive: rows before any NEW

        if (r.tag == "KEY") {
            IdKey k;
            k.id = r.a;            // opaque reference token
            k.identityHash = r.b;  // System.identityHashCode value
            keyByOpaque[r.a] = k;
        } else if (r.tag == "ADD") {
            ++total;
            int got = map->add(keyByOpaque.at(r.a));
            if (!bitsEqual(got, r.b)) fail("ADD", r, got, r.b);
        } else if (r.tag == "MAP") {
            map->addMapping(keyByOpaque.at(r.a), r.b);
        } else if (r.tag == "GETID") {
            ++total;
            int got = map->getId(keyByOpaque.at(r.a));
            if (!bitsEqual(got, r.b)) fail("GETID", r, got, r.b);
        } else if (r.tag == "BYID") {
            ++total;
            const IdKey& res = map->byId(r.a);
            int gotOpaque = res.isNull() ? -1 : res.id;
            if (!bitsEqual(gotOpaque, r.b)) fail("BYID", r, gotOpaque, r.b);
        } else if (r.tag == "CONT") {
            ++total;
            int got = map->contains(keyByOpaque.at(r.a)) ? 1 : 0;
            if (!bitsEqual(got, r.b)) fail("CONT", r, got, r.b);
        } else if (r.tag == "CONTI") {
            ++total;
            int got = map->contains(r.a) ? 1 : 0;
            if (!bitsEqual(got, r.b)) fail("CONTI", r, got, r.b);
        } else if (r.tag == "SIZE") {
            ++total;
            if (!bitsEqual(map->size(), r.a)) fail("SIZE.size", r, map->size(), r.a);
            ++total;
            if (!bitsEqual(map->nextIdField(), r.b))
                fail("SIZE.nextId", r, map->nextIdField(), r.b);
        } else if (r.tag == "CAP") {
            ++total;
            if (!bitsEqual(map->capacity(), r.a)) fail("CAP", r, map->capacity(), r.a);
        }
        // unknown tags ignored
    }

    std::cout << "CrudeBiMap cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
