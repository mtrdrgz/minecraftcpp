// Bit-exact parity gate for net.minecraft.world.entity.ai.attributes
//   .AttributeModifier.Operation (id/ordinal/name/getSerializedName) AND the
// AttributeInstance value-combine formula it drives.
//
// Reuses the certified header world/entity/ai/attributes/AttributeMath.h
// (Operation enum + calculateValue) and the enum-metadata accessors in
// world/entity/ai/attributes/AttributeOperation.h. No engine header is modified.
//
// Reads ground-truth rows from AttributeModifierOpParity.java:
//   OP   <ordinal> <id> <name> <serializedName>
//   CALC <base> <min> <max> <nAdd> [add...] <nMulBase> [mulBase...]
//          <nMulTotal> [mulTotal...] <result>
// doubles are 16-hex of the raw IEEE-754 bits, in Java's map-iteration order.
//
// Run:  mcpp/build/attribute_op_parity.exe --cases mcpp/build/attribute_op.tsv

#include "world/entity/ai/attributes/AttributeMath.h"
#include "world/entity/ai/attributes/AttributeOperation.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using mc::world::entity::ai::attributes::Operation;
using mc::world::entity::ai::attributes::calculateValue;
using mc::world::entity::ai::attributes::operationId;
using mc::world::entity::ai::attributes::operationOrdinal;
using mc::world::entity::ai::attributes::operationName;
using mc::world::entity::ai::attributes::operationSerializedName;

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
      const std::string& tag = f[0];

      if (tag == "OP") {
         // OP <ordinal> <id> <name> <serializedName>
         ++cases;
         int ord = std::stoi(f[1]);
         int id = std::stoi(f[2]);
         const std::string& name = f[3];
         const std::string& serName = f[4];

         // Reconstruct the enum from its ordinal (== declaration index).
         auto op = static_cast<Operation>(ord);
         bool ok = true;
         if (operationOrdinal(op) != ord) ok = false;
         if (operationId(op) != id) ok = false;
         if (name != std::string(operationName(op))) ok = false;
         if (serName != std::string(operationSerializedName(op))) ok = false;

         if (!ok) {
            ++mismatches;
            std::fprintf(stderr,
                         "OP mismatch ord=%d id=%d name=%s serName=%s\n",
                         ord, id, name.c_str(), serName.c_str());
         }
      } else if (tag == "CALC") {
         // CALC <base> <min> <max> <nAdd> [add...] <nMulBase> [...] <nMulTotal> [...] <result>
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
                            "CALC mismatch base=%016llx min=%016llx max=%016llx "
                            "expected=%016llx got=%016llx\n",
                            (unsigned long long)db(base), (unsigned long long)db(min),
                            (unsigned long long)db(max),
                            (unsigned long long)db(expected),
                            (unsigned long long)db(got));
            }
         }
      }
      // unknown tags ignored
   }

   std::printf("AttributeModifierOp cases=%ld mismatches=%ld\n", cases, mismatches);
   return mismatches == 0 ? 0 : 1;
}
