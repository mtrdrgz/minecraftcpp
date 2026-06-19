// Bit-exact parity gate for the pure cell/geometry math of
// net.minecraft.client.renderer.CloudRenderer (Minecraft 26.1.2). Reads the TSV
// emitted by tools/CloudCellMathParity.java (which drives the REAL private
// helpers via reflection + an Unsafe-allocated GL-free instance) and re-runs the
// C++ render/CloudCellMath.h port for each row, comparing every result exactly.
//
//   ints/longs   : decimal
//   floats/doubles: raw IEEE-754 bit patterns (floatToRawIntBits /
//                   doubleToRawLongBits) compared via std::bit_cast
//
//   mcpp/build/cloud_cell_math_parity.exe --cases mcpp/build/cloud_cell_math.tsv
//
// Row tags: PACK, EMPTY, SIDES, SIZE, FACE, POS (see the GT tool header).

#include "render/CloudCellMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cc = mc::render::cloud;

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

int64_t L(const std::string& s) { return std::stoll(s); }
int32_t I(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }
float fbits(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s)));
}
double dbits(const std::string& s) {
    return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(s)));
}
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: cloud_cell_math_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long checks = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& msg) {
        ++mism;
        if (shown++ < 30) std::cerr << "MISMATCH " << msg << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "EMPTY") {
            // EMPTY <color> <isEmpty>
            ++checks;
            int color = I(p[1]);
            int exp = I(p[2]);
            int got = cc::isCellEmpty(color) ? 1 : 0;
            if (got != exp) fail(line);
        } else if (tag == "PACK") {
            // PACK <color> <n> <e> <s> <w> <packedLong>
            ++checks;
            int color = I(p[1]);
            bool n = I(p[2]) != 0, e = I(p[3]) != 0, s = I(p[4]) != 0, w = I(p[5]) != 0;
            int64_t exp = L(p[6]);
            int64_t got = cc::packCellData(color, n, e, s, w);
            if (got != exp) fail(line);
        } else if (tag == "SIDES") {
            // SIDES <cellData> <north> <east> <south> <west>
            int64_t cd = L(p[1]);
            int en = I(p[2]), ee = I(p[3]), es = I(p[4]), ew = I(p[5]);
            ++checks;
            if ((cc::isNorthEmpty(cd) ? 1 : 0) != en) fail(line + " [north]");
            ++checks;
            if ((cc::isEastEmpty(cd) ? 1 : 0) != ee) fail(line + " [east]");
            ++checks;
            if ((cc::isSouthEmpty(cd) ? 1 : 0) != es) fail(line + " [south]");
            ++checks;
            if ((cc::isWestEmpty(cd) ? 1 : 0) != ew) fail(line + " [west]");
        } else if (tag == "SIZE") {
            // SIZE <radiusCells> <int32>
            ++checks;
            int32_t r = I(p[1]);
            int32_t exp = I(p[2]);
            int32_t got = cc::getSizeForCloudDistance(r);
            if (got != exp) fail(line);
        } else if (tag == "FACE") {
            // FACE <dir3d> <flags> <x> <z> <bx> <bz> <bDirFlags>
            ++checks;
            cc::Dir3D dir = static_cast<cc::Dir3D>(I(p[1]));
            int32_t flags = I(p[2]);
            int32_t x = I(p[3]);
            int32_t z = I(p[4]);
            int ebx = I(p[5]), ebz = I(p[6]), ebdf = I(p[7]);
            cc::EncodedFace ef = cc::encodeFace(dir, flags, x, z);
            if (static_cast<int>(ef.bx) != ebx ||
                static_cast<int>(ef.bz) != ebz ||
                static_cast<int>(ef.bDirFlags) != ebdf) {
                fail(line + " got bx=" + std::to_string(ef.bx) + " bz=" +
                     std::to_string(ef.bz) + " bdf=" + std::to_string(ef.bDirFlags));
            }
        } else if (tag == "POS") {
            // POS <w> <h> <camXbits> <camZbits> <gameTime> <ptBits>
            //     <cloudXbits> <cloudZbits> <cellX> <cellZ> <xInCellBits> <zInCellBits>
            int width = I(p[1]);
            int height = I(p[2]);
            double camX = dbits(p[3]);
            double camZ = dbits(p[4]);
            int64_t gameTime = L(p[5]);
            float pt = fbits(p[6]);
            uint64_t eCloudX = std::stoull(p[7]);
            uint64_t eCloudZ = std::stoull(p[8]);
            int32_t eCellX = I(p[9]);
            int32_t eCellZ = I(p[10]);
            uint32_t eXInCell = static_cast<uint32_t>(std::stoul(p[11]));
            uint32_t eZInCell = static_cast<uint32_t>(std::stoul(p[12]));
            cc::CloudCellPos r = cc::computeCloudCellPos(width, height, camX, camZ, gameTime, pt);
            ++checks;
            if (db(r.cloudX) != eCloudX) fail(line + " [cloudX]");
            ++checks;
            if (db(r.cloudZ) != eCloudZ) fail(line + " [cloudZ]");
            ++checks;
            if (r.cellX != eCellX) fail(line + " [cellX]");
            ++checks;
            if (r.cellZ != eCellZ) fail(line + " [cellZ]");
            ++checks;
            if (fb(r.xInCell) != eXInCell) fail(line + " [xInCell]");
            ++checks;
            if (fb(r.zInCell) != eZInCell) fail(line + " [zInCell]");
        }
        // unknown tags ignored (forward-compatible)
    }

    std::cout << "CloudCellMath checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
