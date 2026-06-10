import net.minecraft.util.Brightness;

// Focused VERIFY ground truth for the existing engine header mcpp/src/util/Brightness.h.
// Exercises ONLY net.minecraft.util.Brightness's own surface (the record + its
// pack()/unpack() and the FULL_BRIGHT constant) over the full physical 0..15 light
// domain. The LightCoordsUtil bit math underneath is already certified by
// BrightnessParity; this gate pins the Brightness record's behavior itself.
//
// Brightness is a `public record Brightness(int block, int sky)` — block()/sky() are the
// generated record accessors, pack()/unpack() are public, FULL_BRIGHT is a public field.
// All public, so no reflection needed.
//
// Row formats (TAG \t inputs... \t outputs...):
//   PACK     block sky | packed        (new Brightness(block, sky).pack())
//   UNPACK   packed | block sky        (Brightness.unpack(packed).block()/.sky())
//   FULL     block sky packed          (Brightness.FULL_BRIGHT accessors + pack())
public class BrightnessVerifyParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // pack() over the entire valid light domain block,sky in 0..15 (16x16 = 256 rows).
        for (int b = 0; b <= 15; b++) {
            for (int s = 0; s <= 15; s++) {
                Brightness br = new Brightness(b, s);
                O.println("PACK\t" + b + "\t" + s + "\t" + br.pack());
            }
        }

        // unpack(): round-trip every packed value produced by pack() over 0..15, so the
        // recovered block()/sky() must equal the originals; plus the named constants.
        for (int b = 0; b <= 15; b++) {
            for (int s = 0; s <= 15; s++) {
                int packed = new Brightness(b, s).pack();
                Brightness u = Brightness.unpack(packed);
                O.println("UNPACK\t" + packed + "\t" + u.block() + "\t" + u.sky());
            }
        }
        // A few stand-alone packed coords (the FULL_* layout and zeros) for unpack().
        int[] extraPacked = { 0, Brightness.FULL_BRIGHT.pack(), new Brightness(15, 0).pack(),
                              new Brightness(0, 15).pack(), new Brightness(7, 11).pack() };
        for (int packed : extraPacked) {
            Brightness u = Brightness.unpack(packed);
            O.println("UNPACK\t" + packed + "\t" + u.block() + "\t" + u.sky());
        }

        // FULL_BRIGHT constant: block, sky, and its packed value.
        O.println("FULL\t" + Brightness.FULL_BRIGHT.block() + "\t" + Brightness.FULL_BRIGHT.sky()
            + "\t" + Brightness.FULL_BRIGHT.pack());
    }
}
