// Parity gate for StructurePiecesBuilderMath.h vs the REAL
//   net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder
// vertical-placement math (offsetPiecesVertically / moveBelowSeaLevel / moveInsideHeights).
//
//   build:  add_executable(structure_pieces_builder_math_parity ...) — header-only test.
//   run:    structure_pieces_builder_math_parity.exe --cases mcpp/build/structure_pieces_builder_math.tsv
//
// TSV (whitespace tokens), dispatched by leading TAG (see StructurePiecesBuilderMathParity.java):
//   OFFSET   <nb> <6*nb input> <dy> <6*nb result>
//   BELOWSEA <nb> <6*nb input> <seaLevel> <minY> <offset> <ndraw> [draw] <retDy> <6*nb result>
//   INSIDE   <nb> <6*nb input> <lowest> <highest> <ndraw> [draw] <dy> <6*nb result>

#include "StructurePiecesBuilderMath.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::pieces::ScriptedNextIntSource;
using mc::levelgen::structure::pieces::StructurePiecesBuilderMath;

namespace {

std::vector<BoundingBox> readBoxes(const std::vector<std::string>& tok, size_t& i, int nb) {
    std::vector<BoundingBox> out;
    out.reserve(nb);
    for (int k = 0; k < nb; ++k) {
        int32_t x0 = std::stoi(tok[i++]);
        int32_t y0 = std::stoi(tok[i++]);
        int32_t z0 = std::stoi(tok[i++]);
        int32_t x1 = std::stoi(tok[i++]);
        int32_t y1 = std::stoi(tok[i++]);
        int32_t z1 = std::stoi(tok[i++]);
        out.emplace_back(x0, y0, z0, x1, y1, z1);
    }
    return out;
}

bool boxesEqual(const std::vector<BoundingBox>& a, const std::vector<BoundingBox>& b) {
    if (a.size() != b.size()) return false;
    for (size_t k = 0; k < a.size(); ++k) {
        const BoundingBox& x = a[k];
        const BoundingBox& y = b[k];
        if (x.minX != y.minX || x.minY != y.minY || x.minZ != y.minZ ||
            x.maxX != y.maxX || x.maxY != y.maxY || x.maxZ != y.maxZ)
            return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const char* casesPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (!casesPath) {
        std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", casesPath);
        return 2;
    }

    long checks = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> tok;
        {
            std::istringstream ss(line);
            std::string t;
            while (ss >> t) tok.push_back(t);
        }
        if (tok.empty()) continue;

        const std::string& tag = tok[0];
        size_t i = 1;
        int nb = std::stoi(tok[i++]);
        std::vector<BoundingBox> input = readBoxes(tok, i, nb);

        bool ok = false;
        try {
            if (tag == "OFFSET") {
                int32_t dy = std::stoi(tok[i++]);
                std::vector<BoundingBox> expect = readBoxes(tok, i, nb);
                StructurePiecesBuilderMath b(input);
                b.offsetPiecesVertically(dy);
                ok = boxesEqual(b.boxes(), expect);
            } else if (tag == "BELOWSEA") {
                int32_t seaLevel = std::stoi(tok[i++]);
                int32_t minY = std::stoi(tok[i++]);
                int32_t offset = std::stoi(tok[i++]);
                int ndraw = std::stoi(tok[i++]);
                std::vector<int32_t> draws;
                for (int k = 0; k < ndraw; ++k) draws.push_back(std::stoi(tok[i++]));
                int32_t retDy = std::stoi(tok[i++]);
                std::vector<BoundingBox> expect = readBoxes(tok, i, nb);
                StructurePiecesBuilderMath b(input);
                ScriptedNextIntSource src(draws);
                int32_t dy = b.moveBelowSeaLevel(seaLevel, minY, src, offset);
                ok = (dy == retDy) && boxesEqual(b.boxes(), expect) &&
                     (static_cast<int>(src.consumed()) == ndraw);
            } else if (tag == "INSIDE") {
                int32_t lowest = std::stoi(tok[i++]);
                int32_t highest = std::stoi(tok[i++]);
                int ndraw = std::stoi(tok[i++]);
                std::vector<int32_t> draws;
                for (int k = 0; k < ndraw; ++k) draws.push_back(std::stoi(tok[i++]));
                int32_t expectDy = std::stoi(tok[i++]);
                std::vector<BoundingBox> expect = readBoxes(tok, i, nb);
                StructurePiecesBuilderMath b(input);
                ScriptedNextIntSource src(draws);
                int32_t dy = b.moveInsideHeights(src, lowest, highest);
                ok = (dy == expectDy) && boxesEqual(b.boxes(), expect) &&
                     (static_cast<int>(src.consumed()) == ndraw);
            } else {
                continue;  // unknown tag
            }
        } catch (...) {
            ok = false;  // out-of-range draw replay etc. => divergence
        }

        ++checks;
        if (!ok) {
            ++mismatches;
            if (mismatches <= 10)
                std::fprintf(stderr, "MISMATCH: %s\n", line.c_str());
        }
    }

    std::printf("StructurePiecesBuilderMath checks=%ld mismatches=%ld\n", checks, mismatches);
    return mismatches > 0 ? 1 : 0;
}
