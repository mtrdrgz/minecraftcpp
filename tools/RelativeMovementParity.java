// Ground truth for net.minecraft.world.entity.Relative (Minecraft 26.1.2).
//
// (Assignment calls this "RelativeMovement" — its name in older versions; in
// 26.1.2 the decompiled class is net.minecraft.world.entity.Relative, with the
// same pack(Set)->int / unpack(int)->Set bitset semantics. We call the REAL
// net.minecraft methods.)
//
// We exercise:
//   * each constant's ordinal(), name(), and `bit` field (reflected)
//   * the public static helpers pack/unpack/rotation/position/direction over an
//     exhaustive battery of FINITE int inputs
//   * the public static sets ALL / ROTATION / DELTA (packed via pack())
//
// pack/unpack are public static so we call them directly. The `bit` field is
// private final, read via reflection+setAccessible. We never touch the
// network StreamCodec (SET_STREAM_CODEC) — it merely wraps pack/unpack.
//
// Output is captured at class-load via O = System.out BEFORE bootstrap so engine
// chatter never pollutes the TSV.
//
// Row tags (tab-separated):
//   CONST   <ordinal:int> <name:string> <bit:int> <mask:int>
//   COUNT   <values.length:int>
//   UNPACK  <value:int> <packedResult:int>   (pack(unpack(value)))
//   PACKALL <value:int> <packedResult:int>   (pack of the full unpacked set; == UNPACK)
//   ROT     <yRot:0/1> <xRot:0/1> <packed:int>
//   POS     <x:0/1> <y:0/1> <z:0/1> <packed:int>
//   DIR     <x:0/1> <y:0/1> <z:0/1> <packed:int>
//   STATIC  <ALL:int> <ROTATION:int> <DELTA:int>
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.EnumSet;
import java.util.Set;
import net.minecraft.world.entity.Relative;

public class RelativeMovementParity {
    static final java.io.PrintStream O = System.out;
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) throws Exception {
        // Harmless for a fieldless-ish enum; keeps us robust if class init ever
        // touches registries.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
        }

        Relative[] vals = Relative.values();

        // Reflected private final int bit.
        Field bitField = Relative.class.getDeclaredField("bit");
        bitField.setAccessible(true);

        for (Relative r : vals) {
            int bit = bitField.getInt(r);
            int mask = 1 << bit;
            row("CONST", Integer.toString(r.ordinal()), r.name(),
                Integer.toString(bit), Integer.toString(mask));
        }
        row("COUNT", Integer.toString(vals.length));

        // Public static pack(Set<Relative>) and unpack(int).
        Method packM = Relative.class.getMethod("pack", Set.class);
        Method unpackM = Relative.class.getMethod("unpack", int.class);

        // Exhaustive battery over all 9-bit combinations (0..511) PLUS a handful
        // of values with stray high bits set, to confirm unpack() drops them and
        // pack(unpack(v)) keeps only the nine valid masks. All finite, physical.
        int[] strays = {
            512, 1024, 0x8000, 0x10000, 0x40000000,
            511 | 512, 0xFFFF, 0x7FFFFFFF,
        };

        for (int v = 0; v <= 511; v++) {
            Set<Relative> s = invokeUnpack(unpackM, v);
            int packed = invokePack(packM, s);
            row("UNPACK", Integer.toString(v), Integer.toString(packed));
            // pack of the same unpacked set again (identity check for the path).
            row("PACKALL", Integer.toString(v), Integer.toString(invokePack(packM, s)));
        }
        for (int v : strays) {
            Set<Relative> s = invokeUnpack(unpackM, v);
            int packed = invokePack(packM, s);
            row("UNPACK", Integer.toString(v), Integer.toString(packed));
            row("PACKALL", Integer.toString(v), Integer.toString(invokePack(packM, s)));
        }

        // rotation / position / direction across all boolean combinations.
        for (int yr = 0; yr <= 1; yr++) {
            for (int xr = 0; xr <= 1; xr++) {
                Set<Relative> s = Relative.rotation(yr != 0, xr != 0);
                row("ROT", Integer.toString(yr), Integer.toString(xr),
                    Integer.toString(invokePack(packM, s)));
            }
        }
        for (int x = 0; x <= 1; x++) {
            for (int y = 0; y <= 1; y++) {
                for (int z = 0; z <= 1; z++) {
                    Set<Relative> sp = Relative.position(x != 0, y != 0, z != 0);
                    row("POS", Integer.toString(x), Integer.toString(y),
                        Integer.toString(z), Integer.toString(invokePack(packM, sp)));
                    Set<Relative> sd = Relative.direction(x != 0, y != 0, z != 0);
                    row("DIR", Integer.toString(x), Integer.toString(y),
                        Integer.toString(z), Integer.toString(invokePack(packM, sd)));
                }
            }
        }

        // Static sets, packed.
        row("STATIC",
            Integer.toString(invokePack(packM, Relative.ALL)),
            Integer.toString(invokePack(packM, Relative.ROTATION)),
            Integer.toString(invokePack(packM, Relative.DELTA)));

        O.print(OUT);
        O.flush();
    }

    @SuppressWarnings("unchecked")
    static Set<Relative> invokeUnpack(Method unpackM, int v) throws Exception {
        return (Set<Relative>) unpackM.invoke(null, v);
    }

    static int invokePack(Method packM, Set<Relative> set) throws Exception {
        return (Integer) packM.invoke(null, set);
    }

    static void row(String tag, String... cols) {
        OUT.append(tag);
        for (String c : cols) OUT.append('\t').append(c);
        OUT.append('\n');
    }
}
