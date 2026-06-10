// Ground-truth generator for net.minecraft.world.level.material.MapColor (the
// map-color col/id table + nested Brightness enum + the pure color math
// calculateARGBColor / getColorFromPackedId / getPackedId). Pure value math; no
// world/registry needed, but a Bootstrap guard is included in case static init of
// related classes trips "Not bootstrapped".
//
// Emits tab-separated <TAG>\t<inputs>\t<outputs>. Ints decimal. Calls the REAL
// net.minecraft methods (public API: byId, calculateARGBColor, getColorFromPackedId,
// getPackedId). The col/id table and brightness modifiers are read from the public
// final fields directly (col, id, modifier).
//
//   tools/run_groundtruth.ps1 -Tool MapColorParity -Out mcpp/build/map_color.tsv

import net.minecraft.world.level.material.MapColor;

public class MapColorParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // pure value math doesn't require bootstrap; ignore if it fails
        }

        // --- TABLE: dump col/id for every populated MapColor (ids 0..61). ---
        // MapColor.byId(id) returns the registered instance (or NONE for empty
        // slots). col and id are public final ints.
        for (int id = 0; id <= 63; id++) {
            MapColor mc = MapColor.byId(id);
            O.println("TABLE\t" + id + "\t" + mc.id + "\t" + mc.col);
        }

        // --- BRIGHTNESS: dump id/modifier for every Brightness (ids 0..3). ---
        for (int bid = 0; bid <= 3; bid++) {
            MapColor.Brightness b = MapColor.Brightness.byId(bid);
            O.println("BRIGHTNESS\t" + bid + "\t" + b.id + "\t" + b.modifier);
        }

        // --- CALC: calculateARGBColor(brightness) for every (id, brightness). ---
        for (int id = 0; id <= 63; id++) {
            MapColor mc = MapColor.byId(id);
            for (int bid = 0; bid <= 3; bid++) {
                MapColor.Brightness b = MapColor.Brightness.byId(bid);
                int argb = mc.calculateARGBColor(b);
                O.println("CALC\t" + id + "\t" + bid + "\t" + argb);
            }
        }

        // --- PACKED: getColorFromPackedId(packedId) for every byte value. ---
        for (int p = 0; p <= 255; p++) {
            int argb = MapColor.getColorFromPackedId(p);
            O.println("PACKED\t" + p + "\t" + argb);
        }
        // a few values beyond a byte to exercise the & 0xFF masking
        int[] extraPacked = { 256, 257, -1, -128, 0x1FF, 0x300, 0xFF00, -256 };
        for (int p : extraPacked) {
            int argb = MapColor.getColorFromPackedId(p);
            O.println("PACKED\t" + p + "\t" + argb);
        }

        // --- GETPACKED: getPackedId(brightness) for every (id, brightness). ---
        // Returns a signed byte; emit as a decimal int (sign-preserved).
        for (int id = 0; id <= 63; id++) {
            MapColor mc = MapColor.byId(id);
            for (int bid = 0; bid <= 3; bid++) {
                MapColor.Brightness b = MapColor.Brightness.byId(bid);
                byte packed = mc.getPackedId(b);
                O.println("GETPACKED\t" + id + "\t" + bid + "\t" + ((int) packed));
            }
        }
    }
}
