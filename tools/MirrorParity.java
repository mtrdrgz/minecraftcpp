// Ground-truth generator for net.minecraft.world.level.block.Mirror using the
// REAL decompiled 26.1.2 class. Mirror is a pure StringRepresentable enum:
//   NONE("none"), LEFT_RIGHT("left_right"), FRONT_BACK("front_back")
//   int      mirror(int rotation, int steps)   (Mirror.java:28-39)
//   Rotation getRotation(Direction value)       (Mirror.java:41-44)
//   Direction mirror(Direction direction)       (Mirror.java:46-52)
//   String   getSerializedName()                (Mirror.java:62-65, returns id)
//   plus ordinal()/name() per constant.
//
// All exercised methods are public + side-effect-free (no registries/world), so
// no Bootstrap is needed. The C++ test (MirrorParityTest) recomputes each row
// from the same inputs (via the certified StructureTransforms.h Mirror port plus
// MirrorSerializedName.h for the id strings) and must match exactly.
//
//   tools/run_groundtruth.ps1 -Tool MirrorParity -Out mcpp/build/mirror.tsv
//
// Enum ordinals are exchanged as ints (match the C++ enums):
//   Mirror:    NONE=0, LEFT_RIGHT=1, FRONT_BACK=2
//   Rotation:  NONE=0, CLOCKWISE_90=1, CLOCKWISE_180=2, COUNTERCLOCKWISE_90=3
//   Direction: DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5

import net.minecraft.core.Direction;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;

public class MirrorParity {
    static final java.io.PrintStream O = System.out;

    static final Mirror[] MIRS = { Mirror.NONE, Mirror.LEFT_RIGHT, Mirror.FRONT_BACK };
    static final Rotation[] ROTS = { Rotation.NONE, Rotation.CLOCKWISE_90, Rotation.CLOCKWISE_180, Rotation.COUNTERCLOCKWISE_90 };
    static final Direction[] DIRS = { Direction.DOWN, Direction.UP, Direction.NORTH, Direction.SOUTH, Direction.WEST, Direction.EAST };

    static int mi(Mirror m) { return java.util.Arrays.asList(MIRS).indexOf(m); }
    static int ri(Rotation r) { return java.util.Arrays.asList(ROTS).indexOf(r); }
    static int di(Direction d) { return d.ordinal(); }

    public static void main(String[] args) throws Exception {
        // Per-constant enum metadata: ordinal()/name()/getSerializedName().
        for (Mirror m : MIRS) {
            O.println("MIR_META\t" + mi(m) + "\t" + m.ordinal() + "\t" + m.name() + "\t" + m.getSerializedName());
        }

        // Mirror.mirror(int rotation, int steps) — the int-rotation overload.
        // steps is the period; rotation ranges over a full period (the only
        // physically meaningful inputs to this 1:1 rotation-index helper).
        for (Mirror m : MIRS)
            for (int steps : new int[] { 4, 8, 16, 90, 360 })
                for (int rot = 0; rot < steps; rot++)
                    O.println("MIR_INT\t" + mi(m) + "\t" + rot + "\t" + steps + "\t" + m.mirror(rot, steps));

        // Mirror.getRotation(Direction) and Mirror.mirror(Direction).
        for (Mirror m : MIRS)
            for (Direction d : DIRS) {
                O.println("MIR_GETROT\t" + mi(m) + "\t" + di(d) + "\t" + ri(m.getRotation(d)));
                O.println("MIR_DIR\t" + mi(m) + "\t" + di(d) + "\t" + di(m.mirror(d)));
            }
    }
}
