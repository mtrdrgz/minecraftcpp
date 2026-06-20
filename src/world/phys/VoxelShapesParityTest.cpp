// Voxel-shape + collision parity vs the real net.minecraft.world.phys.shapes.*
// (tools/VoxelShapesParity.java). The Java tool emits a construction program
// (DEF lines) plus expected observations; this harness replays the exact same
// program through the C++ port and compares every observation BIT-exactly
// (raw IEEE-754 bit hex for all doubles).
//
//   voxel_shapes_parity [--cases mcpp/build/voxel_shapes_cases.tsv]
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "world/phys/AABB.h"
#include "world/phys/BlockHitResult.h"
#include "world/phys/Direction.h"
#include "world/phys/shapes/BooleanOp.h"
#include "world/phys/shapes/CollisionContext.h"
#include "world/phys/shapes/Shapes.h"
#include "world/phys/shapes/VoxelShape.h"

using namespace mc;

namespace {

double fromBits(const std::string& hexTok) {
    uint64_t bits = std::stoull(hexTok, nullptr, 16);
    double d;
    std::memcpy(&d, &bits, 8);
    return d;
}

std::string toBits(double d) {
    uint64_t bits;
    std::memcpy(&bits, &d, 8);
    char buf[20];
    snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(bits));
    return buf;
}

BooleanOp opByName(const std::string& name) {
    if (name == "OR") return BooleanOps::OR;
    if (name == "AND") return BooleanOps::AND;
    if (name == "ONLY_FIRST") return BooleanOps::ONLY_FIRST;
    if (name == "ONLY_SECOND") return BooleanOps::ONLY_SECOND;
    if (name == "NOT_SAME") return BooleanOps::NOT_SAME;
    throw std::invalid_argument("unknown op " + name);
}

struct Harness {
    std::map<std::string, VoxelShapePtr> shapes;
    long checks = 0;
    long defs = 0;
    long failures = 0;
    std::map<std::string, long> failuresByKind;

    void fail(const std::string& kind, const std::string& detail) {
        ++failures;
        ++failuresByKind[kind];
        if (failures <= 50) std::cerr << kind << "-MISMATCH " << detail << "\n";
    }

    const VoxelShapePtr& shape(const std::string& name) {
        auto it = shapes.find(name);
        if (it == shapes.end()) throw std::runtime_error("unknown shape " + name);
        return it->second;
    }

    void handle(const std::vector<std::string>& t) {
        const std::string& kind = t[0];
        if (kind == "DEF") {
            ++defs;
            const std::string& name = t[1];
            const std::string& recipe = t[2];
            if (recipe == "EMPTY") {
                shapes[name] = Shapes::empty();
            } else if (recipe == "BLOCK") {
                shapes[name] = Shapes::block();
            } else if (recipe == "BOX" || recipe == "CREATE") {
                double v[6];
                for (int i = 0; i < 6; ++i) v[i] = fromBits(t[3 + i]);
                shapes[name] = recipe == "BOX"
                                   ? Shapes::box(v[0], v[1], v[2], v[3], v[4], v[5])
                                   : Shapes::create(v[0], v[1], v[2], v[3], v[4], v[5]);
            } else if (recipe == "JOIN" || recipe == "JOINU") {
                BooleanOp op = opByName(t[5]);
                shapes[name] = recipe == "JOIN"
                                   ? Shapes::join(shape(t[3]), shape(t[4]), op)
                                   : Shapes::joinUnoptimized(shape(t[3]), shape(t[4]), op);
            } else if (recipe == "MOVE") {
                shapes[name] =
                    shape(t[3])->move(fromBits(t[4]), fromBits(t[5]), fromBits(t[6]));
            } else if (recipe == "OPT") {
                shapes[name] = shape(t[3])->optimize();
            } else if (recipe == "FACE") {
                shapes[name] = shape(t[3])->getFaceShape(static_cast<Direction>(std::stoi(t[4])));
            } else {
                throw std::runtime_error("unknown recipe " + recipe);
            }
        } else if (kind == "EMPTYQ") {
            ++checks;
            bool expected = t[2] == "1";
            if (shape(t[1])->isEmpty() != expected) fail("EMPTYQ", t[1]);
        } else if (kind == "COORDS") {
            ++checks;
            const VoxelShapePtr& s = shape(t[1]);
            size_t idx = 2;
            bool ok = true;
            for (Axis axis : AXIS_VALUES) {
                DoubleListPtr coords = s->getCoords(axis);
                int32_t n = std::stoi(t[idx++]);
                if (coords->size() != n) {
                    ok = false;
                    idx += n;
                    continue;
                }
                for (int32_t i = 0; i < n; ++i)
                    if (toBits(coords->getDouble(i)) != t[idx++]) ok = false;
            }
            if (!ok) fail("COORDS", t[1]);
        } else if (kind == "BOUNDS") {
            ++checks;
            AABB b = shape(t[1])->bounds();
            double got[6] = {b.minCorner.x, b.minCorner.y, b.minCorner.z,
                             b.maxCorner.x, b.maxCorner.y, b.maxCorner.z};
            for (int i = 0; i < 6; ++i)
                if (toBits(got[i]) != t[2 + i]) {
                    fail("BOUNDS", t[1]);
                    break;
                }
        } else if (kind == "AABBS") {
            ++checks;
            std::vector<AABB> boxes = shape(t[1])->toAabbs();
            int32_t n = std::stoi(t[2]);
            if (static_cast<int32_t>(boxes.size()) != n) {
                fail("AABBS", t[1] + " count got=" + std::to_string(boxes.size())
                                  + " want=" + std::to_string(n));
            } else {
                bool ok = true;
                size_t idx = 3;
                for (const AABB& b : boxes) {
                    double got[6] = {b.minCorner.x, b.minCorner.y, b.minCorner.z,
                                     b.maxCorner.x, b.maxCorner.y, b.maxCorner.z};
                    for (int i = 0; i < 6; ++i)
                        if (toBits(got[i]) != t[idx++]) ok = false;
                }
                if (!ok) fail("AABBS", t[1]);
            }
        } else if (kind == "CLIP") {
            ++checks;
            glm::dvec3 from(fromBits(t[2]), fromBits(t[3]), fromBits(t[4]));
            glm::dvec3 to(fromBits(t[5]), fromBits(t[6]), fromBits(t[7]));
            BlockPos pos{std::stoi(t[8]), std::stoi(t[9]), std::stoi(t[10])};
            bool expHit = t[11] == "1";
            std::optional<BlockHitResult> hit = shape(t[1])->clip(from, to, pos);
            if (hit.has_value() != expHit) {
                fail("CLIP", t[1] + " hit got=" + std::to_string(hit.has_value())
                                  + " want=" + t[11]);
            } else if (hit) {
                bool ok = toBits(hit->location.x) == t[12] && toBits(hit->location.y) == t[13]
                       && toBits(hit->location.z) == t[14]
                       && std::to_string(static_cast<int>(hit->direction)) == t[15]
                       && (hit->inside ? "1" : "0") == t[16];
                if (!ok) fail("CLIP", t[1]);
            }
        } else if (kind == "COLLIDE") {
            ++checks;
            Axis axis = static_cast<Axis>(std::stoi(t[2]));
            AABB moving(fromBits(t[3]), fromBits(t[4]), fromBits(t[5]), fromBits(t[6]),
                        fromBits(t[7]), fromBits(t[8]));
            double dist = fromBits(t[9]);
            double res = shape(t[1])->collide(axis, moving, dist);
            if (toBits(res) != t[10])
                fail("COLLIDE", t[1] + " got=" + toBits(res) + " want=" + t[10]);
        } else if (kind == "COLLIDEN") {
            ++checks;
            Axis axis = static_cast<Axis>(std::stoi(t[1]));
            AABB moving(fromBits(t[2]), fromBits(t[3]), fromBits(t[4]), fromBits(t[5]),
                        fromBits(t[6]), fromBits(t[7]));
            double dist = fromBits(t[8]);
            int32_t k = std::stoi(t[9]);
            std::vector<VoxelShapePtr> list;
            for (int32_t i = 0; i < k; ++i) list.push_back(shape(t[10 + i]));
            double res = Shapes::collide(axis, moving, list, dist);
            if (toBits(res) != t[10 + k])
                fail("COLLIDEN", "got=" + toBits(res) + " want=" + t[10 + k]);
        } else if (kind == "JOINNE") {
            ++checks;
            bool res = Shapes::joinIsNotEmpty(shape(t[1]), shape(t[2]), opByName(t[3]));
            if ((res ? "1" : "0") != t[4]) fail("JOINNE", t[1] + " " + t[2] + " " + t[3]);
        } else if (kind == "OCCB") {
            ++checks;
            bool res = Shapes::blockOccludes(shape(t[1]), shape(t[2]),
                                             static_cast<Direction>(std::stoi(t[3])));
            if ((res ? "1" : "0") != t[4]) fail("OCCB", t[1] + " " + t[2] + " " + t[3]);
        } else if (kind == "OCCM") {
            ++checks;
            bool res = Shapes::mergedFaceOccludes(shape(t[1]), shape(t[2]),
                                                  static_cast<Direction>(std::stoi(t[3])));
            if ((res ? "1" : "0") != t[4]) fail("OCCM", t[1] + " " + t[2] + " " + t[3]);
        } else if (kind == "OCCF") {
            ++checks;
            bool res = Shapes::faceShapeOccludes(shape(t[1]), shape(t[2]));
            if ((res ? "1" : "0") != t[3]) fail("OCCF", t[1] + " " + t[2]);
        } else if (kind == "EQUALQ") {
            ++checks;
            bool res = Shapes::equal(shape(t[1]), shape(t[2]));
            if ((res ? "1" : "0") != t[3]) fail("EQUALQ", t[1] + " " + t[2]);
        } else if (kind == "MINMAX") {
            ++checks;
            const VoxelShapePtr& s = shape(t[1]);
            Axis axis = static_cast<Axis>(std::stoi(t[2]));
            double b = fromBits(t[3]);
            double c = fromBits(t[4]);
            if (toBits(s->min(axis, b, c)) != t[5] || toBits(s->max(axis, b, c)) != t[6])
                fail("MINMAX", t[1]);
        } else if (kind == "CLOSEST") {
            ++checks;
            glm::dvec3 point(fromBits(t[2]), fromBits(t[3]), fromBits(t[4]));
            std::optional<glm::dvec3> res = shape(t[1])->closestPointTo(point);
            bool expPresent = t[5] == "1";
            if (res.has_value() != expPresent) {
                fail("CLOSEST", t[1] + " presence");
            } else if (res) {
                if (toBits(res->x) != t[6] || toBits(res->y) != t[7] || toBits(res->z) != t[8])
                    fail("CLOSEST", t[1]);
            }
        } else {
            throw std::runtime_error("unknown line kind " + kind);
        }
    }
};

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/voxel_shapes_cases.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];
    std::ifstream f(casesPath, std::ios::binary);
    if (!f) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    // Compile-time touch of the CollisionContext surface (no Java observations
    // to replay yet — it has no behavior beyond constants until entities land).
    (void)CollisionContext::empty().alwaysCollideWithFluid();
    (void)CollisionContext::emptyWithFluidCollisions().alwaysCollideWithFluid();

    Harness h;
    std::string line;
    long lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::vector<std::string> tokens;
        std::string tok;
        while (ss >> tok) tokens.push_back(tok);
        if (tokens.empty()) continue;
        try {
            h.handle(tokens);
        } catch (const std::exception& e) {
            std::cerr << "EXCEPTION line " << lineNo << ": " << e.what() << "\n";
            ++h.failures;
        }
    }

    std::cout << "VoxelShapesParity defs=" << h.defs << " checks=" << h.checks
              << " failures=" << h.failures << "\n";
    for (const auto& [kind, count] : h.failuresByKind)
        std::cout << "  " << kind << " failures=" << count << "\n";
    return h.failures == 0 ? 0 : 1;
}
