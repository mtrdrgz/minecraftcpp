// Ground-truth generator for net.minecraft.world.level.levelgen.Column
// (Minecraft 26.1.2) using the REAL decompiled class. Pure integer column/ray
// range algebra; getCeiling/getFloor/getHeight return java.util.OptionalInt.
//
//   tools/run_groundtruth.ps1 -Tool ColumnParity -Out mcpp/build/column.tsv
//
// TSV row format (tab-separated), dispatched by leading TAG in the C++ test.
// OptionalInt is emitted as <present 1/0>\t<value> (value 0 when empty). Ints
// in decimal, booleans 1/0, strings raw. No Bootstrap needed (Column is a pure
// math class — it does not touch registries, world, or block state here; the
// world-coupled scan() is intentionally NOT exercised).
//
// Validity note: Column.Range's ctor throws IllegalArgumentException when
// height() = ceiling - floor - 1 < 0. inside(f,c) builds Range(f,c); around(l,h)
// builds Range(l-1,h+1). We emit a VALID flag column for those two factories so
// the C++ port can assert the same throw/no-throw boundary, and only query the
// accessors when the construction succeeds.

import java.util.OptionalInt;
import net.minecraft.world.level.levelgen.Column;

public class ColumnParity {
    static final java.io.PrintStream O = System.out;

    // Wide int battery: zeros, units, signs, powers of two, large/overflow-prone.
    // Kept finite/physical (no special encodings needed — these are plain ints).
    static final int[] VALS = {
        0, 1, -1, 2, -2, 3, 4, 7, 8, 15, 16, -16, 17, 31, 32, -32,
        63, 64, 100, -100, 127, 128, 255, 256, -256, 320, -64,
        1000, -1000, 4095, 4096, 65535, 65536, -65536,
        1000000, -1000000, 1073741824, -1073741824,
        2147483646, 2147483647, -2147483647, -2147483648
    };

    // OptionalInt sample set for create/withFloor/withCeiling (present + empty).
    static OptionalInt[] OPTS = {
        OptionalInt.empty(),
        OptionalInt.of(0), OptionalInt.of(1), OptionalInt.of(-1),
        OptionalInt.of(16), OptionalInt.of(64), OptionalInt.of(-64),
        OptionalInt.of(100), OptionalInt.of(-100), OptionalInt.of(320),
        OptionalInt.of(2147483647), OptionalInt.of(-2147483648)
    };

    static String opt(OptionalInt o) {
        return (o.isPresent() ? "1\t" + o.getAsInt() : "0\t0");
    }

    // Emit getCeiling/getFloor/getHeight for a constructed Column under TAG.
    static void accessors(String tag, String inputs, Column c) {
        O.println(tag + "\t" + inputs
            + "\t" + opt(c.getCeiling())
            + "\t" + opt(c.getFloor())
            + "\t" + opt(c.getHeight())
            + "\t" + c.toString());
    }

    public static void main(String[] args) throws Exception {
        // ---- inside(floor, ceiling) -> Range(floor, ceiling) --------------
        // Range ctor throws when ceiling - floor - 1 < 0. Emit VALID flag so the
        // C++ port asserts the same boundary, then accessors only when valid.
        for (int floor : VALS) {
            for (int ceiling : VALS) {
                boolean valid;
                try {
                    Column c = Column.inside(floor, ceiling);
                    valid = true;
                    // INSIDE\tfloor\tceiling\t1\t<ceil opt>\t<floor opt>\t<height opt>\t<str>
                    accessors("INSIDE", floor + "\t" + ceiling + "\t1", c);
                } catch (IllegalArgumentException e) {
                    valid = false;
                    O.println("INSIDE\t" + floor + "\t" + ceiling + "\t0"
                        + "\t0\t0\t0\t0\t0\t0\t-");
                }
            }
        }

        // ---- around(lowest, highest) -> Range(lowest-1, highest+1) --------
        for (int lowest : VALS) {
            for (int highest : VALS) {
                try {
                    Column c = Column.around(lowest, highest);
                    accessors("AROUND", lowest + "\t" + highest + "\t1", c);
                } catch (IllegalArgumentException e) {
                    O.println("AROUND\t" + lowest + "\t" + highest + "\t0"
                        + "\t0\t0\t0\t0\t0\t0\t-");
                }
            }
        }

        // ---- below(ceiling) -> Ray(ceiling, false) ------------------------
        for (int v : VALS) {
            accessors("BELOW", String.valueOf(v), Column.below(v));
        }
        // ---- fromHighest(highest) -> Ray(highest+1, false) ----------------
        for (int v : VALS) {
            accessors("FROMHIGHEST", String.valueOf(v), Column.fromHighest(v));
        }
        // ---- above(floor) -> Ray(floor, true) -----------------------------
        for (int v : VALS) {
            accessors("ABOVE", String.valueOf(v), Column.above(v));
        }
        // ---- fromLowest(lowest) -> Ray(lowest-1, true) --------------------
        for (int v : VALS) {
            accessors("FROMLOWEST", String.valueOf(v), Column.fromLowest(v));
        }

        // ---- line() -------------------------------------------------------
        accessors("LINE", "0", Column.line());

        // ---- create(floor?, ceiling?) ------------------------------------
        // When both present, dispatches to inside() which can throw; flag VALID.
        for (OptionalInt f : OPTS) {
            for (OptionalInt cc : OPTS) {
                String in = opt(f) + "\t" + opt(cc);
                try {
                    Column c = Column.create(f, cc);
                    O.println("CREATE\t" + in + "\t1"
                        + "\t" + opt(c.getCeiling())
                        + "\t" + opt(c.getFloor())
                        + "\t" + opt(c.getHeight())
                        + "\t" + c.toString());
                } catch (IllegalArgumentException e) {
                    O.println("CREATE\t" + in + "\t0\t0\t0\t0\t0\t0\t0\t-");
                }
            }
        }

        // ---- withFloor(OptionalInt) on a base column ----------------------
        // Base columns span line/ray/range; withFloor = create(floor, base.getCeiling()).
        Column[] bases = {
            Column.line(),
            Column.below(100), Column.above(50),
            Column.below(-32), Column.above(0),
            Column.inside(0, 320), Column.inside(-64, 320), Column.around(0, 100)
        };
        String[] baseNames = { "line", "below100", "above50", "belowm32",
                               "above0", "inside0_320", "insidem64_320", "around0_100" };
        for (int bi = 0; bi < bases.length; bi++) {
            Column base = bases[bi];
            for (OptionalInt f : OPTS) {
                String in = baseNames[bi] + "\t" + opt(f);
                try {
                    Column c = base.withFloor(f);
                    O.println("WITHFLOOR\t" + in + "\t1"
                        + "\t" + opt(c.getCeiling())
                        + "\t" + opt(c.getFloor())
                        + "\t" + opt(c.getHeight())
                        + "\t" + c.toString());
                } catch (IllegalArgumentException e) {
                    O.println("WITHFLOOR\t" + in + "\t0\t0\t0\t0\t0\t0\t0\t-");
                }
            }
            for (OptionalInt cc : OPTS) {
                String in = baseNames[bi] + "\t" + opt(cc);
                try {
                    Column c = base.withCeiling(cc);
                    O.println("WITHCEILING\t" + in + "\t1"
                        + "\t" + opt(c.getCeiling())
                        + "\t" + opt(c.getFloor())
                        + "\t" + opt(c.getHeight())
                        + "\t" + c.toString());
                } catch (IllegalArgumentException e) {
                    O.println("WITHCEILING\t" + in + "\t0\t0\t0\t0\t0\t0\t0\t-");
                }
            }
        }
    }
}
