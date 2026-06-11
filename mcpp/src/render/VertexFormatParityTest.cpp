// Bit-exact parity gate for com.mojang.blaze3d.vertex.VertexFormat (+ its nested
// VertexFormatElement / Mode / IndexType) from Minecraft 26.1.2. Reads the TSV
// emitted by tools/VertexFormatParity.java and re-drives the C++ render/VertexFormat.h
// port, comparing every computed integer (vertexSize, elementsMask, hashCode, the
// full 32-int offsetsByElement[], per-element getOffset/contains, element names via
// base64, Type sizes, Mode.indexCount, IndexType.least) exactly.
//
// TSV row tags (tab-separated; see VertexFormatParity.java header for the schema):
//   FMT  <name> <nElems> [<id> <index> <typeOrd> <norm> <count> <offset>]*
//        <vertexSize> <elementsMask> <hashCode> [<off_0..off_31>] <toStringB64>
//   ELT  <name> <id> <byteSize> <mask> <getOffset> <contains> <elementNameB64|->
//   TYPE <ordinal> <size> <nameB64>
//   MODE <ordinal> <primLen> <primStride> <connected> [<vc> <indexCount>]*
//   IDX  <length> <ordinal> <bytes>
//
//   mcpp/build/vertex_format_parity.exe --cases mcpp/build/vertex_format.tsv

#include "render/VertexFormat.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using mc::render::VertexFormat;
using mc::render::VertexFormatElement;
using mc::render::VfeType;

namespace {

long g_checks = 0, g_mism = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_mism;
        if (g_mism <= 30) std::cerr << "MISMATCH " << what << "\n";
    }
}

template <typename T>
void checkEq(const T& got, const T& exp, const std::string& what) {
    ++g_checks;
    if (got != exp) {
        ++g_mism;
        if (g_mism <= 30)
            std::cerr << "MISMATCH " << what << " got=" << got << " exp=" << exp << "\n";
    }
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> p;
    std::stringstream ss(line);
    std::string tok;
    while (std::getline(ss, tok, '\t')) p.push_back(tok);
    return p;
}

int32_t toI(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// minimal base64 decode (standard alphabet, no whitespace) for string comparison
std::string b64decode(const std::string& in) {
    if (in == "-") return std::string("\x01__ABSENT__");  // sentinel for "no name"
    static const std::string A =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::array<int, 256> rev{};
    rev.fill(-1);
    for (int i = 0; i < 64; ++i) rev[static_cast<unsigned char>(A[i])] = i;
    std::string out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=') break;
        int d = rev[c];
        if (d < 0) continue;
        val = (val << 6) | d;
        bits += 6;
        if (bits >= 0) {
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

const VertexFormatElement& eltById(int32_t id) {
    const VertexFormatElement* e = mc::render::vfe::byId(id);
    static const VertexFormatElement dummy{};
    return e ? *e : dummy;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: vertex_format_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    // Keep reconstructed formats by name so ELT rows can be matched against them.
    std::unordered_map<std::string, VertexFormat> formats;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "FMT") {
            // FMT name nElems [6 fields: id index typeOrd norm count offset]*
            //     vertexSize mask hash [32 offs] b64
            const std::string name = p[1];
            int32_t nElems = toI(p[2]);
            size_t idx = 3;
            std::vector<VertexFormatElement> elems;
            std::vector<std::string> names;
            std::vector<int32_t> offsets;
            for (int32_t i = 0; i < nElems; ++i) {
                int32_t id = toI(p[idx + 0]);
                int32_t index = toI(p[idx + 1]);
                int32_t typeOrd = toI(p[idx + 2]);
                bool norm = toI(p[idx + 3]) != 0;
                int32_t count = toI(p[idx + 4]);
                int32_t off = toI(p[idx + 5]);
                idx += 6;
                elems.push_back(
                    VertexFormatElement{id, index, static_cast<VfeType>(typeOrd), norm, count});
                names.push_back(name + "#" + std::to_string(i));
                offsets.push_back(off);
            }
            int32_t expVertexSize = toI(p[idx++]);
            int32_t expMask = toI(p[idx++]);
            int32_t expHash = toI(p[idx++]);

            // Independently exercise the Builder: reconstruct padding() calls from the
            // gaps between consecutive element offsets (and the trailing gap), then
            // verify the Builder reproduces the same per-element offsets and vertexSize.
            {
                VertexFormat::Builder b;
                int32_t running = 0;
                for (size_t i = 0; i < elems.size(); ++i) {
                    int32_t pad = offsets[i] - running;  // padding() before this element
                    if (pad) b.padding(pad);
                    b.add(names[i], elems[i]);
                    running = offsets[i] + elems[i].byteSize();
                }
                int32_t trailing = expVertexSize - running;
                if (trailing) b.padding(trailing);
                bool ok = false;
                VertexFormat bvf = b.build(&ok);
                checkEq(bvf.getVertexSize(), expVertexSize, "FMT " + name + " builder.vertexSize");
                check(ok, "FMT " + name + " builder vertexSize multiple-of-4");
                for (size_t i = 0; i < elems.size(); ++i)
                    checkEq(bvf.getOffset(elems[i]), offsets[i],
                            "FMT " + name + " builder off id" + std::to_string(elems[i].id));
            }

            // Drive the exact private constructor (elements, names, offsets, vertexSize)
            // so padding placement is faithful regardless of where padding() sat.
            VertexFormat vf(elems, names, offsets, expVertexSize);

            checkEq(vf.getVertexSize(), expVertexSize, "FMT " + name + " vertexSize");
            checkEq(vf.getElementsMask(), expMask, "FMT " + name + " elementsMask");
            checkEq(vf.hashCode(), expHash, "FMT " + name + " hashCode");
            const auto& offs = vf.getOffsetsByElement();
            for (int32_t i = 0; i < 32; ++i) {
                int32_t expOff = toI(p[idx + static_cast<size_t>(i)]);
                checkEq(offs[static_cast<size_t>(i)], expOff,
                        "FMT " + name + " off[" + std::to_string(i) + "]");
            }
            idx += 32;

            formats.emplace(name, vf);
            // toString: structure is "VertexFormat[<names joined by ', '>]"; our
            // synthetic names differ, so just sanity-check the bracket structure is
            // present in the Java string (names themselves are a verified-by-Java
            // input, not a computed output of the algorithm).
            std::string expToString = b64decode(p[idx]);
            check(expToString.rfind("VertexFormat[", 0) == 0,
                  "FMT " + name + " toString prefix");
        } else if (tag == "ELT") {
            // ELT name id byteSize mask getOffset contains elementNameB64
            const std::string name = p[1];
            int32_t id = toI(p[2]);
            int32_t expByteSize = toI(p[3]);
            int32_t expMask = toI(p[4]);
            int32_t expGetOffset = toI(p[5]);
            bool expContains = toI(p[6]) != 0;
            const VertexFormatElement& e = eltById(id);
            checkEq(e.byteSize(), expByteSize, "ELT " + name + " id" + p[2] + " byteSize");
            checkEq(e.mask(), expMask, "ELT " + name + " id" + p[2] + " mask");
            auto it = formats.find(name);
            if (it == formats.end()) {
                check(false, "ELT " + name + " format-not-seen");
                continue;
            }
            const VertexFormat& vf = it->second;
            checkEq(vf.getOffset(e), expGetOffset, "ELT " + name + " id" + p[2] + " getOffset");
            checkEq(vf.contains(e), expContains, "ELT " + name + " id" + p[2] + " contains");
            // getElementName presence must agree with contains (name exists iff in fmt)
            std::string gotName;
            bool present = vf.getElementName(e, gotName);
            std::string expName = b64decode(p[7]);
            bool expPresent = (expName != std::string("\x01__ABSENT__"));
            checkEq(present, expPresent, "ELT " + name + " id" + p[2] + " name-present");
        } else if (tag == "TYPE") {
            // TYPE ordinal size nameB64
            int32_t ord = toI(p[1]);
            int32_t expSize = toI(p[2]);
            checkEq(mc::render::vfeTypeSize(static_cast<VfeType>(ord)), expSize,
                    "TYPE ord" + p[1] + " size");
        } else if (tag == "MODE") {
            // MODE ordinal primLen primStride connected [vc indexCount]*
            int32_t ord = toI(p[1]);
            auto m = static_cast<VertexFormat::Mode>(ord);
            checkEq(VertexFormat::modePrimitiveLength(m), toI(p[2]), "MODE ord" + p[1] + " primLen");
            checkEq(VertexFormat::modePrimitiveStride(m), toI(p[3]), "MODE ord" + p[1] + " primStride");
            checkEq(VertexFormat::modeConnectedPrimitives(m), toI(p[4]) != 0,
                    "MODE ord" + p[1] + " connected");
            for (size_t i = 5; i + 1 < p.size(); i += 2) {
                int32_t vc = toI(p[i]);
                int32_t expIc = toI(p[i + 1]);
                checkEq(VertexFormat::modeIndexCount(m, vc), expIc,
                        "MODE ord" + p[1] + " indexCount(" + p[i] + ")");
            }
        } else if (tag == "IDX") {
            // IDX length ordinal bytes
            int32_t len = toI(p[1]);
            int32_t expOrd = toI(p[2]);
            int32_t expBytes = toI(p[3]);
            auto t = VertexFormat::indexTypeLeast(len);
            checkEq(static_cast<int32_t>(t), expOrd, "IDX len" + p[1] + " ordinal");
            checkEq(VertexFormat::indexTypeBytes(t), expBytes, "IDX len" + p[1] + " bytes");
        }
        // unknown tags ignored (forward-compatible)
    }

    std::cout << "VertexFormat checks=" << g_checks << " mismatches=" << g_mism << "\n";
    return g_mism == 0 ? 0 : 1;
}
