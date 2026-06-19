// Ground-truth generator for net.minecraft.world.entity.Entity.collideWithShapes (the
// axis-ordered AABB-vs-VoxelShape collision slide — the movement workhorse). Private
// static, reached via reflection. The C++ port (world/entity/EntityCollision.h) must
// match bit-for-bit (doubles as raw IEEE-754 bits). Needs Bootstrap (Entity loads
// registries); O is captured at class load before that.
//
//   tools/run_groundtruth.ps1 -Tool EntityCollisionParity -Out mcpp/build/entity_collision.tsv

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.world.phys.shapes.VoxelShape;

public class EntityCollisionParity {
    static final java.io.PrintStream O = System.out;
    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    // Each scenario: entity AABB, movement, and a set of collider boxes.
    static final double[][] MOVES = {
        {0,-1,0}, {0,-0.1,0}, {0.5,0,0}, {0,0,0.5}, {0.3,-0.4,0.2}, {-0.5,-0.5,-0.5},
        {1.2,0,0}, {0,0,-1.2}, {0.05,-0.08,0.05}, {-0.2,0.6,0.9}
    };

    static List<VoxelShape> boxes(double[][] bs) {
        List<VoxelShape> list = new ArrayList<>();
        for (double[] b : bs) list.add(Shapes.create(new AABB(b[0], b[1], b[2], b[3], b[4], b[5])));
        return list;
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Method m = Entity.class.getDeclaredMethod("collideWithShapes", Vec3.class, AABB.class, List.class);
        m.setAccessible(true);

        // entity box ~ a player standing just above a floor block, plus side walls.
        double[][] entities = {
            {-0.3, 0.0, -0.3, 0.3, 1.8, 0.3},     // player-ish at origin
            {0.2, 1.01, 0.2, 0.8, 2.81, 0.8},     // offset, above a block
        };
        double[][][] shapeSets = {
            { {0,-1,0, 1,0,1} },                                  // floor below
            { {-1,-1,-1, 2,0,2} },                                // wide floor
            { {0,0,0, 1,1,1}, {1,0,0, 2,1,1} },                   // two adjacent blocks (wall)
            { {-1,0,-1, 0,2,2}, {0,-1,0, 1,0,1} },                // wall + floor
            { },                                                  // empty -> movement returned as-is
        };

        for (double[] ent : entities) {
            AABB box = new AABB(ent[0], ent[1], ent[2], ent[3], ent[4], ent[5]);
            for (double[][] ss : shapeSets) {
                List<VoxelShape> shapes = boxes(ss);
                for (double[] mv : MOVES) {
                    Vec3 movement = new Vec3(mv[0], mv[1], mv[2]);
                    Vec3 r = (Vec3) m.invoke(null, movement, box, shapes);
                    StringBuilder sb = new StringBuilder("COLLIDE");
                    for (double e : ent) sb.append('\t').append(d(e));
                    sb.append('\t').append(d(mv[0])).append('\t').append(d(mv[1])).append('\t').append(d(mv[2]));
                    sb.append('\t').append(ss.length);
                    for (double[] b : ss) for (double bv : b) sb.append('\t').append(d(bv));
                    sb.append('\t').append(d(r.x)).append('\t').append(d(r.y)).append('\t').append(d(r.z));
                    O.println(sb.toString());
                }
            }
        }
    }
}
