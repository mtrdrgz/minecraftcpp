// Ground truth for the BLOCK-STATE REGISTRY: every state in vanilla's global
// Block.BLOCK_STATE_REGISTRY in ID order — the ordering that chunk palettes,
// saves and the network protocol depend on — plus the per-state flags the C++
// engine trusts from its block_states.json dump (is_air / occlusion / motion-
// blocking / fluid) and the full property string the engine currently DROPS.
//
//   STATE <id> <blockKey> <props or -> <isAir 0/1> <canOcclude 0/1> <blocksMotion 0/1> <fluid 0/1>
//   TOTAL <count>
import java.util.Map;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;

public class BlockStateRegistryParity {
    public static void main(String[] args) {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StringBuilder out = new StringBuilder(1 << 22);
        int count = 0;
        for (BlockState state : Block.BLOCK_STATE_REGISTRY) {
            int id = Block.BLOCK_STATE_REGISTRY.getId(state);
            String key = BuiltInRegistries.BLOCK.getKey(state.getBlock()).toString();
            // exact vanilla canonical property string (StateHolder.toString joins
            // Property.Value::toString with commas, StateHolder.java:47)
            String props = state.getValues().map(Property.Value::toString)
                                .collect(java.util.stream.Collectors.joining(","));
            out.append("STATE\t").append(id).append('\t').append(key).append('\t')
               .append(props.isEmpty() ? "-" : props).append('\t')
               .append(state.isAir() ? 1 : 0).append('\t')
               .append(state.canOcclude() ? 1 : 0).append('\t')
               .append(state.blocksMotion() ? 1 : 0).append('\t')
               .append(state.getFluidState().isEmpty() ? 0 : 1).append('\n');
            count++;
        }
        // the block's DEFAULT state id (Block.defaultBlockState — NOT necessarily the
        // block's first registered state; e.g. pillar defaults are axis=y)
        for (Block b : BuiltInRegistries.BLOCK) {
            out.append("DEFAULT\t").append(BuiltInRegistries.BLOCK.getKey(b)).append('\t')
               .append(Block.BLOCK_STATE_REGISTRY.getId(b.defaultBlockState())).append('\n');
        }
        out.append("TOTAL\t").append(count).append('\n');
        System.out.print(out);
    }

}
