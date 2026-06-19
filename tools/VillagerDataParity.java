// Reference value generator for the C++ VillagerData port
// (mcpp/src/world/entity/npc/villager/VillagerData.h).
//
// Runs the REAL decompiled net.minecraft.world.entity.npc.villager.VillagerData
// from client.jar so every emitted value is exact ground truth. Only the pure,
// world-free arithmetic is exercised:
//
//   * static int  getMinXpPerLevel(int level)
//   * static int  getMaxXpPerLevel(int level)
//   * static boolean canLevelUp(int currentLevel)
//   * the compact-constructor clamp  level = Math.max(1, level)
//
// The three statics are invoked reflectively. The clamp is driven by invoking the
// canonical record constructor (Holder<VillagerType>, Holder<VillagerProfession>,
// int) with null Holders — the compact constructor body only touches `level`, so
// no registry/Holder is dereferenced — then reading back level() via the accessor.
// No Villager instance, no world/level is built. The tested methods are
// registry-free, but loading the class runs its <clinit>, which builds a CODEC
// referencing BuiltInRegistries.VILLAGER_TYPE; so SharedConstants/Bootstrap are
// invoked first (their stdout chatter is harmless -- only `sb` is printed at end).
//
//   mcpp/tools/run_groundtruth.ps1 -Tool VillagerDataParity -Out mcpp/build/villager_data.tsv
//
// Rows are tab-separated and dispatched by a leading TAG:
//
//   MINXP  <level>          <int>      VillagerData.getMinXpPerLevel(level)
//   MAXXP  <level>          <int>      VillagerData.getMaxXpPerLevel(level)
//   CANLVL <level>          <0|1>      VillagerData.canLevelUp(level)
//   CLAMP  <inLevel>        <int>      new VillagerData(null, null, inLevel).level()
//   CONST  <name(b64)>      <int>      a named public static final int constant
//
// Result columns: ints are decimal; the CONST name is base64 of the UTF-8 bytes.
//
// O is captured at class load so any chatter on stdout stays out of the TSV.
public class VillagerDataParity {
    static final java.io.PrintStream O = System.out;

    static String b64(String s) {
        return java.util.Base64.getEncoder()
            .encodeToString(s.getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }

    public static void main(String[] args) throws Exception {
        // VillagerData's static initializer builds a CODEC that references
        // BuiltInRegistries.VILLAGER_TYPE, which throws "Not bootstrapped" unless
        // the registries are bootstrapped first. The tested methods themselves are
        // registry-free, but loading the class triggers <clinit>, so we bootstrap.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> vd =
            Class.forName("net.minecraft.world.entity.npc.villager.VillagerData");

        java.lang.reflect.Method mMin =
            vd.getDeclaredMethod("getMinXpPerLevel", int.class);
        java.lang.reflect.Method mMax =
            vd.getDeclaredMethod("getMaxXpPerLevel", int.class);
        java.lang.reflect.Method mCan =
            vd.getDeclaredMethod("canLevelUp", int.class);
        mMin.setAccessible(true);
        mMax.setAccessible(true);
        mCan.setAccessible(true);

        // Canonical record constructor: (Holder type, Holder profession, int level).
        // The component types are Holder<...>; we find the 3-arg ctor whose last
        // parameter is int and pass nulls for the two Holders. The compact ctor
        // body only assigns `level = Math.max(1, level)`, never touching the
        // Holders, so null is safe and registry-free.
        java.lang.reflect.Constructor<?> canonical = null;
        for (java.lang.reflect.Constructor<?> ctor : vd.getDeclaredConstructors()) {
            Class<?>[] ps = ctor.getParameterTypes();
            if (ps.length == 3 && ps[2] == int.class) {
                canonical = ctor;
                break;
            }
        }
        if (canonical == null) {
            throw new IllegalStateException("VillagerData canonical (_, _, int) constructor not found");
        }
        canonical.setAccessible(true);
        java.lang.reflect.Method mLevel = vd.getDeclaredMethod("level");
        mLevel.setAccessible(true);

        StringBuilder sb = new StringBuilder();

        // --- the named public static final int constants -------------------
        for (String fieldName : new String[] {"MIN_VILLAGER_LEVEL", "MAX_VILLAGER_LEVEL"}) {
            java.lang.reflect.Field f = vd.getDeclaredField(fieldName);
            f.setAccessible(true);
            int value = f.getInt(null);
            sb.append("CONST").append('\t').append(b64(fieldName)).append('\t').append(value).append('\n');
        }

        // --- getMin / getMax / canLevelUp over a broad level battery -------
        // Cover every in-range level (1..4), the boundaries (0 and 5 where the
        // array would go out of bounds without the canLevelUp gate), negatives,
        // and the int extremes (which must short-circuit to 0 / false, never index).
        int[] levels = new int[] {
            Integer.MIN_VALUE, -1000000, -100, -5, -2, -1,
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,
            100, 1000000, Integer.MAX_VALUE
        };
        for (int level : levels) {
            int min = (Integer) mMin.invoke(null, level);
            int max = (Integer) mMax.invoke(null, level);
            boolean can = (Boolean) mCan.invoke(null, level);
            sb.append("MINXP").append('\t').append(level).append('\t').append(min).append('\n');
            sb.append("MAXXP").append('\t').append(level).append('\t').append(max).append('\n');
            sb.append("CANLVL").append('\t').append(level).append('\t').append(can ? 1 : 0).append('\n');
        }

        // --- the constructor clamp: level = Math.max(1, level) -------------
        int[] clampInputs = new int[] {
            Integer.MIN_VALUE, -1000000, -100, -2, -1,
            0, 1, 2, 3, 4, 5, 6, 99, 1000,
            1000000, Integer.MAX_VALUE
        };
        for (int in : clampInputs) {
            Object inst = canonical.newInstance(null, null, in);
            int got = (Integer) mLevel.invoke(inst);
            sb.append("CLAMP").append('\t').append(in).append('\t').append(got).append('\n');
        }

        O.print(sb);
        O.flush();
    }
}
