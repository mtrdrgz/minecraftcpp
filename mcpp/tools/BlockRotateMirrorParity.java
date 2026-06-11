// Ground truth for net.minecraft.world.level.block.state.BlockState.rotate(Rotation)
// and .mirror(Mirror) over EVERY state in Block.BLOCK_STATE_REGISTRY — the transforms
// StructureTemplate.placeInWorld applies to each template block (and the keystone the
// world-placement layer needs). RULE #0: we drive the REAL BlockState.rotate/mirror,
// never reimplement Java-side.
//
// BlockState.rotate(r)/mirror(m) dispatch to Block.rotate(state,r)/mirror(state,m); the
// default (BlockBehaviour) returns the state unchanged, and ~66 block classes override.
// Each result is reported as a global state id (Block.BLOCK_STATE_REGISTRY.getId), so the
// C++ port reproduces it via its (block,props)->id reverse lookup.
//
// Rows:
//   R <id> <rotCW90> <rotCW180> <rotCCW90> <mirLR> <mirFB>   (NONE is identity, omitted)
//   FAM <blockKey> <rotateDeclaringClass> <mirrorDeclaringClass>
//       the class in the block's hierarchy that DECLARES rotate/mirror — the C++ certifies
//       a block iff it has ported that declaring class's transform (so the gate goes green
//       family-by-family, and blocks sharing a declarer are all covered at once).
//   TOTAL <count>
import java.lang.reflect.Method;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.state.BlockState;

public final class BlockRotateMirrorParity {
    // The class in `start`'s hierarchy that declares method(name, paramTypes); "?" if none.
    private static String declaringClass(Class<?> start, String name, Class<?>... params) {
        for (Class<?> c = start; c != null; c = c.getSuperclass()) {
            try {
                Method m = c.getDeclaredMethod(name, params);
                if (m != null) return c.getSimpleName();
            } catch (NoSuchMethodException ignored) {
                // keep walking up
            }
        }
        return "?";
    }

    public static void main(String[] args) {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StringBuilder out = new StringBuilder(1 << 23);
        int count = 0;
        for (BlockState state : Block.BLOCK_STATE_REGISTRY) {
            int id = Block.BLOCK_STATE_REGISTRY.getId(state);
            int cw90  = Block.BLOCK_STATE_REGISTRY.getId(state.rotate(Rotation.CLOCKWISE_90));
            int cw180 = Block.BLOCK_STATE_REGISTRY.getId(state.rotate(Rotation.CLOCKWISE_180));
            int ccw90 = Block.BLOCK_STATE_REGISTRY.getId(state.rotate(Rotation.COUNTERCLOCKWISE_90));
            int mlr   = Block.BLOCK_STATE_REGISTRY.getId(state.mirror(Mirror.LEFT_RIGHT));
            int mfb   = Block.BLOCK_STATE_REGISTRY.getId(state.mirror(Mirror.FRONT_BACK));
            out.append("R\t").append(id).append('\t')
               .append(cw90).append('\t').append(cw180).append('\t').append(ccw90).append('\t')
               .append(mlr).append('\t').append(mfb).append('\n');
            count++;
        }
        // per-block rotate/mirror declaring class (the family key for incremental certification).
        for (Block b : BuiltInRegistries.BLOCK) {
            String key = BuiltInRegistries.BLOCK.getKey(b).toString();
            String rotFam = declaringClass(b.getClass(), "rotate", BlockState.class, Rotation.class);
            String mirFam = declaringClass(b.getClass(), "mirror", BlockState.class, Mirror.class);
            out.append("FAM\t").append(key).append('\t').append(rotFam).append('\t').append(mirFam).append('\n');
        }
        out.append("TOTAL\t").append(count).append('\n');
        System.out.print(out);
    }
}
