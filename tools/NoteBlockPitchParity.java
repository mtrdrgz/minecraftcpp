// Ground truth for net.minecraft.world.level.block.NoteBlock.getPitchFromNote
// (Minecraft Java Edition 26.1.2).
//
// Exercises the REAL class:
//   - getPitchFromNote(int twoOctaveRangeNote)   NoteBlock.java:143-145
//       return (float)Math.pow(2.0, (twoOctaveRangeNote - 12) / 12.0);
//
// getPitchFromNote is public static, so we resolve it via reflection
// (setAccessible) and invoke it with no instance — NO world/registry/GL
// coupling whatsoever. We still bootstrap SharedConstants/Bootstrap so the
// class loads exactly as it does in-game.
//
// The returned float is emitted as 8-hex raw int bits (Float.floatToRawIntBits)
// so the C++ side compares the EXACT bit pattern — no tolerance.
//
// Battery:
//   * Every note in the canonical two-octave range 0..24 (the NOTE property
//     domain; BlockStateProperties.NOTE is 0..24).
//   * The 16 amethyst-resonance tones SculkSensorBlock feeds through this helper
//     (toneMap = {0,0,2,4,6,7,9,10,12,14,15,18,19,21,22,24}).
//   * Out-of-domain probes (negatives and >24) to exercise the negative-exponent
//     and large-exponent paths of the int subtraction + double division.
//
// Row tag (tab-separated):
//   PITCH   <note>   <pitch8>
import java.lang.reflect.Method;
import java.util.LinkedHashSet;
import net.minecraft.world.level.block.NoteBlock;

public class NoteBlockPitchParity {
    static final java.io.PrintStream O = System.out;

    static String fb(float f) { return String.format("%08x", Float.floatToRawIntBits(f)); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Method getPitch = NoteBlock.class.getMethod("getPitchFromNote", int.class);
        getPitch.setAccessible(true);

        // Use an ordered, de-duplicated set so explicit overlaps (e.g. the
        // resonance toneMap values, which are all within 0..24) are emitted once.
        LinkedHashSet<Integer> notes = new LinkedHashSet<>();

        // Canonical two-octave range 0..24 (BlockStateProperties.NOTE domain).
        for (int n = 0; n <= 24; n++) notes.add(n);

        // The amethyst-resonance toneMap (SculkSensorBlock.RESONANCE_PITCH_BEND).
        int[] toneMap = {0, 0, 2, 4, 6, 7, 9, 10, 12, 14, 15, 18, 19, 21, 22, 24};
        for (int t : toneMap) notes.add(t);

        // Out-of-domain probes: negative notes (negative exponent path) and notes
        // above 24 (large positive exponent), plus a couple of extremes that are
        // still finite under Math.pow.
        int[] extra = {-1, -2, -5, -12, -24, 25, 30, 36, 48, 100, -100};
        for (int e : extra) notes.add(e);

        for (int note : notes) {
            float pitch = (Float) getPitch.invoke(null, note);
            O.println("PITCH\t" + note + "\t" + fb(pitch));
        }
    }
}
