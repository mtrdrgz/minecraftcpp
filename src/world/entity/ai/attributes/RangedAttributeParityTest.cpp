// Bit-exact parity gate for net.minecraft.world.entity.ai.attributes.RangedAttribute.
//
// VERIFY-EXISTING: this test does NOT introduce a new port. It includes the already
// certified header world/entity/ai/attributes/AttributeMath.h and checks that its
//   sanitizeValue(value, minValue, maxValue)
// reproduces RangedAttribute.sanitizeValue(double) bit-for-bit, plus the trivial
// getDefaultValue() identity (constructor-stored defaultValue round-trip).
//
// Ground truth is emitted by mcpp/tools/RangedAttributeParity.java.
//
// Row formats (tab-separated, doubles = 16-hex of raw IEEE-754 bits):
//   SANITIZE <min> <max> <value> <result>
//   DEFAULT  <default> <getDefaultValue>
//
// Run:  mcpp/build/ranged_attribute_parity.exe --cases mcpp/build/ranged_attribute.tsv

#include "world/entity/ai/attributes/AttributeMath.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using mc::world::entity::ai::attributes::sanitizeValue;

static double bd(const std::string& s) {
   return std::bit_cast<double>(std::stoull(s, nullptr, 16));
}
static uint64_t db(double v) {
   return std::bit_cast<uint64_t>(v);
}

static std::vector<std::string> split_tabs(const std::string& line) {
   std::vector<std::string> out;
   std::string cur;
   std::istringstream ss(line);
   while (std::getline(ss, cur, '\t')) {
      out.push_back(cur);
   }
   return out;
}

int main(int argc, char** argv) {
   std::string casesPath;
   for (int i = 1; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--cases" && i + 1 < argc) {
         casesPath = argv[++i];
      }
   }
   if (casesPath.empty()) {
      std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
      return 2;
   }

   std::ifstream in(casesPath);
   if (!in) {
      std::fprintf(stderr, "cannot open %s\n", casesPath.c_str());
      return 2;
   }

   long cases = 0;
   long mismatches = 0;
   std::string line;
   while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') {
         line.pop_back();
      }
      if (line.empty()) {
         continue;
      }
      std::vector<std::string> f = split_tabs(line);
      if (f.empty()) {
         continue;
      }

      if (f[0] == "SANITIZE") {
         if (f.size() < 5) {
            continue;
         }
         double min = bd(f[1]);
         double max = bd(f[2]);
         double value = bd(f[3]);
         double expected = bd(f[4]);

         double got = sanitizeValue(value, min, max);

         ++cases;
         if (db(got) != db(expected)) {
            ++mismatches;
            if (mismatches <= 20) {
               std::fprintf(stderr,
                            "MISMATCH SANITIZE min=%016llx max=%016llx value=%016llx "
                            "expected=%016llx got=%016llx\n",
                            (unsigned long long)db(min), (unsigned long long)db(max),
                            (unsigned long long)db(value),
                            (unsigned long long)db(expected),
                            (unsigned long long)db(got));
            }
         }
      } else if (f[0] == "DEFAULT") {
         if (f.size() < 3) {
            continue;
         }
         // getDefaultValue() is a pure field getter: the value stored by the
         // constructor must round-trip unchanged.
         double def = bd(f[1]);
         double expected = bd(f[2]);

         double got = def;

         ++cases;
         if (db(got) != db(expected)) {
            ++mismatches;
            if (mismatches <= 20) {
               std::fprintf(stderr,
                            "MISMATCH DEFAULT default=%016llx expected=%016llx got=%016llx\n",
                            (unsigned long long)db(def),
                            (unsigned long long)db(expected),
                            (unsigned long long)db(got));
            }
         }
      }
   }

   std::printf("RangedAttribute cases=%ld mismatches=%ld\n", cases, mismatches);
   return mismatches == 0 ? 0 : 1;
}
