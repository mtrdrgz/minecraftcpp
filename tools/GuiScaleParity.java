// Ground-truth generator for com.mojang.blaze3d.platform.Window#calculateScale
// (Minecraft Java Edition 26.1.2).
//
// Window#calculateScale is a public INSTANCE method that reads the live
// instance fields framebufferWidth / framebufferHeight. Constructing a Window
// requires an initialized GLFW context (a real OS window + GL), which is not
// available headless and cannot be faked via reflection. Therefore this
// harness COPIES THE EXACT METHOD BODY verbatim from
//   26.1.2/src/com/mojang/blaze3d/platform/Window.java  (lines 445-463)
// substituting the two instance fields with the method parameters
// framebufferWidth / framebufferHeight. Nothing else is changed: same
// conditions, same order, same int arithmetic. The C++ port in GuiScale.h is
// certified bit-for-bit against the rows this emits.
//
// Emitted row format (tab-separated):
//   SCALE\t<framebufferWidth>\t<framebufferHeight>\t<maxScale>\t<enforceUnicode 0|1>\t<result>

public class GuiScaleParity {
    static final java.io.PrintStream O = System.out;

    // VERBATIM copy of Window#calculateScale's body (fields -> params).
    static int calculateScale(int framebufferWidth, int framebufferHeight, int maxScale, boolean enforceUnicode) {
        int scale = 1;

        while (
            scale != maxScale
                && scale < framebufferWidth
                && scale < framebufferHeight
                && framebufferWidth / (scale + 1) >= 320
                && framebufferHeight / (scale + 1) >= 240
        ) {
            scale++;
        }

        if (enforceUnicode && scale % 2 != 0) {
            scale++;
        }

        return scale;
    }

    static void emit(int w, int h, int maxScale, boolean uni) {
        int r = calculateScale(w, h, maxScale, uni);
        O.println("SCALE\t" + w + "\t" + h + "\t" + maxScale + "\t" + (uni ? 1 : 0) + "\t" + r);
    }

    public static void main(String[] args) throws Exception {
        // Representative framebuffer sizes: tiny, exactly-threshold, common
        // monitor resolutions, ultrawide, 4K/8K, and asymmetric shapes.
        int[] widths = {
            0, 1, 2, 3, 319, 320, 321, 639, 640, 641, 854, 960, 1024,
            1279, 1280, 1281, 1366, 1440, 1600, 1920, 2048, 2560, 3440,
            3840, 5120, 7680, 15360, 100000
        };
        int[] heights = {
            0, 1, 2, 3, 239, 240, 241, 479, 480, 481, 540, 600, 720,
            767, 768, 769, 800, 900, 1080, 1200, 1440, 2160, 4320, 100000
        };
        // guiScale option values: 0 (auto), then 1..8 plus the engine's max sentinel.
        int[] maxScales = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 16, 2147483646, 2147483647 };

        for (int w : widths) {
            for (int h : heights) {
                for (int m : maxScales) {
                    emit(w, h, m, false);
                    emit(w, h, m, true);
                }
            }
        }

        // A few explicit canonical scenarios for clarity / regression anchoring.
        emit(854, 480, 0, false);    // default small window, auto -> 1
        emit(1920, 1080, 0, false);  // 1080p auto
        emit(3840, 2160, 0, false);  // 4K auto
        emit(7680, 4320, 0, false);  // 8K auto
        emit(1920, 1080, 0, true);   // 1080p auto + forceUnicode
        emit(1920, 1080, 3, true);   // clamped to 3, odd -> unicode bumps to 4
        emit(1920, 1080, 2, false);  // explicit 2
        emit(640, 480, 0, true);     // exactly threshold, unicode
    }
}
