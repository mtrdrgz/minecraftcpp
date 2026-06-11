// Ground truth for net.minecraft.world.phys.AABB — ray clip (the collision/
// raytrace primitive), expandTowards, contract, distanceToSqr — over a battery of
// edge cases + seeded pseudo-random cases. All doubles are emitted as raw IEEE
// bit hex so the C++ aabb_parity comparison is BIT-exact (no formatting slack).
//
// Per case:
//   IN   <name> <minX minY minZ maxX maxY maxZ fromX fromY fromZ toX toY toZ>  (12 hex doubles)
//   CLIP <name> <0|1> [<hitX hitY hitZ>]
//   EXP  <name> <6 hex>   expandTowards(to-from)
//   CON  <name> <6 hex>   contract(to-from)
//   DST  <name> <1 hex>   distanceToSqr(from)
import java.util.Optional;
import java.util.Random;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;

public class AabbParity {
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) {
        // edge battery
        c("face_hit",      0,0,0, 1,1,1,  -1,0.5,0.5,  2,0.5,0.5);
        c("miss",          0,0,0, 1,1,1,  -1,2.5,0.5,  2,2.5,0.5);
        c("corner_graze",  0,0,0, 1,1,1,  -1,-1,-1,    2,2,2);
        c("edge_graze_eps",0,0,0, 1,1,1,  -1,1.0000000999,0.5, 2,1.0000000999,0.5);
        c("from_inside",   0,0,0, 1,1,1,  0.5,0.5,0.5, 2,0.5,0.5);
        c("to_inside",     0,0,0, 1,1,1,  -1,0.5,0.5,  0.5,0.5,0.5);
        c("parallel_x",    0,0,0, 1,1,1,  -1,0.5,0.5,  -1,2,2);
        c("zero_ray",      0,0,0, 1,1,1,  0.5,0.5,0.5, 0.5,0.5,0.5);
        c("neg_dir",       0,0,0, 1,1,1,  2,0.5,0.5,   -1,0.5,0.5);
        c("thin_slab",     0,0,0, 1,0,1,  0.5,1,0.5,   0.5,-1,0.5);
        c("exact_face_start", 0,0,0, 1,1,1, 0,0.5,0.5, 2,0.5,0.5);
        c("big_coords",    1e6,1e6,1e6, 1e6+1,1e6+1,1e6+1, 1e6-1,1e6+0.5,1e6+0.5, 1e6+2,1e6+0.5,1e6+0.5);

        // Ctor min/max EDGE semantics (Java AABB(...) uses Math.min/Math.max, NOT a `<`
        // ternary): signed zero (min(+0,-0)==-0, max(-0,+0)==+0) + NaN poisoning + infinities.
        // The C++ ternary diverged here; these cases gate the java.lang.Math-exact fix.
        c("signed_zero_x",  0.0,0,0,   -0.0,1,1,    0.5,0.5,0.5, 0.5,2,0.5);
        c("signed_zero_all",0.0,0.0,0.0, -0.0,-0.0,-0.0, -1,-1,-1, 2,2,2);
        c("nan_minx",       Double.NaN,0,0, 1,1,1,   -1,0.5,0.5, 2,0.5,0.5);
        c("nan_maxy",       0,1,0, 1,Double.NaN,1,   -1,0.5,0.5, 2,0.5,0.5);
        c("inf_box",        Double.NEGATIVE_INFINITY,0,0, Double.POSITIVE_INFINITY,1,1, -1,0.5,0.5, 2,0.5,0.5);

        // seeded pseudo-random battery (inputs are EMITTED, so the C++ side
        // needs no RNG — it replays the exact bit-identical inputs)
        Random r = new Random(123456789L);
        for (int i = 0; i < 500; i++) {
            double ax = r.nextDouble() * 16 - 8, ay = r.nextDouble() * 16 - 8, az = r.nextDouble() * 16 - 8;
            double bx = ax + r.nextDouble() * 4, by = ay + r.nextDouble() * 4, bz = az + r.nextDouble() * 4;
            double fx = r.nextDouble() * 32 - 16, fy = r.nextDouble() * 32 - 16, fz = r.nextDouble() * 32 - 16;
            double tx = r.nextDouble() * 32 - 16, ty = r.nextDouble() * 32 - 16, tz = r.nextDouble() * 32 - 16;
            c("rand_" + i, ax, ay, az, bx, by, bz, fx, fy, fz, tx, ty, tz);
        }
        System.out.print(OUT);
    }

    static void c(String name, double minX, double minY, double minZ,
                  double maxX, double maxY, double maxZ,
                  double fx, double fy, double fz, double tx, double ty, double tz) {
        AABB box = new AABB(minX, minY, minZ, maxX, maxY, maxZ);
        Vec3 from = new Vec3(fx, fy, fz), to = new Vec3(tx, ty, tz);
        OUT.append("IN\t").append(name).append('\t');
        hex(minX); hex(minY); hex(minZ); hex(maxX); hex(maxY); hex(maxZ);
        hex(fx); hex(fy); hex(fz); hex(tx); hex(ty); hex(tz);
        OUT.append('\n');

        Optional<Vec3> hit = box.clip(from, to);
        OUT.append("CLIP\t").append(name).append('\t').append(hit.isPresent() ? 1 : 0).append('\t');
        if (hit.isPresent()) { hex(hit.get().x); hex(hit.get().y); hex(hit.get().z); }
        OUT.append('\n');

        AABB exp = box.expandTowards(to.subtract(from));
        OUT.append("EXP\t").append(name).append('\t');
        hex(exp.minX); hex(exp.minY); hex(exp.minZ); hex(exp.maxX); hex(exp.maxY); hex(exp.maxZ);
        OUT.append('\n');

        AABB con = box.contract(tx - fx, ty - fy, tz - fz);
        OUT.append("CON\t").append(name).append('\t');
        hex(con.minX); hex(con.minY); hex(con.minZ); hex(con.maxX); hex(con.maxY); hex(con.maxZ);
        OUT.append('\n');

        OUT.append("DST\t").append(name).append('\t');
        hex(box.distanceToSqr(from));
        OUT.append('\n');

        // Extended surface (all already implemented in the C++ AABB; previously ungated).
        AABB box2 = new AABB(from, to);
        Vec3 ctr = box.getCenter();
        OUT.append("CENTER\t").append(name).append('\t'); hex(ctr.x); hex(ctr.y); hex(ctr.z); OUT.append('\n');
        Vec3 bctr = box.getBottomCenter();
        OUT.append("BOTCENTER\t").append(name).append('\t'); hex(bctr.x); hex(bctr.y); hex(bctr.z); OUT.append('\n');
        OUT.append("SIZES\t").append(name).append('\t'); hex(box.getSize()); hex(box.getXsize()); hex(box.getYsize()); hex(box.getZsize()); OUT.append('\n');
        OUT.append("AXIS\t").append(name).append('\t');
        hex(box.min(Direction.Axis.X)); hex(box.min(Direction.Axis.Y)); hex(box.min(Direction.Axis.Z));
        hex(box.max(Direction.Axis.X)); hex(box.max(Direction.Axis.Y)); hex(box.max(Direction.Axis.Z)); OUT.append('\n');
        AABB inf = box.inflate(0.25);
        OUT.append("INFLATE\t").append(name).append('\t'); hex(inf.minX); hex(inf.minY); hex(inf.minZ); hex(inf.maxX); hex(inf.maxY); hex(inf.maxZ); OUT.append('\n');
        AABB def = box.deflate(0.1);
        OUT.append("DEFLATE\t").append(name).append('\t'); hex(def.minX); hex(def.minY); hex(def.minZ); hex(def.maxX); hex(def.maxY); hex(def.maxZ); OUT.append('\n');
        AABB it = box.intersect(box2);
        OUT.append("INTERSECT\t").append(name).append('\t'); hex(it.minX); hex(it.minY); hex(it.minZ); hex(it.maxX); hex(it.maxY); hex(it.maxZ); OUT.append('\n');
        AABB mm = box.minmax(box2);
        OUT.append("MINMAX\t").append(name).append('\t'); hex(mm.minX); hex(mm.minY); hex(mm.minZ); hex(mm.maxX); hex(mm.maxY); hex(mm.maxZ); OUT.append('\n');
        OUT.append("INTERSECTS\t").append(name).append('\t').append(box.intersects(box2) ? 1 : 0).append('\n');
        OUT.append("CONTAINS\t").append(name).append('\t').append(box.contains(from) ? 1 : 0).append('\n');
        OUT.append("HASNAN\t").append(name).append('\t').append(box.hasNaN() ? 1 : 0).append('\n');
    }

    static void hex(double d) {
        OUT.append(String.format("%016x ", Double.doubleToRawLongBits(d)));
    }
}
