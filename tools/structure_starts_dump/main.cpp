// structure_starts_dump — headless dumper of structure starts (piece id + bounding
// box + orientation + genDepth) for the C++ engine, in the SAME TSV format as
// tools/StructureStartsDump.java so the two outputs can be byte-diffed.
//
// This is the structure-starts parity gate: it verifies that for a given seed,
// the C++ engine assembles the SAME structures at the SAME chunks with the SAME
// piece list as the real Minecraft server.
//
// Two modes:
//   1. --mirror <server.tsv> : read the server's structure starts and dump the
//      C++ engine's pieces for the same (structure_id, chunk_x, chunk_z) tuples.
//      This is the direct comparison mode — diff the two TSVs.
//   2. --scan --from X Z --to X Z : scan a region, find structure chunks via
//      StructureState, and dump pieces. Useful for finding mismatches in WHICH
//      chunks spawn structures (biome gate differences).
//
// Output format (identical to StructureStartsDump.java):
//   S\t<structureId>\t<chunkX>\t<chunkZ>\t<references>\t<childCount>
//   C\t<pieceId>\t<minX>\t<minY>\t<minZ>\t<maxX>\t<maxY>\t<maxZ>\t<O>\t<GD>
//
// Build (parity-only build, Linux GCC):
//   cmake --build build-linux --target structure_starts_dump
// Run (from repo root):
//   ./build-linux/structure_starts_dump --seed 1 --mirror server_structure_starts.tsv

#include "world/level/levelgen/structure/StructureGen.h"
#include "world/level/levelgen/structure/placement/StructurePlacement.h"
#include "world/level/levelgen/NoiseBasedChunkGenerator.h"
#include "world/level/block/Blocks.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace mc;
using namespace mc::levelgen::structure;

namespace {

std::string argValue(int argc, char** argv, const std::string& flag, const std::string& def) {
    for (int i = 1; i + 1 < argc; ++i)
        if (flag == argv[i]) return argv[i + 1];
    return def;
}

// Parse the server TSV and extract (structureId, chunkX, chunkZ) tuples.
struct ServerStart {
    std::string structureId;
    int chunkX, chunkZ;
    int childCount;
};

std::vector<ServerStart> parseServerTsv(const std::string& path) {
    std::vector<ServerStart> out;
    std::ifstream in(path);
    if (!in) { std::cerr << "ERROR: cannot open " << path << "\n"; return out; }
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] != 'S') continue;
        std::istringstream ss(line);
        std::string tag, id, cx, cz, refs, cc;
        std::getline(ss, tag, '\t');
        std::getline(ss, id, '\t');
        std::getline(ss, cx, '\t');
        std::getline(ss, cz, '\t');
        std::getline(ss, refs, '\t');
        std::getline(ss, cc, '\t');
        if (id.empty()) continue;
        out.push_back({id, std::atoi(cx.c_str()), std::atoi(cz.c_str()), std::atoi(cc.c_str())});
    }
    return out;
}

void dumpStarts(std::ostream& out, const std::vector<DumpStart>& starts) {
    for (const auto& s : starts) {
        if (s.pieces.empty()) continue;  // skip empty starts (assembly not available)
        out << "S\t" << s.structureId << "\t" << s.chunkX << "\t" << s.chunkZ
            << "\t0\t" << s.pieces.size() << "\n";
        for (const auto& p : s.pieces) {
            out << "C\t" << p.id
                << "\t" << p.minX << "\t" << p.minY << "\t" << p.minZ
                << "\t" << p.maxX << "\t" << p.maxY << "\t" << p.maxZ
                << "\t" << p.orientation
                << "\t" << p.genDepth << "\n";
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    initBlocks();

    const std::string data    = argValue(argc, argv, "--data", "26.1.2/data/minecraft");
    const int64_t seed        = std::strtoll(argValue(argc, argv, "--seed", "1").c_str(), nullptr, 10);
    const std::string mirror  = argValue(argc, argv, "--mirror", "");
    const std::string outFile = argValue(argc, argv, "--out", "");

    int fx = -20, fz = -20, tx = 90, tz = 30;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--from" && i + 2 < argc) { fx = std::atoi(argv[++i]); fz = std::atoi(argv[++i]); }
        else if (a == "--to" && i + 2 < argc) { tx = std::atoi(argv[++i]); tz = std::atoi(argv[++i]); }
    }

    std::cerr << "structure_starts_dump: data=" << data << " seed=" << seed;
    if (!mirror.empty()) std::cerr << " mirror=" << mirror;
    else std::cerr << " region=(" << fx << "," << fz << ")-(" << tx << "," << tz << ")";
    std::cerr << "\n";

    std::ofstream outf;
    if (!outFile.empty()) { outf.open(outFile); }
    std::ostream& out = outFile.empty() ? std::cout : outf;

    // Use the REAL biome source (NoiseBasedChunkGenerator) so the biome gate
    // matches the server exactly. This is critical for structures that only
    // spawn in specific biomes (e.g. swamp_hut requires swamp).
    mc::levelgen::NoiseBasedChunkGenerator gen(static_cast<uint64_t>(seed));
    auto biomeGetter = [&gen](int x, int y, int z) -> std::string {
        return gen.getBiome(x, y, z);
    };
    // OCEAN_FLOOR_WG heightmap for structures that project to the ocean floor
    // (buried_treasure, shipwreck, ocean_ruin). This is the key missing piece
    // for the Y adjustment that caused the ocean_ruin mismatch.
    auto heightAt = [&gen](int x, int z) -> int {
        return gen.getOceanFloorHeight(x, z) - 1;  // heightAt returns surface-1
    };

    int startsDumped = 0;
    int piecesDumped = 0;
    std::map<std::string, int> perType;

    if (!mirror.empty()) {
        // Mirror mode: dump C++ pieces for the same (structure_id, chunk) as the server.
        // Iterate chunks in the EXACT ORDER they appear in the server TSV so the
        // output is byte-diffable without sorting.
        auto serverStarts = parseServerTsv(mirror);
        std::cerr << "Parsed " << serverStarts.size() << " structure starts from server TSV\n";

        // Collect unique chunks in server-encounter order (not sorted).
        std::vector<std::pair<int,int>> chunks;
        std::set<std::pair<int,int>> seen;
        for (const auto& s : serverStarts) {
            if (seen.insert({s.chunkX, s.chunkZ}).second) {
                chunks.push_back({s.chunkX, s.chunkZ});
            }
        }

        for (const auto& [cx, cz] : chunks) {
            auto starts = dumpStructureStarts({cx, cz}, static_cast<uint64_t>(seed), biomeGetter, heightAt, data);
            // Filter to only structures the server reported at this chunk
            std::set<std::string> serverIdsAtChunk;
            for (const auto& s : serverStarts)
                if (s.chunkX == cx && s.chunkZ == cz) serverIdsAtChunk.insert(s.structureId);
            std::vector<DumpStart> filtered;
            for (auto& s : starts) {
                if (serverIdsAtChunk.count(s.structureId)) {
                    perType[s.structureId]++;
                    startsDumped++;
                    piecesDumped += static_cast<int>(s.pieces.size());
                    filtered.push_back(std::move(s));
                }
            }
            dumpStarts(out, filtered);
        }
    } else {
        // Scan mode: find structure chunks and dump.
        for (int cz = fz; cz <= tz; ++cz) {
            for (int cx = fx; cx <= tx; ++cx) {
                auto starts = dumpStructureStarts({cx, cz}, static_cast<uint64_t>(seed), biomeGetter, heightAt, data);
                for (const auto& s : starts) {
                    perType[s.structureId]++;
                    startsDumped++;
                    piecesDumped += static_cast<int>(s.pieces.size());
                }
                dumpStarts(out, starts);
            }
        }
    }

    std::cerr << "=== SUMMARY: dumped " << startsDumped << " structure starts, "
              << piecesDumped << " pieces total\n";
    for (const auto& [id, n] : perType) {
        std::cerr << "  " << id << ": " << n << " starts\n";
    }
    return startsDumped > 0 ? 0 : 1;
}
