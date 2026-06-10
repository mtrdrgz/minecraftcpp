// Reference value generator for the C++ mc::levelgen::RandomSupport port.
// Runs the REAL decompiled net.minecraft.world.level.levelgen.RandomSupport
// (and its Seed128bit record) from client.jar so every emitted value is exact
// ground truth.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/RandomSupportParity.java
//   java  -cp "<out>;26.1.2/client.jar;26.1.2/libs/guava-33.5.0-jre.jar" RandomSupportParity > random_support.tsv
//
// All outputs are longs, emitted decimal (signed two's-complement) so the C++
// comparison via std::bit_cast / direct equality is exact.
//
// Covered (verbatim against real net.minecraft methods):
//   MIX    mixStafford13(z)
//   UNMIX  upgradeSeedTo128bitUnmixed(seed) -> (lo, hi)
//   UP     upgradeSeedTo128bit(seed)        -> (lo, hi)
//   XOR    Seed128bit(lo,hi).xor(xlo,xhi)   -> (lo, hi)  (long overload)
//   XORS   a.xor(b)                          -> (lo, hi)  (Seed128bit overload)
//   MIXED  Seed128bit(lo,hi).mixed()        -> (lo, hi)
//   HASH   seedFromHashOf(string)           -> (lo, hi)  (real Guava MD5)
//
// generateUniqueSeed()/SEED_UNIQUIFIER are time-dependent (System.nanoTime) and
// stateful (AtomicLong) -> not portable to a deterministic gate; intentionally
// excluded.
import net.minecraft.world.level.levelgen.RandomSupport;
import net.minecraft.world.level.levelgen.RandomSupport.Seed128bit;

public class RandomSupportParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Finite/physical long inputs: representative legacy seeds across the
        // 64-bit range plus structurally interesting bit patterns.
        long[] seeds = {
            0L, 1L, -1L, 2L, 42L,
            123456789L, -987654321L,
            2147483647L, -2147483648L,
            4294967296L, -4294967296L,
            1181783497276652981L, -7046029254386353131L, 7640891576956012809L,
            9223372036854775807L, -9223372036854775808L,
            0x0123456789ABCDEFL, 0xFEDCBA9876543210L,
            0x5555555555555555L, 0xAAAAAAAAAAAAAAAAL,
            8682522807148012L, 25214903917L, 281474976710655L
        };

        // MIX: mixStafford13(z)
        for (long z : seeds) {
            O.println("MIX\t" + z + "\t" + RandomSupport.mixStafford13(z));
        }

        // UNMIX: upgradeSeedTo128bitUnmixed(seed)
        for (long s : seeds) {
            Seed128bit r = RandomSupport.upgradeSeedTo128bitUnmixed(s);
            O.println("UNMIX\t" + s + "\t" + r.seedLo() + "\t" + r.seedHi());
        }

        // UP: upgradeSeedTo128bit(seed)
        for (long s : seeds) {
            Seed128bit r = RandomSupport.upgradeSeedTo128bit(s);
            O.println("UP\t" + s + "\t" + r.seedLo() + "\t" + r.seedHi());
        }

        // XOR (long overload), XORS (Seed128bit overload), MIXED.
        // Build the base seed from the unmixed upgrade so we exercise realistic
        // 128-bit states, and cross every base with every xor operand.
        for (int i = 0; i < seeds.length; i++) {
            Seed128bit base = RandomSupport.upgradeSeedTo128bitUnmixed(seeds[i]);

            // MIXED of the base.
            Seed128bit m = base.mixed();
            O.println("MIXED\t" + base.seedLo() + "\t" + base.seedHi() + "\t" + m.seedLo() + "\t" + m.seedHi());

            for (int j = 0; j < seeds.length; j++) {
                long xlo = seeds[j];
                long xhi = seeds[(j + 7) % seeds.length];

                Seed128bit rx = base.xor(xlo, xhi);
                O.println("XOR\t" + base.seedLo() + "\t" + base.seedHi() + "\t" + xlo + "\t" + xhi
                        + "\t" + rx.seedLo() + "\t" + rx.seedHi());

                Seed128bit other = new Seed128bit(xlo, xhi);
                Seed128bit rxs = base.xor(other);
                O.println("XORS\t" + base.seedLo() + "\t" + base.seedHi() + "\t" + xlo + "\t" + xhi
                        + "\t" + rxs.seedLo() + "\t" + rxs.seedHi());
            }
        }

        // HASH: seedFromHashOf(string) via the real Guava MD5_128. Strings are
        // raw (single token, no whitespace) so they survive tab parsing. These
        // mirror real worldgen noise/router names plus edge cases.
        String[] names = {
            "",
            "a",
            "ab",
            "abc",
            "minecraft:overworld",
            "minecraft:temperature",
            "minecraft:vegetation",
            "minecraft:continentalness",
            "minecraft:erosion",
            "minecraft:depth",
            "minecraft:ridge",
            "minecraft:offset",
            "octave_0",
            "octave_-7",
            "WorldGenRegion",
            "0123456789",
            "the_quick_brown_fox",
            "message_digest",
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
        };
        for (String n : names) {
            Seed128bit r = RandomSupport.seedFromHashOf(n);
            O.println("HASH\t" + n + "\t" + r.seedLo() + "\t" + r.seedHi());
        }
    }
}
