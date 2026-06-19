// Ground truth for the ITEM REGISTRY: every item in vanilla id order with the
// core properties the engine needs (the full component system comes later).
//
//   ITEM <id> <key> <maxStack> <maxDamage> <blockKey or -> <hasFood 0/1>
//   TOTAL <count>
import net.minecraft.core.component.DataComponents;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.item.BlockItem;
import net.minecraft.world.item.Item;

public class ItemRegistryParity {
    public static void main(String[] args) {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // bind item data components (the registry holders start unbound;
        // DataComponentInitializers.PendingComponents.apply -> bindComponents,
        // same as RegistryDataCollector does at runtime)
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());

        StringBuilder out = new StringBuilder(1 << 20);
        int count = 0;
        for (Item item : BuiltInRegistries.ITEM) {
            int id = BuiltInRegistries.ITEM.getId(item);
            String key = BuiltInRegistries.ITEM.getKey(item).toString();
            int maxStack = item.components().getOrDefault(DataComponents.MAX_STACK_SIZE, 64);
            Integer maxDamage = item.components().get(DataComponents.MAX_DAMAGE);
            String blockKey = item instanceof BlockItem bi
                ? BuiltInRegistries.BLOCK.getKey(bi.getBlock()).toString() : "-";
            boolean hasFood = item.components().has(DataComponents.FOOD);
            out.append("ITEM\t").append(id).append('\t').append(key).append('\t')
               .append(maxStack).append('\t').append(maxDamage == null ? 0 : maxDamage).append('\t')
               .append(blockKey).append('\t').append(hasFood ? 1 : 0).append('\n');
            count++;
        }
        out.append("TOTAL\t").append(count).append('\n');
        System.out.print(out);
    }
}
