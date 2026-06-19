// Reference value generator for the C++ Rabbit.Variant port
// (mcpp/src/world/entity/animal/rabbit/RabbitVariant.h).
//
// Runs the REAL decompiled net.minecraft.world.entity.animal.rabbit.Rabbit.Variant
// enum from client.jar so every emitted value is exact ground truth. Only the
// pure static/instance helpers on the nested enum are exercised reflectively --
// no Rabbit instance, no world/level, no registry access is constructed.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool RabbitVariantParity -Out mcpp/build/rabbit_variant.tsv
//
// Rows are tab-separated and dispatched by a leading TAG:
//
//   ID    <ordinal>   <id>            Variant.id()                for values()[ordinal]
//   NAME  <ordinal>   <base64(name)>  Variant.getSerializedName() for values()[ordinal]
//   BYID  <id>        <ordinal>       Variant.byId(id).ordinal()  (ByIdMap.sparse, default BROWN)
//
// Result columns: ints are decimal; the NAME string is base64 of the UTF-8 bytes.
//
// O is captured at class load so any bootstrap chatter on stdout stays out of the
// TSV; everything else uses reflection against the real enum. ByIdMap.sparse is a
// hash map keyed by the declared id() (ids {0,1,2,3,4,5,99}); every other id --
// negatives, the holes 6..98, and 100+ -- must fold to DEFAULT (BROWN, ordinal 0).
@SuppressWarnings({"deprecation", "unchecked"})
public class RabbitVariantParity {
    static final java.io.PrintStream O = System.out;

    static String b64(String s) {
        return java.util.Base64.getEncoder()
            .encodeToString(s.getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The nested enum: net.minecraft.world.entity.animal.rabbit.Rabbit$Variant
        Class<? extends Enum<?>> variantClass = (Class<? extends Enum<?>>)
            Class.forName("net.minecraft.world.entity.animal.rabbit.Rabbit$Variant");

        Object[] values = variantClass.getEnumConstants();

        java.lang.reflect.Method mId = variantClass.getDeclaredMethod("id");
        java.lang.reflect.Method mName =
            variantClass.getDeclaredMethod("getSerializedName");
        java.lang.reflect.Method mById =
            variantClass.getDeclaredMethod("byId", int.class);

        mId.setAccessible(true);
        mName.setAccessible(true);
        mById.setAccessible(true);

        StringBuilder sb = new StringBuilder();

        // --- ID and NAME for every declared variant -------------------------
        for (Object v : values) {
            int ordinal = ((Enum<?>) v).ordinal();
            int id = (Integer) mId.invoke(v);
            String name = (String) mName.invoke(v);
            sb.append("ID").append('\t').append(ordinal).append('\t').append(id).append('\n');
            sb.append("NAME").append('\t').append(ordinal).append('\t').append(b64(name)).append('\n');
        }

        // --- byId: sweep the whole interesting domain ------------------------
        // Declared keys {0,1,2,3,4,5,99} hit their variant; everything else --
        // negatives, the big hole 6..98, the boundary around 99, and 100+ --
        // must return DEFAULT (BROWN, ordinal 0). We enumerate every int in
        // [-4, 110] (covers all keys, the full hole, both 99 boundaries) plus
        // far-flung and extreme ids.
        java.util.LinkedHashSet<Integer> idSet = new java.util.LinkedHashSet<>();
        for (int id = -4; id <= 110; id++) {
            idSet.add(id);
        }
        int[] extras = new int[] {
            Integer.MIN_VALUE, Integer.MIN_VALUE + 1, -2000000000, -1000000,
            -65536, -256, -128, -100, -99, -7, -6,
            127, 128, 200, 255, 256, 999, 1000, 65535, 65536,
            1000000, 2000000000, Integer.MAX_VALUE - 1, Integer.MAX_VALUE
        };
        for (int id : extras) {
            idSet.add(id);
        }
        for (int id : idSet) {
            Object r = mById.invoke(null, id);
            int ordinal = ((Enum<?>) r).ordinal();
            sb.append("BYID").append('\t').append(id).append('\t').append(ordinal).append('\n');
        }

        O.print(sb);
        O.flush();
    }
}
