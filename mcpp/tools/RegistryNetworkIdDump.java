// Dumps the network-relevant BuiltInRegistries in their EXACT getId() order — the
// canonical wire-id order a 1:1 port must reproduce so that registry/holder packet
// fields (VarInt(getId)) byte-match vanilla. Same pattern as ItemRegistryParity (which
// produced items.json) — the C++ engine loads these lists into mc::Registry<T> so that
// resolving a ResourceLocation -> VarInt id matches BuiltInRegistries exactly.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool RegistryNetworkIdDump -Out mcpp/build/registry_network_ids.tsv
//
// Row format (tab-separated):
//   REG   <registryName>   <count>
//   E     <registryName>   <id>   <resourceLocation>
// ids are 0-based getId() (== insertion order == on-wire VarInt). resourceLocation is ns:path.

import net.minecraft.core.Registry;
import net.minecraft.core.registries.BuiltInRegistries;
// NOTE: net.minecraft.resources.ResourceLocation was renamed to Identifier in 26.1.2; we
// avoid naming the type (use var + toString) so this tool is rename-agnostic.

public class RegistryNetworkIdDump {
    static final java.io.PrintStream O = System.out;

    // Dump one registry in getId() order. We iterate the registry's natural iteration
    // (which for MappedRegistry is byId order == getId order) and also assert id==index.
    static <T> void dump(String name, Registry<T> reg) {
        int count = reg.size();
        O.println("REG\t" + name + "\t" + count);
        // byId order: ask for each id 0..count-1 (MappedRegistry.byId(i)); getKey gives the RL.
        for (int id = 0; id < count; id++) {
            T value = reg.byId(id);
            var rl = reg.getKey(value);
            // sanity: the registry's own getId must equal our index (else the dump order is wrong)
            int gid = reg.getId(value);
            if (gid != id) throw new IllegalStateException(name + " id mismatch at " + id + " got " + gid);
            O.println("E\t" + name + "\t" + id + "\t" + (rl == null ? "null" : rl.toString()));
        }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Built-in registries whose ids appear directly on the play-protocol wire as
        // VarInt(getId) via ByteBufCodecs.registry/holderRegistry/holder.
        dump("minecraft:item", BuiltInRegistries.ITEM);  // ItemStack/Item.STREAM_CODEC holder id (== items.json order)
        dump("minecraft:mob_effect", BuiltInRegistries.MOB_EFFECT);
        dump("minecraft:sound_event", BuiltInRegistries.SOUND_EVENT);
        dump("minecraft:menu", BuiltInRegistries.MENU);
        dump("minecraft:entity_type", BuiltInRegistries.ENTITY_TYPE);
        dump("minecraft:particle_type", BuiltInRegistries.PARTICLE_TYPE);
        dump("minecraft:data_component_type", BuiltInRegistries.DATA_COMPONENT_TYPE);
        dump("minecraft:attribute", BuiltInRegistries.ATTRIBUTE);
        O.flush();
    }
}
