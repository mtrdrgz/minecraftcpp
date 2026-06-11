// Ground truth for net.minecraft.world.item.component.DyedItemColor.applyDyes
// (Minecraft Java Edition 26.1.2).
//
// Exercises the REAL class:
//   - applyDyes(@Nullable DyedItemColor currentDye, List<DyeColor> dyes)
//                                                       DyedItemColor.java:40-79
//       averages per-channel RGB across the current colour (if any) + each dye's
//       textureDiffuseColor, then rescales to the average peak intensity.
//
// applyDyes(DyedItemColor, List) is public static, so we resolve it via
// reflection and invoke it with no instance — NO world/registry/ItemStack
// coupling for this overload. The current colour is built with the public record
// constructor new DyedItemColor(int rgb); dyes are DyeColor enum constants. We
// still bootstrap SharedConstants/Bootstrap so the classes load exactly as
// in-game (DyeColor's enum-constant ARGB.opaque() etc. run on class init).
//
// The returned DyedItemColor.rgb() int is emitted as 8-hex (raw 32-bit) so the
// C++ side compares the EXACT bit pattern — no tolerance.
//
// Row tag (tab-separated):
//   DYE  <hasCurrent:0|1>  <currentRgb8>  <ndyes>  <dyeId0,dyeId1,...>  <resultRgb8>
//   (when hasCurrent==0 the currentRgb8 field is "00000000" and is ignored;
//    when ndyes==0 the dye-id list field is "-")
@SuppressWarnings({"deprecation", "unchecked"})
public class DyedItemColorParity {
    static final java.io.PrintStream O = System.out;

    static String h8(int v) { return String.format("%08x", v); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> dyedCls = Class.forName("net.minecraft.world.item.component.DyedItemColor");
        Class<?> dyeColorCls = Class.forName("net.minecraft.world.item.DyeColor");

        // applyDyes(DyedItemColor, List)
        java.lang.reflect.Method applyDyes = dyedCls.getMethod(
            "applyDyes", dyedCls, java.util.List.class);
        applyDyes.setAccessible(true);

        // new DyedItemColor(int) — the canonical record constructor.
        java.lang.reflect.Constructor<?> dyedCtor = dyedCls.getConstructor(int.class);
        dyedCtor.setAccessible(true);

        // DyedItemColor.rgb() accessor.
        java.lang.reflect.Method rgbAccessor = dyedCls.getMethod("rgb");
        rgbAccessor.setAccessible(true);

        // The 16 DyeColor constants in ordinal order (WHITE..BLACK).
        Object[] dyeConstants = dyeColorCls.getEnumConstants();

        // ---- battery ----------------------------------------------------------
        // 1) Single dye, no current colour — one row per DyeColor (the canonical
        //    "dye a plain leather piece once" case).
        for (int i = 0; i < dyeConstants.length; i++) {
            emit(applyDyes, dyedCtor, rgbAccessor, dyeConstants,
                 null, new int[]{i});
        }

        // 2) Every ordered pair of dyes (no current colour) — 16*16 = 256 rows.
        //    Order matters to the averaging? No — addition is commutative — but
        //    the integer/float truncation can differ by pairing, so exercise all.
        for (int a = 0; a < dyeConstants.length; a++) {
            for (int b = 0; b < dyeConstants.length; b++) {
                emit(applyDyes, dyedCtor, rgbAccessor, dyeConstants,
                     null, new int[]{a, b});
            }
        }

        // 3) Every ordered triple of dyes (no current colour) — 16^3 = 4096 rows.
        for (int a = 0; a < dyeConstants.length; a++) {
            for (int b = 0; b < dyeConstants.length; b++) {
                for (int c = 0; c < dyeConstants.length; c++) {
                    emit(applyDyes, dyedCtor, rgbAccessor, dyeConstants,
                         null, new int[]{a, b, c});
                }
            }
        }

        // 4) A current colour + each single dye — exercises the currentDye branch
        //    with a spread of starting RGBs (including channel extremes, the
        //    LEATHER_COLOR default, and asymmetric/grey colours).
        int[] currents = {
            0x000000, 0xFFFFFF, 0xA06540 /* ~LEATHER_COLOR rgb */,
            0xFF0000, 0x00FF00, 0x0000FF, 0x010203, 0x7F7F7F,
            0x123456, 0xABCDEF, 0x00FF80, 0x804000, 0x0F0F0F, 0xFE0102
        };
        for (int cur : currents) {
            for (int i = 0; i < dyeConstants.length; i++) {
                emit(applyDyes, dyedCtor, rgbAccessor, dyeConstants,
                     Integer.valueOf(cur), new int[]{i});
            }
        }

        // 5) A current colour + a pair of dyes — a focused slice (a few currents
        //    crossed with all 256 dye pairs) to exercise the 3+-colour averaging
        //    with a seeded starting colour.
        int[] mixCurrents = {0xA06540, 0x336699, 0xFFFFFF, 0x000000};
        for (int cur : mixCurrents) {
            for (int a = 0; a < dyeConstants.length; a++) {
                for (int b = 0; b < dyeConstants.length; b++) {
                    emit(applyDyes, dyedCtor, rgbAccessor, dyeConstants,
                         Integer.valueOf(cur), new int[]{a, b});
                }
            }
        }
    }

    // Build the DyedItemColor current (or null), the List<DyeColor>, invoke the
    // REAL applyDyes, read back rgb(), and print the TSV row. NEVER replicate the
    // body Java-side — we only call the real method.
    static void emit(java.lang.reflect.Method applyDyes,
                     java.lang.reflect.Constructor<?> dyedCtor,
                     java.lang.reflect.Method rgbAccessor,
                     Object[] dyeConstants,
                     Integer currentRgb,
                     int[] dyeIds) throws Exception {
        Object current = null;
        if (currentRgb != null) {
            current = dyedCtor.newInstance(currentRgb.intValue());
        }

        java.util.List<Object> dyes = new java.util.ArrayList<>();
        StringBuilder idList = new StringBuilder();
        for (int k = 0; k < dyeIds.length; k++) {
            dyes.add(dyeConstants[dyeIds[k]]);
            if (k > 0) idList.append(',');
            idList.append(dyeIds[k]);
        }

        Object result = applyDyes.invoke(null, current, dyes);
        int resultRgb = ((Integer) rgbAccessor.invoke(result)).intValue();

        int hasCurrent = (currentRgb != null) ? 1 : 0;
        String curField = (currentRgb != null) ? h8(currentRgb.intValue()) : "00000000";
        String idField = (dyeIds.length == 0) ? "-" : idList.toString();

        O.println("DYE\t" + hasCurrent + "\t" + curField + "\t" + dyeIds.length
                  + "\t" + idField + "\t" + h8(resultRgb));
    }
}
