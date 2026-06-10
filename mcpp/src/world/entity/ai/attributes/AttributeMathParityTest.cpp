// Bit-exact parity gate for AttributeInstance.calculateValue (+ RangedAttribute
// sanitize / Mth.clamp). Reads ground-truth rows from AttributeMathParity.java.
//
// Row format (tab-separated):
//   CALC <base> <min> <max> <nAdd> [add...] <nMulBase> [mulBase...] <nMulTotal> [mulTotal...] <result>
// all doubles as 16-hex of the raw IEEE-754 bits, in Java's map-iteration order.
//
// Run:  mcpp/build/attribute_math_parity.exe --cases mcpp/build/attribute_math.tsv

#include "world/entity/ai/attributes/AttributeMath.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using mc::world::entity::ai::attributes::calculateValue;

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
      if (f.empty() || f[0] != "CALC") {
         continue;
      }

      // Parse fixed head: tag, base, min, max
      size_t idx = 1;
      double base = bd(f[idx++]);
      double min = bd(f[idx++]);
      double max = bd(f[idx++]);

      auto readList = [&](std::vector<double>& dst) {
         int n = std::stoi(f[idx++]);
         dst.clear();
         dst.reserve(static_cast<size_t>(n));
         for (int k = 0; k < n; ++k) {
            dst.push_back(bd(f[idx++]));
         }
      };

      std::vector<double> add, mulBase, mulTotal;
      readList(add);
      readList(mulBase);
      readList(mulTotal);

      double expected = bd(f[idx++]);

      double got = calculateValue(base, min, max, add, mulBase, mulTotal);

      ++cases;
      if (db(got) != db(expected)) {
         ++mismatches;
         if (mismatches <= 20) {
            std::fprintf(stderr,
                         "MISMATCH base=%016llx min=%016llx max=%016llx "
                         "expected=%016llx got=%016llx\n",
                         (unsigned long long)db(base), (unsigned long long)db(min),
                         (unsigned long long)db(max),
                         (unsigned long long)db(expected),
                         (unsigned long long)db(got));
         }
      }
   }

   std::printf("AttributeMath cases=%ld mismatches=%ld\n", cases, mismatches);
   return mismatches == 0 ? 0 : 1;
}
