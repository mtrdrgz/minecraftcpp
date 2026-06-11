// Reference value generator for the C++ Axolotl.Variant port
// (mcpp/src/world/entity/animal/axolotl/AxolotlVariant.h).
//
// Runs the REAL decompiled net.minecraft.world.entity.animal.axolotl.Axolotl.Variant
// enum from client.jar so every emitted value is exact ground truth. Only pure
// static helpers on the nested enum are exercised reflectively — no Axolotl
// instance, no world/level, no registry access is constructed.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool AxolotlVariantParity -Out mcpp/build/axolotl_variant.tsv
//
// Rows are tab-separated and dispatched by a leading TAG:
//
//   ID    <ordinal>            <id>            Variant.getId()    for values()[ordinal]
//   NAME  <ordinal>            <base64(name)>  Variant.getName()  for values()[ordinal]
//   BYID  <id>                 <ordinal>       Variant.byId(id).ordinal()  (ZERO strategy)
//   COMMON <seed>              <ordinal>       Variant.getCommonSpawnVariant(LegacyRandomSource(seed)).ordinal()
//   RARE   <seed>              <ordinal>       Variant.getRareSpawnVariant(LegacyRandomSource(seed)).ordinal()
//
// Result columns: ints are decimal; the NAME string is base64 of the UTF-8 bytes.
//
// O is captured at class load so any bootstrap chatter on stdout stays out of the
// TSV; everything else uses reflection against the real enum.
public class AxolotlVariantParity {
    static final java.io.PrintStream O = System.out;

    static String b64(String s) {
        return java.util.Base64.getEncoder()
            .encodeToString(s.getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The nested enum: net.minecraft.world.entity.animal.axolotl.Axolotl$Variant
        @SuppressWarnings("unchecked")
        Class<? extends Enum<?>> variantClass = (Class<? extends Enum<?>>)
            Class.forName("net.minecraft.world.entity.animal.axolotl.Axolotl$Variant");

        Object[] values = variantClass.getEnumConstants();

        java.lang.reflect.Method mGetId = variantClass.getDeclaredMethod("getId");
        java.lang.reflect.Method mGetName = variantClass.getDeclaredMethod("getName");
        java.lang.reflect.Method mById =
            variantClass.getDeclaredMethod("byId", int.class);

        // RandomSource is an interface; create(long) returns a LegacyRandomSource.
        Class<?> randomSourceClass =
            Class.forName("net.minecraft.util.RandomSource");
        java.lang.reflect.Method mCreate =
            randomSourceClass.getDeclaredMethod("create", long.class);

        java.lang.reflect.Method mCommon =
            variantClass.getDeclaredMethod("getCommonSpawnVariant", randomSourceClass);
        java.lang.reflect.Method mRare =
            variantClass.getDeclaredMethod("getRareSpawnVariant", randomSourceClass);

        mGetId.setAccessible(true);
        mGetName.setAccessible(true);
        mById.setAccessible(true);
        mCreate.setAccessible(true);
        mCommon.setAccessible(true);
        mRare.setAccessible(true);

        StringBuilder sb = new StringBuilder();

        // --- ID and NAME for every declared variant -------------------------
        for (Object v : values) {
            int ordinal = ((Enum<?>) v).ordinal();
            int id = (Integer) mGetId.invoke(v);
            String name = (String) mGetName.invoke(v);
            sb.append("ID").append('\t').append(ordinal).append('\t').append(id).append('\n');
            sb.append("NAME").append('\t').append(ordinal).append('\t').append(b64(name)).append('\n');
        }

        // --- byId: in range, plus out-of-range (ZERO strategy => LUCY=0) -----
        int len = values.length;
        int[] idCases = new int[] {
            Integer.MIN_VALUE, -1000000, -128, -2, -1,
            0, 1, 2, 3, 4,
            5, 6, 7, 100, 1000000, Integer.MAX_VALUE,
            len, len - 1, len + 1
        };
        for (int id : idCases) {
            Object r = mById.invoke(null, id);
            int ordinal = ((Enum<?>) r).ordinal();
            sb.append("BYID").append('\t').append(id).append('\t').append(ordinal).append('\n');
        }

        // --- getCommonSpawnVariant / getRareSpawnVariant over many seeds -----
        // A representative battery of LegacyRandomSource seeds, including small,
        // negative, large, and structured values, to exercise the nextInt(4) and
        // nextInt(1) draws (the latter still advances the RNG via next(31)).
        long[] seeds = new long[] {
            0L, 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L, 9L,
            10L, 11L, 12L, 42L, 100L, 123L, 255L, 256L, 1000L, 65535L,
            -1L, -2L, -42L, -1000L, -123456789L,
            123456789L, 987654321L, 2147483647L, 4294967296L,
            9223372036854775807L, -9223372036854775808L,
            0x5DEECE66DL, 0x123456789ABCDEFL, 0xDEADBEEFL, 0xCAFEBABEL
        };
        for (long seed : seeds) {
            Object randC = mCreate.invoke(null, seed);
            Object cv = mCommon.invoke(null, randC);
            int cOrdinal = ((Enum<?>) cv).ordinal();
            sb.append("COMMON").append('\t').append(seed).append('\t').append(cOrdinal).append('\n');

            Object randR = mCreate.invoke(null, seed);
            Object rv = mRare.invoke(null, randR);
            int rOrdinal = ((Enum<?>) rv).ordinal();
            sb.append("RARE").append('\t').append(seed).append('\t').append(rOrdinal).append('\n');
        }

        O.print(sb);
        O.flush();
    }
}
