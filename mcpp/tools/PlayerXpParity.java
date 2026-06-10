// Ground-truth generator for two pure XP integer formulas in Minecraft 26.1.2:
//
//   net.minecraft.world.entity.player.Player.getXpNeededForNextLevel()  (instance)
//   net.minecraft.world.entity.ExperienceOrb.getExperienceValue(int)    (static)
//
//   tools/run_groundtruth.ps1 -Tool PlayerXpParity -Out mcpp/build/player_xp.tsv
//
// TSV rows (tab-separated), dispatched by leading TAG in the C++ test:
//   XPNEED <experienceLevel:int> <result:int>
//   XPVAL  <maxValue:int>        <result:int>
//
// REFLECTION NOTE:
//   ExperienceOrb.getExperienceValue(int) is STATIC and is invoked on the REAL
//   class via reflection (Method.setAccessible(true), invoke(null, value)).
//   Player.getXpNeededForNextLevel() is an INSTANCE method that reads only the
//   private field this.experienceLevel; constructing a real Player needs a live
//   Level/World, so its verbatim body (Player.java lines 1553-1559) is REPLICATED
//   below in xpNeeded(int). This is a faithful copy, not an invention — the field
//   read is simply lifted to the parameter. Both sides (Java GT + C++ PlayerXp.h)
//   carry the identical replicated body, so any divergence still surfaces.

import java.lang.reflect.Method;

public class PlayerXpParity {
    static final java.io.PrintStream O = System.out;

    // Verbatim copy of Player.getXpNeededForNextLevel() with this.experienceLevel -> level.
    static int xpNeeded(int level) {
        if (level >= 30) {
            return 112 + (level - 30) * 9;
        } else {
            return level >= 15 ? 37 + (level - 15) * 5 : 7 + level * 2;
        }
    }

    public static void main(String[] args) throws Exception {
        // Some net.minecraft classes trip "Not bootstrapped" at class load.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // If bootstrap is unavailable, the static reflective call below may still work.
        }

        // Real static method on the REAL class.
        Method m = net.minecraft.world.entity.ExperienceOrb.class
                .getDeclaredMethod("getExperienceValue", int.class);
        m.setAccessible(true);

        // ---- XPNEED: getXpNeededForNextLevel(experienceLevel) ----
        // experienceLevel is clamped >= 0 in vanilla, but exercise the whole int
        // range incl. negatives, the 15 and 30 breakpoints, and overflow extremes.
        int[] LEVELS = {
            -2147483648, -1000000, -100, -31, -30, -16, -15, -2, -1,
            0, 1, 2, 3, 7, 13, 14, 15, 16, 17, 21, 28, 29, 30, 31, 32, 40, 50,
            100, 200, 1000, 21474836, 100000000, 238609294, 238609295, 238609296,
            2147483646, 2147483647
        };
        for (int lvl : LEVELS) {
            O.println("XPNEED\t" + lvl + "\t" + xpNeeded(lvl));
        }

        // ---- XPVAL: getExperienceValue(maxValue) ----
        // Hit every bucket boundary (2477,1237,617,307,149,73,37,17,7,3) and the
        // value just below each, plus negatives, zero, and overflow extremes.
        int[] VALUES = {
            -2147483648, -1000000, -100, -3, -2, -1,
            0, 1, 2, 3, 4, 6, 7, 8, 16, 17, 18, 36, 37, 38,
            72, 73, 74, 148, 149, 150, 306, 307, 308, 616, 617, 618,
            1236, 1237, 1238, 2476, 2477, 2478,
            3000, 10000, 65535, 1000000, 2147483646, 2147483647
        };
        for (int v : VALUES) {
            int got = (Integer) m.invoke(null, v);
            O.println("XPVAL\t" + v + "\t" + got);
        }
    }
}
