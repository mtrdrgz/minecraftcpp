// Byte-exact parity gate for the C++ 1:1 StructureTemplate .nbt loader + jigsaw
// discovery + getShuffledJigsawBlocks ordering (StructureTemplateLoader.h) vs the
// REAL 26.1.2 classes (StructureTemplateLoaderParity.java).
//
// For each TEMPLATE row: base64-decode the .nbt bytes (the SAME gzip bytes the
// Java tool read from client.jar), parse via NbtReader::readGzip +
// loadStructureTemplate, and compare size + StructureBlockInfo count.
// For each JIGSAW row: reproduce getShuffledJigsawBlocks(offset, rotation, seed)
// with the certified RandomSource and compare the FULL ordered jigsaw list
// bit-for-bit (orderIndex, localXYZ, front, top, name, target, pool, joint,
// placementPriority, selectionPriority).
//
// Prints: StructureTemplateLoader checks=N mismatches=M  (exit nonzero iff M>0).
//
//   structure_template_loader_parity --cases mcpp/build/structure_template_loader.tsv

#include "StructureTemplateLoader.h"
#include "../../../../../nbt/NbtIo.h"
#include "../../../../../nbt/Tag.h"
#include "../../RandomSource.h"
#include "../../../../phys/Direction.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace stl = mc::levelgen::structure::templatesystem;
using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::BlockPos;
using stl::LoadedTemplate;
using stl::JigsawBlockInfo;

namespace {

// ── base64 decode (standard alphabet, '=' padding) ───────────────────────────
std::vector<std::uint8_t> base64Decode(const std::string& in) {
    static int8_t lut[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; ++i) lut[i] = -1;
        const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) lut[(unsigned char)a[i]] = (int8_t)i;
        init = true;
    }
    std::vector<std::uint8_t> out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=' || c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        int8_t d = lut[c];
        if (d < 0) continue;
        val = (val << 6) | d;
        bits += 6;
        if (bits >= 0) {
            out.push_back((std::uint8_t)((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// Direction enum -> serialized name, matching net.minecraft.core.Direction.getName().
std::string directionName(mc::Direction d) {
    switch (d) {
        case mc::Direction::DOWN:  return "down";
        case mc::Direction::UP:    return "up";
        case mc::Direction::NORTH: return "north";
        case mc::Direction::SOUTH: return "south";
        case mc::Direction::WEST:  return "west";
        case mc::Direction::EAST:  return "east";
    }
    return "?";
}

std::vector<std::string> split(const std::string& line, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

struct ExpectedJigsaw {
    int orderIndex = 0;
    int lx = 0, ly = 0, lz = 0;
    std::string front, top, name, target, pool, joint;
    int placePriority = 0, selPriority = 0;
};

struct TemplateData {
    bool loaded = false;
    LoadedTemplate tmpl;
    // expected from TEMPLATE row:
    int sizeX = 0, sizeY = 0, sizeZ = 0, numBlockInfos = 0;
    bool sizeChecked = false;
};

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/structure_template_loader.tsv";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) {
        std::cerr << "cannot open cases file: " << casesPath << "\n";
        std::cout << "StructureTemplateLoader checks=0 mismatches=1\n";
        return 1;
    }

    long long checks = 0;
    long long mismatches = 0;
    int reported = 0;
    auto fail = [&](const std::string& msg) {
        ++mismatches;
        if (reported < 40) { std::cerr << "MISMATCH: " << msg << "\n"; ++reported; }
    };

    std::map<std::string, TemplateData> templates;
    std::vector<std::string> templateOrder;   // template names in file (emission) order

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> cols = split(line, '\t');
        if (cols.empty()) continue;

        if (cols[0] == "TEMPLATE") {
            // TEMPLATE name b64 sizeX sizeY sizeZ numBlockInfos
            if (cols.size() < 7) { fail("TEMPLATE row too short"); continue; }
            const std::string& name = cols[1];
            const std::string& b64 = cols[2];
            TemplateData td;
            td.sizeX = std::stoi(cols[3]);
            td.sizeY = std::stoi(cols[4]);
            td.sizeZ = std::stoi(cols[5]);
            td.numBlockInfos = std::stoi(cols[6]);

            std::vector<std::uint8_t> bytes = base64Decode(b64);
            std::optional<mc::nbt::NbtCompound> root;
            try {
                root = mc::nbt::NbtReader::readGzip(bytes);
            } catch (const std::exception& e) {
                fail(std::string("readGzip threw for ") + name + ": " + e.what());
            }
            ++checks;
            if (!root.has_value()) {
                fail("readGzip returned nullopt for " + name);
            } else {
                try {
                    td.tmpl = stl::loadStructureTemplate(*root);
                    td.loaded = true;
                } catch (const std::exception& e) {
                    fail(std::string("loadStructureTemplate threw for ") + name + ": " + e.what());
                }
            }

            // size + block count checks
            ++checks;
            if (td.loaded) {
                if (td.tmpl.size.x != td.sizeX || td.tmpl.size.y != td.sizeY ||
                    td.tmpl.size.z != td.sizeZ) {
                    fail("size mismatch for " + name + ": got (" +
                         std::to_string(td.tmpl.size.x) + "," + std::to_string(td.tmpl.size.y) +
                         "," + std::to_string(td.tmpl.size.z) + ") expected (" +
                         cols[3] + "," + cols[4] + "," + cols[5] + ")");
                }
            }
            ++checks;
            if (td.loaded) {
                int got = static_cast<int>(td.tmpl.blocks.size());
                if (got != td.numBlockInfos) {
                    fail("blockInfo count mismatch for " + name + ": got " +
                         std::to_string(got) + " expected " + std::to_string(td.numBlockInfos));
                }
            }
            templateOrder.push_back(name);
            templates[name] = std::move(td);
        }
        // JIGSAW rows are validated in the second pass below (they need the
        // recomputed shuffle lists, which depend on the fixed offset battery the
        // Java tool used and which is not carried per-row).
    }

    // ── Second pass: regenerate jigsaw lists and compare ─────────────────────
    // The JIGSAW rows do NOT carry the offset, so we must reproduce the SAME fixed
    // (offset, rotation, seed) battery the Java tool used, generate each ordered
    // list, and match it against the corresponding consecutive run of JIGSAW rows.
    // We re-read the file to collect JIGSAW rows grouped by (template, rot, seed),
    // in file order, then walk the battery in the same order, consuming one group
    // per (rot, seed) and (implicitly) per offset.
    f.clear();
    f.seekg(0, std::ios::beg);

    // Fixed battery — MUST match StructureTemplateLoaderParity.java exactly.
    const int offsets[][3] = {
        {0, 0, 0}, {3, -7, 11}, {-13, 64, -5}, {100, 0, -100}, {-1, -1, -1},
    };
    const Rotation rots[] = {
        Rotation::NONE, Rotation::CLOCKWISE_90, Rotation::CLOCKWISE_180, Rotation::COUNTERCLOCKWISE_90,
    };
    const long long seeds[] = {
        0LL, 1LL, 2LL, 7LL, 42LL, 99LL, 12345LL, -1LL, -42LL, 123456789LL,
        9223372036854775807LL, (long long)(-9223372036854775807LL - 1),
    };

    // Collect JIGSAW rows in file order, grouped consecutively by (template, rot, seed).
    struct JRow { std::string tname; int rotOrd; long long seed; ExpectedJigsaw ex; };
    std::vector<JRow> jrows;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> cols = split(line, '\t');
        if (cols.empty() || cols[0] != "JIGSAW") continue;
        if (cols.size() < 16) continue;
        JRow r;
        r.tname = cols[1];
        r.rotOrd = std::stoi(cols[2]);
        r.seed = std::stoll(cols[3]);
        r.ex.orderIndex = std::stoi(cols[4]);
        r.ex.lx = std::stoi(cols[5]);
        r.ex.ly = std::stoi(cols[6]);
        r.ex.lz = std::stoi(cols[7]);
        r.ex.front = cols[8];
        r.ex.top = cols[9];
        r.ex.name = cols[10];
        r.ex.target = cols[11];
        r.ex.pool = cols[12];
        r.ex.joint = cols[13];
        r.ex.placePriority = std::stoi(cols[14]);
        r.ex.selPriority = std::stoi(cols[15]);
        jrows.push_back(std::move(r));
    }

    // Walk the battery in the exact (template-outer, offset, rotation, seed) order
    // the Java tool emitted: per template it iterates offsets x rots x seeds, and
    // for each it emits the ordered jigsaw rows. We consume jrows positionally.
    std::size_t cursor = 0;
    for (const std::string& tname : templateOrder) {
        TemplateData& td = templates[tname];
        if (!td.loaded) continue;
        for (const auto& off : offsets) {
            BlockPos position{off[0], off[1], off[2]};
            for (Rotation rot : rots) {
                for (long long seed : seeds) {
                    auto random = mc::levelgen::RandomSource::create((int64_t)seed);
                    std::vector<JigsawBlockInfo> got =
                        stl::getShuffledJigsawBlocks(td.tmpl, position, rot, *random);

                    int rotOrd = static_cast<int>(rot);
                    // Compare the run of jrows for (tname, rotOrd, seed) consecutively.
                    // First, count the expected run length (consecutive jrows that match
                    // this group from `cursor`).
                    std::size_t runStart = cursor;
                    while (cursor < jrows.size() && jrows[cursor].tname == tname &&
                           jrows[cursor].rotOrd == rotOrd && jrows[cursor].seed == seed) {
                        ++cursor;
                    }
                    std::size_t runLen = cursor - runStart;

                    ++checks;
                    if (runLen != got.size()) {
                        fail("jigsaw count mismatch for " + tname + " rot=" +
                             std::to_string(rotOrd) + " seed=" + std::to_string(seed) +
                             ": got " + std::to_string(got.size()) + " expected " +
                             std::to_string(runLen));
                        continue;
                    }

                    for (std::size_t k = 0; k < got.size(); ++k) {
                        const JigsawBlockInfo& g = got[k];
                        const ExpectedJigsaw& e = jrows[runStart + k].ex;
                        ++checks;
                        std::string gFront = directionName(g.info.orientation.front);
                        std::string gTop = directionName(g.info.orientation.top);
                        std::string gJoint = stl::jointTypeName(g.jointType);
                        bool ok =
                            (int)k == e.orderIndex &&
                            g.info.pos.x == e.lx && g.info.pos.y == e.ly && g.info.pos.z == e.lz &&
                            gFront == e.front && gTop == e.top &&
                            g.name == e.name && g.target == e.target && g.pool == e.pool &&
                            gJoint == e.joint &&
                            g.placementPriority == e.placePriority &&
                            g.selectionPriority == e.selPriority;
                        if (!ok) {
                            std::ostringstream os;
                            os << "jigsaw mismatch " << tname << " rot=" << rotOrd
                               << " seed=" << seed << " idx=" << k
                               << " got{pos=(" << g.info.pos.x << "," << g.info.pos.y << "," << g.info.pos.z
                               << ") front=" << gFront << " top=" << gTop
                               << " name=" << g.name << " target=" << g.target
                               << " pool=" << g.pool << " joint=" << gJoint
                               << " place=" << g.placementPriority << " sel=" << g.selectionPriority
                               << "} exp{pos=(" << e.lx << "," << e.ly << "," << e.lz
                               << ") front=" << e.front << " top=" << e.top
                               << " name=" << e.name << " target=" << e.target
                               << " pool=" << e.pool << " joint=" << e.joint
                               << " place=" << e.placePriority << " sel=" << e.selPriority << "}";
                            fail(os.str());
                        }
                    }
                }
            }
        }
    }

    if (cursor != jrows.size()) {
        fail("consumed " + std::to_string(cursor) + " of " + std::to_string(jrows.size()) +
             " JIGSAW rows (battery/order desync)");
    }

    std::cout << "StructureTemplateLoader checks=" << checks
              << " mismatches=" << mismatches << "\n";
    return mismatches > 0 ? 1 : 0;
}
