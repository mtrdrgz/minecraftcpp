// Ground truth for net.minecraft.world.level.levelgen.Heightmap.Types (MC 26.1.2)
// — the ENUM PART ONLY (no heightmap data array). Drives the REAL enum + its real
// private fields and public accessors.
//
// Source: 26.1.2/src/net/minecraft/world/level/levelgen/Heightmap.java
//   enum Types(int id, String serializationKey, Usage usage, Predicate<BlockState> isOpaque)
//     getSerializationKey() / getSerializedName() -> serializationKey
//     sendToClient()      -> usage == Usage.CLIENT
//     keepAfterWorldgen() -> usage != Usage.WORLDGEN
//     isOpaque()          -> the Predicate<BlockState>  (we report its identity)
//   Heightmap.NOT_AIR / Heightmap.MATERIAL_MOTION_BLOCKING are the two SHARED
//   predicate instances; MOTION_BLOCKING / MOTION_BLOCKING_NO_LEAVES use unique
//   inline lambdas — we classify each constant's isOpaque() into one of four
//   categories by reference identity.
//
// Reflection is used for the private instance fields (id, serializationKey, usage,
// isOpaque) and the private static BY_ID; public accessors are called directly.
//
// Row TAGs (tab-separated; ints/bools decimal, strings raw):
//   CONST <ordinal> <id> <serializedName> <getSerializationKey> <usageOrdinal>
//         <usageName> <sendToClient> <keepAfterWorldgen> <opaqueCategory>
//         one row per Types constant. <opaqueCategory> is 0=NOT_AIR,
//         1=MATERIAL_MOTION_BLOCKING, 2=MOTION_BLOCKING, 3=MOTION_BLOCKING_NO_LEAVES.
//   BYID  <id> <resolvedOrdinal>
//         Types.BY_ID.apply(id).ordinal() (ByIdMap.continuous / ZERO).
//
// NOTE: the isOpaque() predicate BODY (test(BlockState)) is block-registry-coupled
// (isAir / blocksMotion / getFluidState / LeavesBlock), so it is NOT evaluated here
// — we certify only WHICH of the four predicate categories each constant uses
// (by reference identity), which is the metadata the C++ header models.
//
// The C++ test rebuilds the identical table and compares every field BIT-FOR-BIT.

import java.lang.reflect.Field;
import java.util.function.IntFunction;
import java.util.function.Predicate;
import net.minecraft.world.level.levelgen.Heightmap;

public class HeightmapTypesParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Heightmap.Types is a plain enum; bootstrap defensively in case the enum's
        // static init (BY_ID / predicate lambdas referencing Blocks) pulls in
        // registry-touching classes during classloading.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignore) {
            // not required for the enum metadata we read
        }

        // Private instance fields on Heightmap.Types.
        Field idF = field(Heightmap.Types.class, "id");
        Field keyF = field(Heightmap.Types.class, "serializationKey");
        Field usageF = field(Heightmap.Types.class, "usage");
        Field opaqueF = field(Heightmap.Types.class, "isOpaque");

        // The two SHARED static predicate instances on Heightmap.
        Predicate<?> notAir = (Predicate<?>) staticField(Heightmap.class, "NOT_AIR");
        Predicate<?> matMotion = (Predicate<?>) staticField(Heightmap.class, "MATERIAL_MOTION_BLOCKING");

        // ----- one CONST row per enum constant -----
        for (Heightmap.Types t : Heightmap.Types.values()) {
            int id = idF.getInt(t);
            String serialized = t.getSerializedName();           // public @Override
            String key = t.getSerializationKey();                // public
            // sanity: both accessors and the private field must agree.
            String privKey = (String) keyF.get(t);
            if (!privKey.equals(serialized) || !privKey.equals(key)) {
                throw new IllegalStateException("key mismatch for " + t.name());
            }
            Enum<?> usage = (Enum<?>) usageF.get(t);             // Heightmap.Usage
            boolean send = t.sendToClient();                     // public
            boolean keep = t.keepAfterWorldgen();                // public

            Predicate<?> pred = (Predicate<?>) opaqueF.get(t);   // == t.isOpaque()
            int cat = classifyPredicate(t, pred, notAir, matMotion);

            O.println("CONST\t" + t.ordinal() + "\t" + id + "\t" + serialized
                      + "\t" + key + "\t" + usage.ordinal() + "\t" + usage.name()
                      + "\t" + (send ? 1 : 0) + "\t" + (keep ? 1 : 0) + "\t" + cat);
        }

        // ----- BY_ID over a battery of int keys (ZERO strategy) -----
        @SuppressWarnings("unchecked")
        IntFunction<Heightmap.Types> byId =
            (IntFunction<Heightmap.Types>) staticField(Heightmap.Types.class, "BY_ID");
        java.util.LinkedHashSet<Integer> ids = new java.util.LinkedHashSet<>();
        for (int i = -16; i <= 16; i++) ids.add(i);  // straddles [0,6) both sides
        ids.add(Integer.MIN_VALUE);
        ids.add(Integer.MIN_VALUE + 1);
        ids.add(Integer.MAX_VALUE);
        ids.add(Integer.MAX_VALUE - 1);
        ids.add(-100000);
        ids.add(100000);
        for (int id : ids) {
            Heightmap.Types r = byId.apply(id);
            O.println("BYID\t" + id + "\t" + r.ordinal());
        }

        O.flush();
    }

    // Classify the isOpaque predicate by reference identity to the two shared
    // statics; the remaining two are the unique inline lambdas, distinguished by
    // which constant declared them (MOTION_BLOCKING vs MOTION_BLOCKING_NO_LEAVES).
    static int classifyPredicate(Heightmap.Types t, Predicate<?> pred,
                                 Predicate<?> notAir, Predicate<?> matMotion) {
        if (pred == notAir) return 0;       // NOT_AIR
        if (pred == matMotion) return 1;    // MATERIAL_MOTION_BLOCKING
        if (t == Heightmap.Types.MOTION_BLOCKING) return 2;            // unique lambda
        if (t == Heightmap.Types.MOTION_BLOCKING_NO_LEAVES) return 3;  // unique lambda
        throw new IllegalStateException("unclassified predicate for " + t.name());
    }

    static Field field(Class<?> cls, String name) throws Exception {
        Field f = cls.getDeclaredField(name);
        f.setAccessible(true);
        return f;
    }

    static Object staticField(Class<?> cls, String name) throws Exception {
        Field f = cls.getDeclaredField(name);
        f.setAccessible(true);
        return f.get(null);
    }
}
