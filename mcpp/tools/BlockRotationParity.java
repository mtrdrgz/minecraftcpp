// Ground-truth generator for net.minecraft.world.level.block.Rotation.
//
// Emits tab-separated rows to STDOUT (captured into rotation_block.tsv):
//
//   META   <ordinal>  <name>  <getSerializedName>  <getIndex(reflected)>
//   ROTATED <selfOrd> <rotOrd> <resultOrd>            Rotation.getRotated(Rotation)
//   RDIR    <selfOrd> <dirOrd> <resultDirOrd>         Rotation.rotate(Direction)
//   RINT    <selfOrd> <rotation> <steps> <result>     Rotation.rotate(int,int)
//
// All inputs are finite/physical. Direction ordinals: DOWN=0,UP=1,NORTH=2,
// SOUTH=3,WEST=4,EAST=5. rotate(int,int) is only called with steps that are
// positive multiples of 4 (the vanilla callers always pass a divisor-friendly
// step count); negative/zero steps would divide-by-zero or be nonsensical.
//
// Calls the REAL net.minecraft Rotation enum. getIndex() is private, so it is
// read via reflection (setAccessible). No registries/world needed; bootstrap is
// added defensively in case any static initializer trips "Not bootstrapped".

import java.io.PrintStream;
import java.lang.reflect.Method;

import net.minecraft.core.Direction;
import net.minecraft.world.level.block.Rotation;

public class BlockRotationParity {
    static final PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        final Rotation[] rots = Rotation.values();
        final Direction[] dirs = Direction.values();

        // getIndex() is private — reflect it.
        Method getIndex = Rotation.class.getDeclaredMethod("getIndex");
        getIndex.setAccessible(true);

        // META: ordinal, name, serialized name, private index.
        for (Rotation r : rots) {
            int idx = (Integer) getIndex.invoke(r);
            O.println("META\t" + r.ordinal() + "\t" + r.name() + "\t"
                    + r.getSerializedName() + "\t" + idx);
        }

        // ROTATED: getRotated(Rotation) for every (self, rot) pair.
        for (Rotation self : rots) {
            for (Rotation rot : rots) {
                Rotation res = self.getRotated(rot);
                O.println("ROTATED\t" + self.ordinal() + "\t" + rot.ordinal()
                        + "\t" + res.ordinal());
            }
        }

        // RDIR: rotate(Direction) for every (self, direction) pair.
        for (Rotation self : rots) {
            for (Direction d : dirs) {
                Direction res = self.rotate(d);
                O.println("RDIR\t" + self.ordinal() + "\t" + d.ordinal()
                        + "\t" + res.ordinal());
            }
        }

        // RINT: rotate(int rotation, int steps). steps = positive multiples of 4;
        // rotation swept across [0, steps) plus a few values >= steps.
        int[] stepValues = {4, 8, 12, 16, 360};
        for (Rotation self : rots) {
            for (int steps : stepValues) {
                for (int rotation = 0; rotation < steps + 8; rotation++) {
                    int res = self.rotate(rotation, steps);
                    O.println("RINT\t" + self.ordinal() + "\t" + rotation
                            + "\t" + steps + "\t" + res);
                }
            }
        }
    }
}
