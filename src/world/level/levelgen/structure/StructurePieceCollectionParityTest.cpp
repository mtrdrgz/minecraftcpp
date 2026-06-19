// Parity test for the PURE list/bbox aggregation helpers in
//   net.minecraft.world.level.levelgen.structure.StructurePiece (26.1.2)
// and the multi-box fold they delegate to in BoundingBox:
//   StructurePiece.createBoundingBox(Stream)   == StructurePiecesBuilder.getBoundingBox()
//   StructurePiece.findCollisionPiece(List,box) == StructurePiecesBuilder.findCollisionPiece
//   BoundingBox.encapsulatingBoxes(Iterable)
//
// Ground truth: tools/StructurePieceCollectionParity.java drives the REAL classes
// and emits a TSV. We recompute each row from StructurePieceCollection.h and
// compare. All values are decimal ints (pure integer geometry), so the gate is
// exact.
//
//   structure_piece_collection_parity --cases mcpp/build/structure_piece_collection.tsv
//
// The Java Bootstrap may print unrelated lines to stdout; rows whose first tab
// field is not a known tag are skipped (not counted, not failed).

#include "StructurePieceCollection.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::piece;
using mc::levelgen::structure::BoundingBox;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return static_cast<int>(std::stoll(s)); }

// Read a 6-int BoundingBox starting at field `base` (minX minY minZ maxX maxY maxZ).
BoundingBox box6(const std::vector<std::string>& p, std::size_t base) {
    return BoundingBox(toi(p[base]), toi(p[base + 1]), toi(p[base + 2]),
                       toi(p[base + 3]), toi(p[base + 4]), toi(p[base + 5]));
}

bool boxEq(const BoundingBox& b, const std::vector<std::string>& p, std::size_t base) {
    return b.minX == toi(p[base]) && b.minY == toi(p[base + 1]) && b.minZ == toi(p[base + 2]) &&
           b.maxX == toi(p[base + 3]) && b.maxY == toi(p[base + 4]) && b.maxZ == toi(p[base + 5]);
}

std::string boxStr(const BoundingBox& b) {
    return std::to_string(b.minX) + "," + std::to_string(b.minY) + "," + std::to_string(b.minZ) + "," +
           std::to_string(b.maxX) + "," + std::to_string(b.maxY) + "," + std::to_string(b.maxZ);
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: structure_piece_collection_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << tag << " " << detail << "\n";
    };

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = splitTabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CREATEBB") {
            // n <box6>*n | resultBox6
            int n = toi(p[1]);
            std::vector<BoundingBox> boxes;
            std::size_t base = 2;
            for (int i = 0; i < n; ++i) { boxes.push_back(box6(p, base)); base += 6; }
            ++total;
            BoundingBox got = createBoundingBox(boxes);
            if (!boxEq(got, p, base))
                fail(tag, line + " got=" + boxStr(got));
        } else if (tag == "CREATEBB_EMPTY") {
            // | THROW   (createBoundingBox over 0 pieces must throw)
            ++total;
            bool threw = false;
            try {
                createBoundingBox(std::vector<BoundingBox>{});
            } catch (const std::logic_error&) {
                threw = true;
            }
            bool expectThrow = (p.size() >= 2 && p[1] == "THROW");
            if (threw != expectThrow)
                fail(tag, line + " got=" + (threw ? "THROW" : "NOTHROW"));
        } else if (tag == "ENCB") {
            // n <box6>*n | PRESENT box6  | EMPTY
            int n = toi(p[1]);
            std::vector<BoundingBox> boxes;
            std::size_t base = 2;
            for (int i = 0; i < n; ++i) { boxes.push_back(box6(p, base)); base += 6; }
            ++total;
            auto got = encapsulatingBoxes(boxes);
            const std::string& kind = p[base];
            if (kind == "EMPTY") {
                if (got.has_value())
                    fail(tag, line + " got=PRESENT " + boxStr(*got));
            } else { // PRESENT
                if (!got.has_value())
                    fail(tag, line + " got=EMPTY");
                else if (!boxEq(*got, p, base + 1))
                    fail(tag, line + " got=" + boxStr(*got));
            }
        } else if (tag == "FINDCP") {
            // n <box6>*n <query6> | index
            int n = toi(p[1]);
            std::vector<BoundingBox> boxes;
            std::size_t base = 2;
            for (int i = 0; i < n; ++i) { boxes.push_back(box6(p, base)); base += 6; }
            BoundingBox query = box6(p, base); base += 6;
            int expected = toi(p[base]);
            ++total;
            int got = findCollisionPieceIndex(boxes, query);
            if (got != expected)
                fail(tag, line + " got=" + std::to_string(got));
        }
        // else: unknown tag (Bootstrap log noise) -> skip, neither counted nor failed.
    }

    std::cout << "StructurePieceCollection checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
