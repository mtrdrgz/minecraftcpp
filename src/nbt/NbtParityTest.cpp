// NBT tag-codec parity vs the real net.minecraft.nbt (NbtParity.java ground truth).
//
// For each case in mcpp/build/nbt_cases.tsv (CANON/REWRITE lines) the raw
// uncompressed payload mcpp/build/nbt_cases/<name>.nbt is read with the C++
// NbtReader; the canonical dump (spec in NbtParity.java) must equal CANON, and the
// NbtWriter::writeRootCompound bytes must equal REWRITE hex byte-for-byte. CANON
// proves read equivalence (incl. MODIFIED UTF-8 decode); REWRITE proves write
// equivalence (insertion order + MUTF-8 encode + framing).
//
//   nbt_parity [--cases mcpp/build/nbt_cases.tsv]
#include "NbtIo.h"
#include "Tag.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::nbt;

namespace {

std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
std::string hexStr(const std::string& v) {
    return hex(std::vector<uint8_t>(v.begin(), v.end()));
}

void canon(const NbtTag& tag, std::string& sb);

void canonCompound(const NbtCompound& c, std::string& sb) {
    // keys sorted by UTF-8 byte order (the canon spec; storage order is insertion)
    std::vector<const std::pair<std::string, NbtTag>*> sorted;
    sorted.reserve(c.entries.size());
    for (auto& e : c.entries) sorted.push_back(&e);
    std::sort(sorted.begin(), sorted.end(), [](auto* a, auto* b) {
        return std::lexicographical_compare(
            (const uint8_t*)a->first.data(), (const uint8_t*)a->first.data() + a->first.size(),
            (const uint8_t*)b->first.data(), (const uint8_t*)b->first.data() + b->first.size());
    });
    sb += '{';
    bool first = true;
    for (auto* e : sorted) {
        if (!first) sb += ',';
        first = false;
        sb += "s\""; sb += hexStr(e->first); sb += "\"=";
        canon(e->second, sb);
    }
    sb += '}';
}

void canon(const NbtTag& tag, std::string& sb) {
    char buf[40];
    switch (tag.type()) {
    case TagType::End:    sb += 'e'; break;
    case TagType::Byte:   snprintf(buf, sizeof buf, "b%d", (int)*tag.as<int8_t>()); sb += buf; break;
    case TagType::Short:  snprintf(buf, sizeof buf, "s%d", (int)*tag.as<int16_t>()); sb += buf; break;
    case TagType::Int:    snprintf(buf, sizeof buf, "i%d", *tag.as<int32_t>()); sb += buf; break;
    case TagType::Long:   snprintf(buf, sizeof buf, "l%lld", (long long)*tag.as<int64_t>()); sb += buf; break;
    case TagType::Float: {
        uint32_t bits; float f = *tag.as<float>(); memcpy(&bits, &f, 4);
        snprintf(buf, sizeof buf, "f%08x", bits); sb += buf; break;
    }
    case TagType::Double: {
        uint64_t bits; double d = *tag.as<double>(); memcpy(&bits, &d, 8);
        snprintf(buf, sizeof buf, "d%016llx", (unsigned long long)bits); sb += buf; break;
    }
    case TagType::String: sb += "s\""; sb += hexStr(*tag.as<std::string>()); sb += '"'; break;
    case TagType::ByteArray: {
        sb += "B[";
        for (int8_t b : *tag.as<NbtByteArray>()) { snprintf(buf, sizeof buf, "%02x", (uint8_t)b); sb += buf; }
        sb += ']'; break;
    }
    case TagType::IntArray: {
        sb += "I[";
        for (int32_t v : *tag.as<NbtIntArray>()) { snprintf(buf, sizeof buf, "%08x", (uint32_t)v); sb += buf; }
        sb += ']'; break;
    }
    case TagType::LongArray: {
        sb += "L[";
        for (int64_t v : *tag.as<NbtLongArray>()) { snprintf(buf, sizeof buf, "%016llx", (unsigned long long)v); sb += buf; }
        sb += ']'; break;
    }
    case TagType::List: {
        sb += '[';
        const NbtList& l = **tag.as<std::shared_ptr<NbtList>>();
        for (size_t i = 0; i < l.elements.size(); ++i) {
            if (i) sb += ',';
            canon(l.elements[i], sb);
        }
        sb += ']'; break;
    }
    case TagType::Compound:
        canonCompound(**tag.as<std::shared_ptr<NbtCompound>>(), sb);
        break;
    }
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/nbt_cases.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];
    std::string casesDir = casesPath.substr(0, casesPath.find_last_of("/\\") + 1) + "nbt_cases/";

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, failures = 0;
    std::string line;
    std::map<std::string, std::pair<std::string, std::string>> expect; // name -> {canon, rewriteHex}
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string kind, name, payload;
        if (!std::getline(ss, kind, '\t') || !std::getline(ss, name, '\t') || !std::getline(ss, payload)) continue;
        if (kind == "CANON") expect[name].first = payload;
        else if (kind == "REWRITE") expect[name].second = payload;
    }

    for (auto& [name, exp] : expect) {
        ++cases;
        std::ifstream pf(casesDir + name + ".nbt", std::ios::binary);
        if (!pf) { std::cerr << "MISSING payload " << name << "\n"; ++failures; continue; }
        std::vector<uint8_t> payload((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());

        NbtReader r(payload);
        auto root = r.readRootCompound();
        if (!root) { std::cerr << "READ-FAIL " << name << "\n"; ++failures; continue; }

        std::string got;
        canonCompound(*root, got);
        if (got != exp.first) {
            ++failures;
            // print the first differing offset for diagnosis
            size_t i = 0, n = std::min(got.size(), exp.first.size());
            while (i < n && got[i] == exp.first[i]) ++i;
            std::cerr << "CANON-MISMATCH " << name << " at char " << i
                      << "\n  got…  " << got.substr(i, 60)
                      << "\n  want… " << exp.first.substr(i, 60) << "\n";
        }

        std::string rew = hex(NbtWriter::writeRootCompound("", *root));
        if (rew != exp.second) {
            ++failures;
            size_t i = 0, n = std::min(rew.size(), exp.second.size());
            while (i < n && rew[i] == exp.second[i]) ++i;
            std::cerr << "REWRITE-MISMATCH " << name << " at hex char " << i
                      << " (got " << rew.size() / 2 << " bytes, want " << exp.second.size() / 2 << ")\n";
        }
    }

    std::cout << "NbtParity cases=" << cases << " failures=" << failures << "\n";
    return failures == 0 ? 0 : 1;
}
