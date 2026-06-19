// Ground-truth generator for the GUI-scale math in
// com.mojang.blaze3d.platform.Window (Minecraft 26.1.2), driving the REAL class.
//
//   tools/run_groundtruth.ps1 -Tool WindowGuiScaleParity -Out mcpp/build/window_gui_scale.tsv
//
// Window.calculateScale(maxScale, enforceUnicode) and Window.setGuiScale(guiScale)
// are pure deterministic int/double arithmetic over the framebuffer dimensions;
// neither touches GLFW/GL/native state. We never reimplement either body Java-side:
// we instantiate a REAL Window WITHOUT running its GLFW constructor (via
// sun.misc.Unsafe.allocateInstance, accessed PURELY REFLECTIVELY so there is no
// `import sun.misc.Unsafe` and javac stays silent), set the framebuffer size with
// the public setWidth/setHeight setters, then invoke the real calculateScale /
// setGuiScale and read back the real outputs through the public getters.
//
// Rows (tab-separated, leading TAG):
//   SCALE  <fbW> <fbH> <maxScale> <enforceUnicode0/1>  <calculateScale>
//   GUI    <fbW> <fbH> <guiScale>                       <guiScaledWidth> <guiScaledHeight>
//
// All values are decimal ints (the math is integer-valued; the intermediate
// double in setGuiScale only affects the integer result).

import com.mojang.blaze3d.platform.Window;

import java.lang.reflect.Method;

public class WindowGuiScaleParity {
    static final java.io.PrintStream O = System.out;

    // Reflectively obtain sun.misc.Unsafe and allocate a Window without invoking
    // its (GLFW-touching) constructor. No `import sun.misc.Unsafe`.
    static Object UNSAFE;
    static Method M_allocateInstance;

    static void resolveUnsafe() throws Exception {
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        java.lang.reflect.Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        UNSAFE = theUnsafe.get(null);
        M_allocateInstance = unsafeClass.getMethod("allocateInstance", Class.class);
    }

    static Window newWindow() throws Exception {
        return (Window) M_allocateInstance.invoke(UNSAFE, Window.class);
    }

    static Window windowWithFramebuffer(int fbW, int fbH) throws Exception {
        Window w = newWindow();
        // public setters write framebufferWidth / framebufferHeight directly.
        w.setWidth(fbW);
        w.setHeight(fbH);
        return w;
    }

    public static void main(String[] args) throws Exception {
        resolveUnsafe();

        // Framebuffer sizes spanning tiny, sub-base, common 16:9 / 16:10 / 4:3
        // resolutions, ultra-wide, and non-divisible oddballs that exercise the
        // int-division guards and the setGuiScale ceiling.
        int[][] fbs = {
            {1, 1}, {2, 2}, {16, 16}, {100, 100},
            {320, 240}, {321, 241}, {319, 480}, {480, 319},
            {640, 360}, {640, 480}, {854, 480}, {960, 540},
            {1024, 768}, {1280, 720}, {1280, 800}, {1280, 1024},
            {1366, 768}, {1440, 900}, {1600, 900}, {1680, 1050},
            {1920, 1080}, {1920, 1200}, {2048, 1080}, {2560, 1080},
            {2560, 1440}, {3440, 1440}, {3840, 2160}, {5120, 1440},
            {7680, 4320}, {333, 777}, {1001, 999}, {12345, 6789},
            {17, 4096}, {4096, 17}, {640, 12000}, {12000, 640},
        };

        // maxScale candidates: the special "unlimited" 0/-1 paths, plus the small
        // values the options screen actually allows, plus an over-large cap.
        int[] maxScales = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 1000};

        for (int[] fb : fbs) {
            int fbW = fb[0], fbH = fb[1];
            Window w = windowWithFramebuffer(fbW, fbH);

            for (int maxScale : maxScales) {
                for (int eu = 0; eu <= 1; eu++) {
                    boolean enforceUnicode = eu != 0;
                    int scale = w.calculateScale(maxScale, enforceUnicode);
                    O.println("SCALE\t" + fbW + "\t" + fbH + "\t" + maxScale + "\t" + eu + "\t" + scale);
                }
            }

            // setGuiScale over the full range of plausible scales (1..16) plus a
            // couple of extremes; reads back the real guiScaled{Width,Height}.
            int[] guiScales = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 32};
            for (int gs : guiScales) {
                w.setGuiScale(gs);
                O.println("GUI\t" + fbW + "\t" + fbH + "\t" + gs
                          + "\t" + w.getGuiScaledWidth() + "\t" + w.getGuiScaledHeight());
            }
        }
    }
}
