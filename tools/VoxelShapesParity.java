// Ground truth for net.minecraft.world.phys.shapes.* — the voxel-shape +
// collision foundation (VoxelShape/Shapes/BitSetDiscreteVoxelShape/IndexMerger
// family/SliceShape). Emits a construction PROGRAM (DEF lines) plus expected
// observations; the C++ voxel_shapes_parity gate replays the same program and
// compares every observation BIT-exactly (all doubles as raw IEEE-754 bit hex).
// Seeded randomness stays Java-side: the C++ replays exact emitted inputs.
//
// Line grammar (space-separated; doubles are 16-digit raw bit hex):
//   DEF <name> EMPTY | BLOCK | BOX <6> | CREATE <6>
//   DEF <name> JOIN <a> <b> <OP>      (Shapes.join — optimized)
//   DEF <name> JOINU <a> <b> <OP>     (Shapes.joinUnoptimized)
//   DEF <name> MOVE <a> <3>
//   DEF <name> OPT <a>                (a.optimize())
//   DEF <name> FACE <a> <dirOrdinal>  (a.getFaceShape(dir))
//   EMPTYQ <name> <0|1>
//   COORDS <name> <nx> <nx hex> <ny> <ny hex> <nz> <nz hex>
//   BOUNDS <name> <6>                 (only for non-empty shapes)
//   AABBS <name> <n> <6n>
//   CLIP <name> <from 3> <to 3> <posX posY posZ(ints)> <0|1> [<loc 3> <dirOrd> <inside01>]
//   COLLIDE <name> <axisOrd> <box 6> <dist> <result>
//   COLLIDEN <axisOrd> <box 6> <dist> <k> <k names> <result>   (Shapes.collide)
//   JOINNE <a> <b> <OP> <0|1>         (Shapes.joinIsNotEmpty)
//   OCCB <a> <b> <dirOrd> <0|1>       (Shapes.blockOccludes)
//   OCCM <a> <b> <dirOrd> <0|1>       (Shapes.mergedFaceOccludes)
//   OCCF <a> <b> <0|1>                (Shapes.faceShapeOccludes)
//   EQUALQ <a> <b> <0|1>              (Shapes.equal)
//   MINMAX <name> <axisOrd> <b> <c> <min> <max>   (VoxelShape.min/max(axis,b,c))
//   CLOSEST <name> <point 3> <0|1> [<3>]
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Random;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
import net.minecraft.world.phys.shapes.BooleanOp;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.world.phys.shapes.VoxelShape;

public class VoxelShapesParity {
    static final StringBuilder OUT = new StringBuilder();
    // captured before Bootstrap wraps System.out into the log4j console appender
    // (which would prefix every emitted line with "[INFO]: ")
    static final java.io.PrintStream REAL_OUT = System.out;
    static final LinkedHashMap<String, VoxelShape> SHAPES = new LinkedHashMap<>();
    static final List<String> NAMES = new ArrayList<>();
    static final String[] OPS = {"OR", "AND", "ONLY_FIRST", "ONLY_SECOND", "NOT_SAME"};

    public static void main(String[] args) {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();   // Shapes.block()/registry-backed shape init
        // ── deterministic battery ────────────────────────────────────────────
        defEmpty("empty");
        defBlock("block");
        defBox("slab_bottom", 0, 0, 0, 1, 0.5, 1);
        defBox("slab_top", 0, 0.5, 0, 1, 1, 1);
        defBox("half_west", 0, 0, 0, 0.5, 1, 1);
        defBox("half_north", 0, 0, 0, 1, 1, 0.5);
        defBox("quarter", 0.25, 0.25, 0.25, 0.75, 0.75, 0.75);
        defBox("eighth_grid", 0, 0, 0, 1, 0.875, 1);
        defBox("carpet", 0, 0, 0, 1, 0.0625, 1);           // 1/16 -> ArrayVoxelShape
        defBox("fence_post", 0.375, 0, 0.375, 0.625, 1.5, 0.625); // exceeds unit cube
        defBox("offgrid", 0.1, 0.2, 0.3, 0.9, 0.8, 0.7);
        defBox("snow_layer", 0, 0, 0, 1, 0.125, 1);
        defBox("pane_ns", 0.4375, 0, 0, 0.5625, 1, 1);     // 7/16..9/16 -> findBits -1
        defBox("fuzz_block", 1e-8, 0, 0, 1, 1, 1);         // fuzz -> identical to block()
        defBox("fuzz_above", 0, 0, 0, 1, 1.0000000999, 1); // < 1.0000001 -> fuzz to block()
        defBox("fuzz_array", 0, 0, 0, 1, 1.0000002, 1);    // > 1.0000001 -> ArrayVoxelShape
        defCreate("thin_empty", 0, 0, 0, 1, 5.0e-8, 1);    // span < EPSILON -> empty()
        defJoin("stairs", "slab_bottom", "half_north", "OR", true);
        defJoin("stairs_u", "slab_bottom", "half_north", "OR", false);
        defJoin("cross_and", "half_west", "half_north", "AND", true);
        defJoin("notch", "block", "quarter", "ONLY_FIRST", true);
        defJoin("xor_halves", "half_west", "slab_bottom", "NOT_SAME", true);
        defJoin("slabs_or_u", "slab_bottom", "slab_top", "OR", false);
        defJoin("carpet_post_u", "carpet", "fence_post", "OR", false);  // mixed grids
        defJoin("self_or", "slab_bottom", "slab_bottom", "OR", false);  // identity branch
        defJoin("apart_u", "quarter", "fence_post", "OR", false);
        defMove("slab_moved", "slab_bottom", 0.5, 0.0, -0.25);
        defMove("slab_eps_move", "slab_bottom", 1.0e-7, -1.0e-7, 0.0);
        defMove("quarter_third", "quarter", 1.0 / 3.0, 0.0, 0.0);
        defMove("empty_moved", "empty", 1.0, 2.0, 3.0);
        defMove("stairs_back", "stairs", -1.0, 0.0, 1.0);
        defOpt("stairs_opt", "stairs_u");
        defOpt("block_opt", "block");
        defOpt("xor_opt", "xor_halves");
        for (Direction d : Direction.values()) {
            defFace("slab_face_" + d.ordinal(), "slab_bottom", d);
            defFace("stairs_face_" + d.ordinal(), "stairs", d);
            defFace("post_face_" + d.ordinal(), "fence_post", d);
            defFace("carpet_face_" + d.ordinal(), "carpet", d);
        }
        defFace("block_face", "block", Direction.UP);
        defFace("empty_face", "empty", Direction.DOWN);

        // crafted clip queries
        qClip("block", new Vec3(0.5, 2, 0.5), new Vec3(0.5, -1, 0.5), BlockPos.ZERO);
        qClip("block", new Vec3(0.5, 0.5, 0.5), new Vec3(2, 0.5, 0.5), BlockPos.ZERO); // inside
        qClip("block", new Vec3(-1, -1, -1), new Vec3(2, 2, 2), BlockPos.ZERO);        // corner
        qClip("block", new Vec3(0.5, 0.5, 0.5), new Vec3(0.5, 0.5, 0.50001), BlockPos.ZERO); // tiny
        qClip("empty", new Vec3(0.5, 2, 0.5), new Vec3(0.5, -1, 0.5), BlockPos.ZERO);
        qClip("slab_bottom", new Vec3(3.5, 0.0, 7.5), new Vec3(3.5, -3.0, 7.5), new BlockPos(3, -2, 7));
        qClip("stairs", new Vec3(13.5, 66, 7.5), new Vec3(13.5, 63, 7.5), new BlockPos(13, 64, 7));
        qClip("stairs", new Vec3(12, 64.25, 7.5), new Vec3(15, 64.75, 7.5), new BlockPos(13, 64, 7));
        qClip("fence_post", new Vec3(0.5, 2, 0.5), new Vec3(0.5, 1.2, 0.5), BlockPos.ZERO);

        // crafted collide queries (face-touch + epsilon boundary cases)
        qCollide("block", Direction.Axis.Y, new AABB(0.2, 1.0, 0.2, 0.8, 2.0, 0.8), -0.7);
        qCollide("block", Direction.Axis.Y, new AABB(0.2, 1.0 + 1e-8, 0.2, 0.8, 2.0, 0.8), -0.7);
        qCollide("block", Direction.Axis.Y, new AABB(0.2, 1.0 + 1e-7, 0.2, 0.8, 2.0, 0.8), -0.7);
        qCollide("block", Direction.Axis.Y, new AABB(0.2, 1.5, 0.2, 0.8, 2.0, 0.8), -0.2);
        qCollide("block", Direction.Axis.Y, new AABB(0.2, -2.0, 0.2, 0.8, -1.0, 0.8), 1.5);
        qCollide("block", Direction.Axis.X, new AABB(-1.0, 0.2, 0.2, -0.3, 0.8, 0.8), 0.9);
        qCollide("block", Direction.Axis.X, new AABB(-1.0, 1.0, 0.2, -0.3, 1.8, 0.8), 0.9); // slides over
        qCollide("block", Direction.Axis.Z, new AABB(0.2, 0.2, 1.2, 0.8, 0.8, 1.9), -3.0);
        qCollide("block", Direction.Axis.Y, new AABB(0.2, 1.2, 0.2, 0.8, 2.0, 0.8), -5.0e-8); // |d|<eps
        qCollide("slab_bottom", Direction.Axis.Y, new AABB(0.2, 0.9, 0.2, 0.8, 1.4, 0.8), -0.9);
        qCollide("stairs", Direction.Axis.Z, new AABB(0.2, 0.75, 1.5, 0.8, 0.95, 2.0), -2.0);
        qCollide("stairs", Direction.Axis.Z, new AABB(0.2, 0.25, 1.5, 0.8, 0.45, 2.0), -2.0);
        qCollide("fence_post", Direction.Axis.Y, new AABB(0.4, 2.0, 0.4, 0.6, 3.0, 0.6), -2.0);
        qCollide("empty", Direction.Axis.Y, new AABB(0.2, 1.2, 0.2, 0.8, 2.0, 0.8), -0.7);
        qCollide("block", Direction.Axis.Y, new AABB(0.2, 0.5, 0.2, 0.8, 0.8, 0.8), -0.7); // overlapping
        qCollide("carpet_post_u", Direction.Axis.Y, new AABB(0.4, 2.0, 0.4, 0.6, 3.0, 0.6), -4.0);

        // crafted pair queries
        for (Direction d : Direction.values()) {
            qOccB("block", "block", d);
            qOccB("slab_bottom", "block", d);
            qOccB("slab_bottom", "slab_top", d);
            qOccB("stairs", "block", d);
            qOccB("block", "empty", d);
            qOccM("slab_bottom", "slab_top", d);
            qOccM("block", "slab_top", d);
            qOccM("stairs", "stairs", d);
        }
        qOccF("block", "empty");
        qOccF("slab_bottom", "slab_top");
        qOccF("stairs", "slab_bottom");
        qOccF("empty", "empty");
        qJoinNE("slab_bottom", "slab_top", "AND");
        qJoinNE("slab_bottom", "slab_top", "OR");
        qJoinNE("slab_bottom", "slab_bottom", "NOT_SAME");
        qJoinNE("quarter", "fence_post", "AND"); // disjoint fast path
        qJoinNE("empty", "block", "AND");
        qEqual("block", "fuzz_block");
        qEqual("stairs", "stairs_u");
        qEqual("stairs", "stairs_opt");
        qEqual("slab_bottom", "slab_top");
        qEqual("empty", "thin_empty");

        // ── seeded random battery (inputs all emitted; C++ has no RNG) ──────
        Random r = new Random(987654321L);

        for (int i = 0; i < 60; i++) { // 1/8-grid boxes -> CubeVoxelShape paths
            double[] v = new double[6];
            for (int a = 0; a < 3; a++) {
                int lo = r.nextInt(8);
                int hi = lo + 1 + r.nextInt(8 - lo);
                v[a] = lo / 8.0;
                v[a + 3] = hi / 8.0;
            }
            defBox("rg" + i, v[0], v[1], v[2], v[3], v[4], v[5]);
        }
        for (int i = 0; i < 30; i++) { // 1/16-grid boxes -> ArrayVoxelShape paths
            double[] v = new double[6];
            for (int a = 0; a < 3; a++) {
                int lo = r.nextInt(16);
                int hi = lo + 1 + r.nextInt(16 - lo);
                v[a] = lo / 16.0;
                v[a + 3] = hi / 16.0;
            }
            defBox("rs" + i, v[0], v[1], v[2], v[3], v[4], v[5]);
        }
        for (int i = 0; i < 30; i++) { // arbitrary boxes (may leave the unit cube)
            double[] v = new double[6];
            for (int a = 0; a < 3; a++) {
                double lo = r.nextDouble() * 1.6 - 0.3;
                v[a] = lo;
                v[a + 3] = lo + 0.02 + r.nextDouble() * 1.2;
            }
            defBox("ra" + i, v[0], v[1], v[2], v[3], v[4], v[5]);
        }
        for (int i = 0; i < 60; i++) { // random joins over the registry so far
            String a = NAMES.get(r.nextInt(NAMES.size()));
            String b = NAMES.get(r.nextInt(NAMES.size()));
            String op = OPS[r.nextInt(OPS.length)];
            defJoin("rj" + i, a, b, op, r.nextBoolean());
        }
        for (int i = 0; i < 20; i++) { // random moves
            String a = NAMES.get(r.nextInt(NAMES.size()));
            defMove("rm" + i, a, r.nextInt(33 - 16) / 16.0 + r.nextInt(3) - 1,
                    r.nextDouble() * 2 - 1, r.nextDouble() * 2 - 1);
        }
        for (int i = 0; i < 15; i++) { // random optimize
            defOpt("ro" + i, NAMES.get(r.nextInt(NAMES.size())));
        }
        for (int i = 0; i < 20; i++) { // random faces
            defFace("rf" + i, NAMES.get(r.nextInt(NAMES.size())),
                    Direction.values()[r.nextInt(6)]);
        }

        // random per-shape queries over the WHOLE registry
        List<String> all = new ArrayList<>(NAMES);
        for (String name : all) {
            for (int i = 0; i < 2; i++) {
                Vec3 from = new Vec3(r.nextDouble() * 4 - 1.5, r.nextDouble() * 4 - 1.5,
                        r.nextDouble() * 4 - 1.5);
                Vec3 to = new Vec3(r.nextDouble() * 4 - 1.5, r.nextDouble() * 4 - 1.5,
                        r.nextDouble() * 4 - 1.5);
                BlockPos pos = i == 0 ? BlockPos.ZERO
                        : new BlockPos(r.nextInt(5) - 2, r.nextInt(5) - 2, r.nextInt(5) - 2);
                qClip(name, i == 0 ? from : from.add(pos.getX(), pos.getY(), pos.getZ()),
                        i == 0 ? to : to.add(pos.getX(), pos.getY(), pos.getZ()), pos);
            }
            for (Direction.Axis axis : Direction.Axis.values()) {
                for (int i = 0; i < 2; i++) {
                    double cx = r.nextDouble() * 3 - 1, cy = r.nextDouble() * 3 - 1,
                            cz = r.nextDouble() * 3 - 1;
                    double hx = 0.05 + r.nextDouble() * 0.65, hy = 0.05 + r.nextDouble() * 0.65,
                            hz = 0.05 + r.nextDouble() * 0.65;
                    AABB moving = new AABB(cx - hx, cy - hy, cz - hz, cx + hx, cy + hy, cz + hz);
                    double dist = (r.nextBoolean() ? 1 : -1) * r.nextDouble() * 2.5;
                    qCollide(name, axis, moving, dist);
                }
            }
            qMinMax(name, Direction.Axis.values()[r.nextInt(3)],
                    r.nextDouble() * 1.4 - 0.2, r.nextDouble() * 1.4 - 0.2);
            // NOTE: closestPointTo is intentionally NOT exercised here. Vanilla's
            // VoxelShape.closestPointTo (VoxelShape.java:188) NPEs on a non-empty
            // shape whose forAllBoxes yields nothing — reachable only by the
            // synthetic random shape generator, never by real block shapes. The
            // movement-critical surface (collide/min/max/clip/coords/bool-ops) is
            // covered above. Re-add closestPointTo with a real-block-shape battery
            // when entity raytracing is ported.
        }

        // random pair queries
        for (int i = 0; i < 150; i++) {
            String a = all.get(r.nextInt(all.size()));
            String b = all.get(r.nextInt(all.size()));
            qJoinNE(a, b, OPS[r.nextInt(OPS.length)]);
            Direction d = Direction.values()[r.nextInt(6)];
            qOccB(a, b, d);
            qOccM(a, b, d);
            qOccF(a, b);
            qEqual(a, b);
        }

        // random multi-shape collide (Shapes.collide)
        for (int i = 0; i < 40; i++) {
            int k = 1 + r.nextInt(4);
            List<String> picks = new ArrayList<>();
            List<VoxelShape> shapes = new ArrayList<>();
            for (int j = 0; j < k; j++) {
                String n = all.get(r.nextInt(all.size()));
                picks.add(n);
                shapes.add(SHAPES.get(n));
            }
            Direction.Axis axis = Direction.Axis.values()[r.nextInt(3)];
            double cx = r.nextDouble() * 3 - 1, cy = r.nextDouble() * 3 - 1,
                    cz = r.nextDouble() * 3 - 1;
            AABB moving = new AABB(cx - 0.3, cy - 0.3, cz - 0.3, cx + 0.3, cy + 0.3, cz + 0.3);
            double dist = (r.nextBoolean() ? 1 : -1) * r.nextDouble() * 3;
            double res = Shapes.collide(axis, moving, shapes, dist);
            OUT.append("COLLIDEN ").append(axis.ordinal()).append(' ');
            aabb(moving);
            hex(dist);
            OUT.append(k);
            for (String n : picks) OUT.append(' ').append(n);
            OUT.append(' ');
            hex(res);
            OUT.append('\n');
        }

        REAL_OUT.print(OUT);
    }

    // ── DEF helpers ──────────────────────────────────────────────────────────
    static void register(String name, VoxelShape shape) {
        SHAPES.put(name, shape);
        NAMES.add(name);
        OUT.append("EMPTYQ ").append(name).append(' ').append(shape.isEmpty() ? 1 : 0).append('\n');
        OUT.append("COORDS ").append(name);
        for (Direction.Axis axis : Direction.Axis.values()) {
            var coords = shape.getCoords(axis);
            OUT.append(' ').append(coords.size());
            for (int i = 0; i < coords.size(); i++) {
                OUT.append(' ');
                hexNoTrail(coords.getDouble(i));
            }
        }
        OUT.append('\n');
        if (!shape.isEmpty()) {
            OUT.append("BOUNDS ").append(name).append(' ');
            aabb(shape.bounds());
            OUT.append('\n');
        }
        List<AABB> boxes = shape.toAabbs();
        OUT.append("AABBS ").append(name).append(' ').append(boxes.size());
        for (AABB box : boxes) {
            OUT.append(' ');
            aabbNoTrail(box);
        }
        OUT.append('\n');
    }

    static void defEmpty(String name) {
        OUT.append("DEF ").append(name).append(" EMPTY\n");
        register(name, Shapes.empty());
    }

    static void defBlock(String name) {
        OUT.append("DEF ").append(name).append(" BLOCK\n");
        register(name, Shapes.block());
    }

    static void defBox(String name, double x1, double y1, double z1, double x2, double y2, double z2) {
        OUT.append("DEF ").append(name).append(" BOX ");
        hex(x1); hex(y1); hex(z1); hex(x2); hex(y2); hexNoTrail(z2);
        OUT.append('\n');
        register(name, Shapes.box(x1, y1, z1, x2, y2, z2));
    }

    static void defCreate(String name, double x1, double y1, double z1, double x2, double y2, double z2) {
        OUT.append("DEF ").append(name).append(" CREATE ");
        hex(x1); hex(y1); hex(z1); hex(x2); hex(y2); hexNoTrail(z2);
        OUT.append('\n');
        register(name, Shapes.create(x1, y1, z1, x2, y2, z2));
    }

    static void defJoin(String name, String a, String b, String opName, boolean optimized) {
        OUT.append("DEF ").append(name).append(optimized ? " JOIN " : " JOINU ")
           .append(a).append(' ').append(b).append(' ').append(opName).append('\n');
        VoxelShape s = optimized ? Shapes.join(SHAPES.get(a), SHAPES.get(b), op(opName))
                                 : Shapes.joinUnoptimized(SHAPES.get(a), SHAPES.get(b), op(opName));
        register(name, s);
    }

    static void defMove(String name, String a, double dx, double dy, double dz) {
        OUT.append("DEF ").append(name).append(" MOVE ").append(a).append(' ');
        hex(dx); hex(dy); hexNoTrail(dz);
        OUT.append('\n');
        register(name, SHAPES.get(a).move(dx, dy, dz));
    }

    static void defOpt(String name, String a) {
        OUT.append("DEF ").append(name).append(" OPT ").append(a).append('\n');
        register(name, SHAPES.get(a).optimize());
    }

    static void defFace(String name, String a, Direction d) {
        OUT.append("DEF ").append(name).append(" FACE ").append(a).append(' ')
           .append(d.ordinal()).append('\n');
        register(name, SHAPES.get(a).getFaceShape(d));
    }

    // ── query helpers ────────────────────────────────────────────────────────
    static void qClip(String name, Vec3 from, Vec3 to, BlockPos pos) {
        BlockHitResult hit = SHAPES.get(name).clip(from, to, pos);
        OUT.append("CLIP ").append(name).append(' ');
        hex(from.x); hex(from.y); hex(from.z); hex(to.x); hex(to.y); hex(to.z);
        OUT.append(pos.getX()).append(' ').append(pos.getY()).append(' ').append(pos.getZ())
           .append(' ').append(hit == null ? 0 : 1);
        if (hit != null) {
            OUT.append(' ');
            hex(hit.getLocation().x); hex(hit.getLocation().y); hex(hit.getLocation().z);
            OUT.append(hit.getDirection().ordinal()).append(' ').append(hit.isInside() ? 1 : 0);
        }
        OUT.append('\n');
    }

    static void qCollide(String name, Direction.Axis axis, AABB moving, double dist) {
        double res = SHAPES.get(name).collide(axis, moving, dist);
        OUT.append("COLLIDE ").append(name).append(' ').append(axis.ordinal()).append(' ');
        aabb(moving);
        hex(dist);
        hexNoTrail(res);
        OUT.append('\n');
    }

    static void qJoinNE(String a, String b, String opName) {
        boolean res = Shapes.joinIsNotEmpty(SHAPES.get(a), SHAPES.get(b), op(opName));
        OUT.append("JOINNE ").append(a).append(' ').append(b).append(' ').append(opName)
           .append(' ').append(res ? 1 : 0).append('\n');
    }

    static void qOccB(String a, String b, Direction d) {
        boolean res = Shapes.blockOccludes(SHAPES.get(a), SHAPES.get(b), d);
        OUT.append("OCCB ").append(a).append(' ').append(b).append(' ').append(d.ordinal())
           .append(' ').append(res ? 1 : 0).append('\n');
    }

    static void qOccM(String a, String b, Direction d) {
        boolean res = Shapes.mergedFaceOccludes(SHAPES.get(a), SHAPES.get(b), d);
        OUT.append("OCCM ").append(a).append(' ').append(b).append(' ').append(d.ordinal())
           .append(' ').append(res ? 1 : 0).append('\n');
    }

    static void qOccF(String a, String b) {
        boolean res = Shapes.faceShapeOccludes(SHAPES.get(a), SHAPES.get(b));
        OUT.append("OCCF ").append(a).append(' ').append(b).append(' ')
           .append(res ? 1 : 0).append('\n');
    }

    static void qEqual(String a, String b) {
        boolean res = Shapes.equal(SHAPES.get(a), SHAPES.get(b));
        OUT.append("EQUALQ ").append(a).append(' ').append(b).append(' ')
           .append(res ? 1 : 0).append('\n');
    }

    static void qMinMax(String name, Direction.Axis axis, double b, double c) {
        VoxelShape s = SHAPES.get(name);
        OUT.append("MINMAX ").append(name).append(' ').append(axis.ordinal()).append(' ');
        hex(b); hex(c); hex(s.min(axis, b, c)); hexNoTrail(s.max(axis, b, c));
        OUT.append('\n');
    }

    static void qClosest(String name, Vec3 point) {
        var res = SHAPES.get(name).closestPointTo(point);
        OUT.append("CLOSEST ").append(name).append(' ');
        hex(point.x); hex(point.y); hex(point.z);
        OUT.append(res.isPresent() ? 1 : 0);
        if (res.isPresent()) {
            OUT.append(' ');
            hex(res.get().x); hex(res.get().y); hexNoTrail(res.get().z);
        }
        OUT.append('\n');
    }

    static BooleanOp op(String name) {
        return switch (name) {
            case "OR" -> BooleanOp.OR;
            case "AND" -> BooleanOp.AND;
            case "ONLY_FIRST" -> BooleanOp.ONLY_FIRST;
            case "ONLY_SECOND" -> BooleanOp.ONLY_SECOND;
            case "NOT_SAME" -> BooleanOp.NOT_SAME;
            default -> throw new IllegalArgumentException(name);
        };
    }

    static void aabb(AABB box) {
        hex(box.minX); hex(box.minY); hex(box.minZ); hex(box.maxX); hex(box.maxY); hex(box.maxZ);
    }

    static void aabbNoTrail(AABB box) {
        hex(box.minX); hex(box.minY); hex(box.minZ); hex(box.maxX); hex(box.maxY); hexNoTrail(box.maxZ);
    }

    static void hex(double d) {
        OUT.append(String.format("%016x", Double.doubleToRawLongBits(d))).append(' ');
    }

    static void hexNoTrail(double d) {
        OUT.append(String.format("%016x", Double.doubleToRawLongBits(d)));
    }
}
