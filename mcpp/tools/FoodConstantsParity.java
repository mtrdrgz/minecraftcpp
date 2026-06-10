// Ground-truth generator for net.minecraft.world.food.FoodConstants (26.1.2) — a pure
// data/constants class. We read EVERY public static final field reflectively out of the
// REAL class (so the value is the genuine jar's, never re-typed here) and dump it, then
// exercise the one pure method saturationByModifier(int,float) via reflection over a grid
// of finite/physical inputs.
//
// ROW SCHEMAS (tab-separated, captured from STDOUT into food_constants.tsv):
//
//   ICONST\t<NAME>\t<value:int decimal>
//   FCONST\t<NAME>\t<bits:%08x of Float.floatToRawIntBits>
//   SATMOD\t<nutrition:int decimal>\t<modifier:%08x>\t<result:%08x>
//
// The C++ side compares its own FoodConstants:: members (int decimal / float bits) and its
// own saturationByModifier against these rows, bit-for-bit.
//
//   tools/run_groundtruth.ps1 -Tool FoodConstantsParity -Out mcpp/build/food_constants.tsv

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import net.minecraft.world.food.FoodConstants;

public class FoodConstantsParity {
    static final java.io.PrintStream O = System.out;
    static String fb(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // finite/physical inputs for saturationByModifier(nutrition, modifier).
    // nutrition: real food item nutrition values + edge/over/negative cases.
    static final int[] NUTR = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 20, -1, -5, 100, -100 };
    // modifier: the actual FoodProperties saturation modifiers seen in vanilla data
    // (FoodConstants.FOOD_SATURATION_*), plus 0, negatives, and a fraction.
    static final float[] MOD = { 0.0f, 0.1f, 0.3f, 0.6f, 0.8f, 1.0f, 1.2f, 0.5f, 2.0f,
                                 -0.3f, -1.0f, 0.3333333f };

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Dump every public static final constant from the real class, by reflection, so
        // each name->value pair is the genuine jar's. int fields -> ICONST, float -> FCONST.
        for (Field f : FoodConstants.class.getDeclaredFields()) {
            int m = f.getModifiers();
            if (!Modifier.isStatic(m) || !Modifier.isFinal(m)) continue;
            f.setAccessible(true);
            Class<?> t = f.getType();
            if (t == int.class) {
                O.println("ICONST\t" + f.getName() + "\t" + f.getInt(null));
            } else if (t == float.class) {
                O.println("FCONST\t" + f.getName() + "\t" + fb(f.getFloat(null)));
            } else {
                // Unexpected constant type — surface it so the gate notices (no silent skip).
                O.println("OTHER\t" + f.getName() + "\t" + t.getName());
            }
        }

        // The pure method saturationByModifier(int,float) — call the REAL static method.
        Method sat = FoodConstants.class.getDeclaredMethod(
                "saturationByModifier", int.class, float.class);
        sat.setAccessible(true);
        for (int n : NUTR) {
            for (float mod : MOD) {
                float r = (Float) sat.invoke(null, n, mod);
                O.println("SATMOD\t" + n + "\t" + fb(mod) + "\t" + fb(r));
            }
        }
    }
}
