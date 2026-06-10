// Parity test for net.minecraft.world.level.chunk.DataLayer. Ground truth:
// tools/DataLayerParity.java vs the real class. Verifies getIndex/getNibbleIndex/
// getByteIndex/packFilled, the homogenous-default get() + packFilled-fill path, the
// byte[]-ctor get()+raw dump, a deterministic set()-sequence get()+raw dump, repeated
// overwrite masking, and val>15 wrapping — all bit-for-bit (raw bytes as signed ints).
//
//   data_layer_parity --cases mcpp/build/data_layer.tsv

#include "DataLayer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::world::level::chunk::DataLayer;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int i(const std::string& s) { return std::stoi(s); }
bool b(const std::string& s) { return s == "true"; }

// Build the deterministic byte[] pattern used by the Java CTOR case (full 0..255 range).
std::vector<int8_t> ctorPattern() {
    std::vector<int8_t> p(2048);
    for (int k = 0; k < 2048; ++k) p[k] = static_cast<int8_t>((k * 31 + 7) & 0xFF);
    return p;
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: data_layer_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    // Stateful objects keyed by their family/defaultValue, lazily constructed.
    std::map<int, std::unique_ptr<DataLayer>> defaultStore;   // DGET/DRAW : keyed by defaultValue
    std::unique_ptr<DataLayer> ctorLayer;                     // CGET/CRAW
    std::unique_ptr<DataLayer> setLayer;                      // SGET/SRAW (set sequence applied)
    std::unique_ptr<DataLayer> ovrLayer;                      // SETOVR/SETNBR
    std::unique_ptr<DataLayer> wrapLayer;                     // SETWRAP

    auto getDefault = [&](int dv) -> DataLayer& {
        auto& p = defaultStore[dv];
        if (!p) p = std::make_unique<DataLayer>(dv);
        return *p;
    };
    auto getCtor = [&]() -> DataLayer& {
        if (!ctorLayer) ctorLayer = std::make_unique<DataLayer>(ctorPattern());
        return *ctorLayer;
    };
    auto getSet = [&]() -> DataLayer& {
        if (!setLayer) {
            setLayer = std::make_unique<DataLayer>();
            for (int idx = 0; idx < 4096; ++idx) {
                int gx = idx & 15, gz = (idx >> 4) & 15, gy = (idx >> 8) & 15;
                int val = (idx * 13 + 5) & 15;
                setLayer->set(gx, gy, gz, val);
            }
        }
        return *setLayer;
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "GETIDX") {
            if (DataLayer::getIndex(i(p[1]), i(p[2]), i(p[3])) != i(p[4])) fail(line);
        } else if (t == "NIBBLE") {
            if (DataLayer::getNibbleIndex(i(p[1])) != i(p[2])) fail(line);
        } else if (t == "BYTEIDX") {
            if (DataLayer::getByteIndex(i(p[1])) != i(p[2])) fail(line);
        } else if (t == "PACK") {
            // packFilled is private in C++; replicate the exact body here to verify it.
            int value = i(p[1]);
            int8_t packed = static_cast<int8_t>(value);
            for (int s = 4; s < 8; s += 4)
                packed = static_cast<int8_t>(static_cast<int>(packed) | (value << s));
            if (static_cast<int>(packed) != i(p[2])) fail(line + " got=" + std::to_string(static_cast<int>(packed)));
        } else if (t == "DHOMO") {
            if (getDefault(i(p[1])).isDefinitelyHomogenous() != b(p[2])) fail(line);
        } else if (t == "DEMPTY") {
            if (getDefault(i(p[1])).isEmpty() != b(p[2])) fail(line);
        } else if (t == "DFILL") {
            if (getDefault(i(p[1])).isDefinitelyFilledWith(i(p[2])) != b(p[3])) fail(line);
        } else if (t == "DGET") {
            DataLayer& dl = getDefault(i(p[1]));
            int idx = i(p[2]); int gx = idx & 15, gz = (idx >> 4) & 15, gy = (idx >> 8) & 15;
            if (dl.get(gx, gy, gz) != i(p[3])) fail(line);
        } else if (t == "DRAWLEN") {
            if (static_cast<int>(getDefault(i(p[1])).getData().size()) != i(p[2])) fail(line);
        } else if (t == "DRAW") {
            if (static_cast<int>(getDefault(i(p[1])).getData()[i(p[2])]) != i(p[3])) fail(line);
        } else if (t == "DHOMO2") {
            // getData() was forced (DRAW rows preceded this) -> no longer homogenous
            if (getDefault(i(p[1])).isDefinitelyHomogenous() != b(p[2])) fail(line);
        } else if (t == "CGET") {
            int idx = i(p[1]); int gx = idx & 15, gz = (idx >> 4) & 15, gy = (idx >> 8) & 15;
            if (getCtor().get(gx, gy, gz) != i(p[2])) fail(line);
        } else if (t == "CRAW") {
            if (static_cast<int>(getCtor().getData()[i(p[1])]) != i(p[2])) fail(line);
        } else if (t == "SHOMO") {
            // Fresh empty layer is homogenous BEFORE any set; check on a throwaway.
            DataLayer fresh; if (fresh.isDefinitelyHomogenous() != b(p[1])) fail(line);
        } else if (t == "SGET") {
            int idx = i(p[1]); int gx = idx & 15, gz = (idx >> 4) & 15, gy = (idx >> 8) & 15;
            if (getSet().get(gx, gy, gz) != i(p[2])) fail(line);
        } else if (t == "SRAW") {
            if (static_cast<int>(getSet().getData()[i(p[1])]) != i(p[2])) fail(line);
        } else if (t == "SHOMO2") {
            if (getSet().isDefinitelyHomogenous() != b(p[1])) fail(line);
        } else if (t == "SETOVR") {
            if (!ovrLayer) ovrLayer = std::make_unique<DataLayer>();
            ovrLayer->set(3, 9, 6, i(p[1]));
            if (ovrLayer->get(3, 9, 6) != i(p[2])) fail(line + " got=" + std::to_string(ovrLayer->get(3, 9, 6)));
        } else if (t == "SETNBR") {
            if (!ovrLayer) ovrLayer = std::make_unique<DataLayer>();
            if (ovrLayer->get(2, 9, 6) != i(p[1])) fail(line);
        } else if (t == "SETWRAP") {
            if (!wrapLayer) wrapLayer = std::make_unique<DataLayer>();
            wrapLayer->set(0, 0, 0, i(p[1]));
            if (wrapLayer->get(0, 0, 0) != i(p[2])) fail(line + " got=" + std::to_string(wrapLayer->get(0, 0, 0)));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "DataLayer cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
