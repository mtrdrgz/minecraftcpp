// Ground-truth oracle for net.minecraft...BlockState.getCollisionShape over every state in
// Block.BLOCK_STATE_REGISTRY — the per-block collision VoxelShapes. This is the foundational
// subsystem behind block collision, face occlusion, and updateFromNeighbourShapes (isFaceSturdy
// -> getBlockSupportShape). RULE #0: we drive the REAL getCollisionShape; never reimplement.
//
// Each shape is emitted as its canonical list of AABBs (VoxelShape.toAabbs(), each box sorted
// then the list sorted) at full double precision, so the C++ port reproduces it via the certified
// VoxelShape primitives (Shapes.box/block/or) and compares with VoxelShape.toAabbs().
//
// Rows:
//   SHAPE <stateId> <numBoxes> [<x1> <y1> <z1> <x2> <y2> <z2>]...    (collision shape)
//   FAM   <blockKey> <getCollisionShapeDeclaringClass> <getShapeDeclaringClass>
//   TOTAL <count>

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.level.EmptyBlockGetter;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;

public final class BlockCollisionShapeParity {
    private static boolean hasCollision(Block b) {
        try {
            java.lang.reflect.Field f = net.minecraft.world.level.block.state.BlockBehaviour.class.getDeclaredField("hasCollision");
            f.setAccessible(true);
            return f.getBoolean(b);
        } catch (Exception e) { return true; }
    }

    private static String declaringClass(Class<?> start, String name, Class<?>... params) {
        for (Class<?> c = start; c != null; c = c.getSuperclass()) {
            try { if (c.getDeclaredMethod(name, params) != null) return c.getSimpleName(); }
            catch (NoSuchMethodException ignored) {}
        }
        return "?";
    }

    @SuppressWarnings({"deprecation", "unchecked"})
    public static void main(String[] args) {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        final StringBuilder out = new StringBuilder(1 << 23);

        int count = 0;
        for (BlockState state : Block.BLOCK_STATE_REGISTRY) {
            int id = Block.BLOCK_STATE_REGISTRY.getId(state);
            List<AABB> boxes;
            try {
                VoxelShape shape = state.getCollisionShape(EmptyBlockGetter.INSTANCE, BlockPos.ZERO, CollisionContext.empty());
                boxes = new ArrayList<>(shape.toAabbs());
            } catch (Throwable t) {
                out.append("SHAPE\t").append(id).append("\t-1\n");  // context-dependent / threw: defer
                count++;
                continue;
            }
            // canonical order: by (minX,minY,minZ,maxX,maxY,maxZ).
            boxes.sort(Comparator.<AABB>comparingDouble(b -> b.minX).thenComparingDouble(b -> b.minY)
                .thenComparingDouble(b -> b.minZ).thenComparingDouble(b -> b.maxX)
                .thenComparingDouble(b -> b.maxY).thenComparingDouble(b -> b.maxZ));
            out.append("SHAPE\t").append(id).append('\t').append(boxes.size());
            for (AABB b : boxes)
                out.append('\t').append(d(b.minX)).append('\t').append(d(b.minY)).append('\t').append(d(b.minZ))
                   .append('\t').append(d(b.maxX)).append('\t').append(d(b.maxY)).append('\t').append(d(b.maxZ));
            out.append('\n');
            count++;
        }
        for (Block b : BuiltInRegistries.BLOCK) {
            out.append("FAM\t").append(BuiltInRegistries.BLOCK.getKey(b)).append('\t')
               .append(declaringClass(b.getClass(), "getCollisionShape", BlockState.class,
                       net.minecraft.world.level.BlockGetter.class, BlockPos.class, CollisionContext.class))
               .append('\t')
               .append(declaringClass(b.getClass(), "getShape", BlockState.class,
                       net.minecraft.world.level.BlockGetter.class, BlockPos.class, CollisionContext.class))
               .append('\t')
               .append(hasCollision(b) ? 1 : 0)  // BlockBehaviour.hasCollision (Properties) — exact
               .append('\n');
        }
        out.append("TOTAL\t").append(count).append('\n');
        System.out.print(out);
    }

    // round-trippable double (17 sig digits is exact for IEEE-754).
    private static String d(double v) { return Double.toString(v); }
}
