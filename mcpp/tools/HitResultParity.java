// Ground-truth generator for net.minecraft.world.phys.HitResult / BlockHitResult /
// EntityHitResult (Minecraft Java Edition 26.1.2). Pure geometry — no Bootstrap
// needed (HitResult/BlockHitResult/EntityHitResult load without registries, and
// BlockPos/Direction/Vec3 are plain data). The C++ port (world/phys/HitResult.h)
// must match bit-for-bit (doubles/floats as raw IEEE-754 bits).
//
//   tools/run_groundtruth.ps1 -Tool HitResultParity -Out mcpp/build/hit_result.tsv
//
// TSV columns (all tab-separated; doubles as %016x bits, ints/booleans as decimal):
//   DISTANCETO  locX locY locZ  posX posY posZ                       -> distSqr(double)
//   GETLOCATION locX locY locZ  dirOrd  bx by bz  inside  wbh        -> outX outY outZ (block ctor)
//   GETBLOCKPOS locX locY locZ  dirOrd  bx by bz  inside  wbh        -> outBx outBy outBz
//   GETDIRECTION ...                                                 -> dirOrd
//   GETTYPE      ... + missFlag                                      -> typeOrd
//   ISINSIDE     ...                                                 -> inside(0/1)
//   ISWBH        ...                                                 -> worldBorderHit(0/1)
//   MISS        locX locY locZ dirOrd bx by bz                       -> typeOrd dirOrd outBx..  loc..  inside wbh
//   WITHDIR     ... newDirOrd                                        -> typeOrd newDir bx.. loc.. inside wbh
//   WITHPOS     ... nbx nby nbz                                      -> typeOrd dir nbx.. loc.. inside wbh
//   HITBORDER   ...                                                  -> typeOrd dir bx.. loc.. inside wbh
//   ENTITY_GETLOC / ENTITY_GETTYPE  locX locY locZ                   -> outX outY outZ / typeOrd

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.HitResult;
import net.minecraft.world.phys.Vec3;

public class HitResultParity {
    static final java.io.PrintStream O = System.out;
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static String v3(Vec3 v) { return d(v.x) + "\t" + d(v.y) + "\t" + d(v.z); }
    static String bp(BlockPos p) { return p.getX() + "\t" + p.getY() + "\t" + p.getZ(); }

    static final double[][] LOCS = {
        {0, 0, 0}, {1, 2, 3}, {-1.5, 0.5, 2.25}, {10.5, -20.25, 30.0},
        {0.1, 0.2, 0.3}, {-3.7, 8.2, -0.001}, {100.0, 0.0, -100.0},
        {0.5, 0.5, 0.5}, {-0.0001, 0.0, 1.0E6}, {2.5, -2.5, 2.5},
        {1234.5, 6789.25, -4321.75}, {-0.0, 0.0, -0.0}
    };
    static final int[][] BPS = {
        {0, 0, 0}, {1, 2, 3}, {-1, -2, -3}, {16, 64, -16},
        {-128, 320, 128}, {7, -7, 7}, {1000000, -64, -1000000}, {15, 15, 15}
    };

    public static void main(String[] args) throws Exception {
        Direction[] DIRS = Direction.values();

        // HitResult.distanceTo geometry (location.x - pos.x …; sum of squares).
        // Constructed as the public BlockHitResult ctor so we call the real
        // inherited HitResult.distanceTo body — but it needs an Entity, so we
        // replicate the exact body (xd*xd+yd*yd+zd*zd) verbatim here against a
        // Vec3 'position'. See notes.
        for (double[] a : LOCS) {
            for (double[] b : LOCS) {
                Vec3 loc = new Vec3(a[0], a[1], a[2]);
                Vec3 pos = new Vec3(b[0], b[1], b[2]);
                double xd = loc.x - pos.x;
                double yd = loc.y - pos.y;
                double zd = loc.z - pos.z;
                double distSqr = xd * xd + yd * yd + zd * zd;
                O.println("DISTANCETO\t" + v3(loc) + "\t" + v3(pos) + "\t" + d(distSqr));
            }
        }

        // BlockHitResult full surface.
        for (double[] a : LOCS) {
            Vec3 loc = new Vec3(a[0], a[1], a[2]);
            for (int[] b : BPS) {
                BlockPos pos = new BlockPos(b[0], b[1], b[2]);
                for (Direction dir : DIRS) {
                    for (int insideI = 0; insideI <= 1; insideI++) {
                        boolean inside = insideI == 1;
                        for (int wbhI = 0; wbhI <= 1; wbhI++) {
                            boolean wbh = wbhI == 1;
                            BlockHitResult r = new BlockHitResult(loc, dir, pos, inside, wbh);
                            String in = v3(loc) + "\t" + dir.ordinal() + "\t" + bp(pos) + "\t"
                                      + (inside ? 1 : 0) + "\t" + (wbh ? 1 : 0);

                            O.println("GETLOCATION\t" + in + "\t" + v3(r.getLocation()));
                            O.println("GETBLOCKPOS\t" + in + "\t" + bp(r.getBlockPos()));
                            O.println("GETDIRECTION\t" + in + "\t" + r.getDirection().ordinal());
                            O.println("GETTYPE\t" + in + "\t" + r.getType().ordinal());
                            O.println("ISINSIDE\t" + in + "\t" + (r.isInside() ? 1 : 0));
                            O.println("ISWBH\t" + in + "\t" + (r.isWorldBorderHit() ? 1 : 0));

                            // withDirection — recompute over every direction.
                            for (Direction nd : DIRS) {
                                BlockHitResult wd = r.withDirection(nd);
                                O.println("WITHDIR\t" + in + "\t" + nd.ordinal() + "\t"
                                    + wd.getType().ordinal() + "\t" + wd.getDirection().ordinal() + "\t"
                                    + bp(wd.getBlockPos()) + "\t" + v3(wd.getLocation()) + "\t"
                                    + (wd.isInside() ? 1 : 0) + "\t" + (wd.isWorldBorderHit() ? 1 : 0));
                            }
                            // withPosition — recompute over a few new positions.
                            for (int[] nb : BPS) {
                                BlockPos np = new BlockPos(nb[0], nb[1], nb[2]);
                                BlockHitResult wp = r.withPosition(np);
                                O.println("WITHPOS\t" + in + "\t" + bp(np) + "\t"
                                    + wp.getType().ordinal() + "\t" + wp.getDirection().ordinal() + "\t"
                                    + bp(wp.getBlockPos()) + "\t" + v3(wp.getLocation()) + "\t"
                                    + (wp.isInside() ? 1 : 0) + "\t" + (wp.isWorldBorderHit() ? 1 : 0));
                            }
                            // hitBorder.
                            BlockHitResult hb = r.hitBorder();
                            O.println("HITBORDER\t" + in + "\t"
                                + hb.getType().ordinal() + "\t" + hb.getDirection().ordinal() + "\t"
                                + bp(hb.getBlockPos()) + "\t" + v3(hb.getLocation()) + "\t"
                                + (hb.isInside() ? 1 : 0) + "\t" + (hb.isWorldBorderHit() ? 1 : 0));
                        }
                    }
                }
            }
        }

        // BlockHitResult.miss(Vec3, Direction, BlockPos).
        for (double[] a : LOCS) {
            Vec3 loc = new Vec3(a[0], a[1], a[2]);
            for (int[] b : BPS) {
                BlockPos pos = new BlockPos(b[0], b[1], b[2]);
                for (Direction dir : DIRS) {
                    BlockHitResult r = BlockHitResult.miss(loc, dir, pos);
                    String in = v3(loc) + "\t" + dir.ordinal() + "\t" + bp(pos);
                    O.println("MISS\t" + in + "\t"
                        + r.getType().ordinal() + "\t" + r.getDirection().ordinal() + "\t"
                        + bp(r.getBlockPos()) + "\t" + v3(r.getLocation()) + "\t"
                        + (r.isInside() ? 1 : 0) + "\t" + (r.isWorldBorderHit() ? 1 : 0));
                }
            }
        }

        // EntityHitResult — only the location-carrying surface (getLocation, getType).
        // Constructed via the (Entity, Vec3) ctor with a null Entity since we never
        // touch the entity; getLocation/getType ignore it.
        for (double[] a : LOCS) {
            Vec3 loc = new Vec3(a[0], a[1], a[2]);
            net.minecraft.world.phys.EntityHitResult e =
                new net.minecraft.world.phys.EntityHitResult(null, loc);
            O.println("ENTITY_GETLOC\t" + v3(loc) + "\t" + v3(e.getLocation()));
            O.println("ENTITY_GETTYPE\t" + v3(loc) + "\t" + e.getType().ordinal());
        }
    }
}
