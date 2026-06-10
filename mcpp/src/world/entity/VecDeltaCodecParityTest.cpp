// Bit-exact parity gate for net.minecraft.network.protocol.game.VecDeltaCodec.
// Reads ground-truth rows from tools/VecDeltaCodecParity.java and recomputes with
// the C++ port (world/entity/VecDeltaCodec.h), comparing every field bit-for-bit.
//
// Row formats (tab-separated):
//   ENC <input_d_hex> <encoded_long>
//   DEC <v_long> <decoded_d_hex>
//   SEQ <baseX> <baseY> <baseZ> <posX> <posY> <posZ>
//       <encX> <encY> <encZ>
//       <decX> <decY> <decZ>
//       <deltaX> <deltaY> <deltaZ>
//       <getBaseX> <getBaseY> <getBaseZ>
// Doubles are 16-hex of raw IEEE-754 bits; longs are decimal.
//
// Run:  mcpp/build/vec_delta_codec_parity.exe --cases mcpp/build/vec_delta_codec.tsv

#include "world/entity/VecDeltaCodec.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static double bd(const std::string& s) {
   return std::bit_cast<double>(std::stoull(s, nullptr, 16));
}
static uint64_t db(double v) {
   return std::bit_cast<uint64_t>(v);
}
static int64_t bl(const std::string& s) {
   return std::stoll(s);
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

      if (f[0] == "ENC") {
         // ENC <input_d_hex> <encoded_long>
         double input = bd(f[1]);
         int64_t expected = bl(f[2]);
         int64_t got = mc::VecDeltaCodec::encode(input);
         ++cases;
         if (got != expected) {
            ++mismatches;
            if (mismatches <= 20) {
               std::fprintf(stderr, "MISMATCH ENC input=%016llx expected=%lld got=%lld\n",
                            (unsigned long long)db(input), (long long)expected, (long long)got);
            }
         }
      } else if (f[0] == "DEC") {
         // DEC <v_long> <decoded_d_hex>
         int64_t v = bl(f[1]);
         double expected = bd(f[2]);
         double got = mc::VecDeltaCodec::decode(v);
         ++cases;
         if (db(got) != db(expected)) {
            ++mismatches;
            if (mismatches <= 20) {
               std::fprintf(stderr, "MISMATCH DEC v=%lld expected=%016llx got=%016llx\n",
                            (long long)v, (unsigned long long)db(expected),
                            (unsigned long long)db(got));
            }
         }
      } else if (f[0] == "SEQ") {
         // SEQ baseX baseY baseZ posX posY posZ encX encY encZ decX decY decZ
         //     deltaX deltaY deltaZ getBaseX getBaseY getBaseZ
         size_t idx = 1;
         double bx = bd(f[idx++]), by = bd(f[idx++]), bz = bd(f[idx++]);
         double px = bd(f[idx++]), py = bd(f[idx++]), pz = bd(f[idx++]);
         int64_t exEncX = bl(f[idx++]), exEncY = bl(f[idx++]), exEncZ = bl(f[idx++]);
         double exDecX = bd(f[idx++]), exDecY = bd(f[idx++]), exDecZ = bd(f[idx++]);
         double exDeltaX = bd(f[idx++]), exDeltaY = bd(f[idx++]), exDeltaZ = bd(f[idx++]);
         double exBaseX = bd(f[idx++]), exBaseY = bd(f[idx++]), exBaseZ = bd(f[idx++]);

         mc::VecDeltaCodec codec;
         codec.setBase(mc::Vec3(bx, by, bz));
         mc::Vec3 pos(px, py, pz);

         int64_t encX = codec.encodeX(pos);
         int64_t encY = codec.encodeY(pos);
         int64_t encZ = codec.encodeZ(pos);
         mc::Vec3 dec = codec.decode(encX, encY, encZ);
         mc::Vec3 delta = codec.delta(pos);
         const mc::Vec3& gb = codec.getBase();

         ++cases;
         bool bad =
            encX != exEncX || encY != exEncY || encZ != exEncZ ||
            db(dec.x) != db(exDecX) || db(dec.y) != db(exDecY) || db(dec.z) != db(exDecZ) ||
            db(delta.x) != db(exDeltaX) || db(delta.y) != db(exDeltaY) || db(delta.z) != db(exDeltaZ) ||
            db(gb.x) != db(exBaseX) || db(gb.y) != db(exBaseY) || db(gb.z) != db(exBaseZ);
         if (bad) {
            ++mismatches;
            if (mismatches <= 20) {
               std::fprintf(stderr,
                            "MISMATCH SEQ base=(%016llx,%016llx,%016llx) pos=(%016llx,%016llx,%016llx)\n"
                            "  enc got=(%lld,%lld,%lld) exp=(%lld,%lld,%lld)\n"
                            "  dec got=(%016llx,%016llx,%016llx) exp=(%016llx,%016llx,%016llx)\n"
                            "  delta got=(%016llx,%016llx,%016llx) exp=(%016llx,%016llx,%016llx)\n"
                            "  base got=(%016llx,%016llx,%016llx) exp=(%016llx,%016llx,%016llx)\n",
                            (unsigned long long)db(bx), (unsigned long long)db(by), (unsigned long long)db(bz),
                            (unsigned long long)db(px), (unsigned long long)db(py), (unsigned long long)db(pz),
                            (long long)encX, (long long)encY, (long long)encZ,
                            (long long)exEncX, (long long)exEncY, (long long)exEncZ,
                            (unsigned long long)db(dec.x), (unsigned long long)db(dec.y), (unsigned long long)db(dec.z),
                            (unsigned long long)db(exDecX), (unsigned long long)db(exDecY), (unsigned long long)db(exDecZ),
                            (unsigned long long)db(delta.x), (unsigned long long)db(delta.y), (unsigned long long)db(delta.z),
                            (unsigned long long)db(exDeltaX), (unsigned long long)db(exDeltaY), (unsigned long long)db(exDeltaZ),
                            (unsigned long long)db(gb.x), (unsigned long long)db(gb.y), (unsigned long long)db(gb.z),
                            (unsigned long long)db(exBaseX), (unsigned long long)db(exBaseY), (unsigned long long)db(exBaseZ));
            }
         }
      }
   }

   std::printf("VecDeltaCodec cases=%ld mismatches=%ld\n", cases, mismatches);
   return mismatches == 0 ? 0 : 1;
}
