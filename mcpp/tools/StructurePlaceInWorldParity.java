// Ground truth for net.minecraft...StructureTemplate.placeInWorld — the routine that stamps a
// structure template's blocks into the world (the core of structure generation). RULE #0: we
// drive the REAL placeInWorld; we never reimplement it Java-side.
//
// We load real single-palette templates from client.jar, place each with several
// (rotation, mirror, position) settings and NO processors into a CAPTURING ServerLevelAccessor
// (a java.lang.reflect.Proxy that records setBlock(pos,state) into a map — last-write-wins — and
// returns air / EMPTY fluid / null block-entity for reads, decoupling from real terrain). We emit
// the final placed map. The C++ gate reproduces it via the certified structureTransform (position)
// + BlockRotation.h (state.mirror().rotate()), parsing the same template nbt (shipped base64).
//
// Rows:
//   NBT  <templateKey> <base64 gzip .nbt>
//   CASE <caseId> <templateKey> <rotOrd> <mirOrd> <posX> <posY> <posZ>
//   PLACED <caseId> <wx> <wy> <wz> <stateId>
//   COUNT <caseId> <numPlaced>

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.zip.ZipFile;

import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.material.Fluids;
import net.minecraft.world.level.ServerLevelAccessor;

public final class StructurePlaceInWorldParity {
    @SuppressWarnings({"deprecation", "unchecked"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        final StringBuilder out = new StringBuilder(1 << 22);

        RegistryAccess registryAccess = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);
        HolderGetter<Block> blockLookup = BuiltInRegistries.BLOCK;
        ZipFile jar = new ZipFile(java.nio.file.Path.of("26.1.2", "client.jar").toFile());

        // a handful of real pillager_outpost pieces (single-palette, no entities).
        String[] templateKeys = {
            "minecraft:pillager_outpost/base_plate",
            "minecraft:pillager_outpost/feature_plate",
            "minecraft:pillager_outpost/watchtower"
        };
        Rotation[] rots = Rotation.values();
        Mirror[] mirs = Mirror.values();
        int[][] positions = {{0,0,0},{16,70,-32}};

        int caseId = 0;
        for (String key : templateKeys) {
            String path = "data/" + "minecraft" + "/structure/" + key.substring(key.indexOf(':')+1) + ".nbt";
            var entry = jar.getEntry(path);
            if (entry == null) { System.err.println("missing " + path); continue; }
            byte[] raw;
            try (var in = jar.getInputStream(entry)) { raw = in.readAllBytes(); }
            out.append("NBT\t").append(key).append('\t')
               .append(java.util.Base64.getEncoder().encodeToString(raw)).append('\n');

            CompoundTag tag = NbtIo.readCompressed(new java.io.ByteArrayInputStream(raw), NbtAccounter.unlimitedHeap());
            StructureTemplate template = new StructureTemplate();
            template.load(blockLookup, tag);

            for (Rotation rot : rots) {
                for (Mirror mir : mirs) {
                    for (int[] p : positions) {
                        BlockPos position = new BlockPos(p[0], p[1], p[2]);
                        // knownShape=true SKIPS the post-placement Block.updateFromNeighbourShapes
                        // pass (which recomputes fence/wall/stair/redstone connections from the placed
                        // neighbours) — isolating the PURE placement transform (pos + state mirror/rotate)
                        // that this gate certifies. The neighbour-shape recompute is a separate subsystem.
                        StructurePlaceSettings settings = new StructurePlaceSettings()
                            .setRotation(rot).setMirror(mir).setIgnoreEntities(true)
                            .setFinalizeEntities(false).setKnownShape(true);
                        Map<BlockPos, BlockState> placed = new LinkedHashMap<>();
                        ServerLevelAccessor level = makeCapturingLevel(placed, registryAccess);
                        RandomSource random = RandomSource.create(0L);
                        template.placeInWorld(level, position, position, settings, random, 2);
                        out.append("CASE\t").append(caseId).append('\t').append(key).append('\t')
                           .append(rot.ordinal()).append('\t').append(mir.ordinal()).append('\t')
                           .append(p[0]).append('\t').append(p[1]).append('\t').append(p[2]).append('\n');
                        for (var e : placed.entrySet()) {
                            out.append("PLACED\t").append(caseId).append('\t')
                               .append(e.getKey().getX()).append('\t').append(e.getKey().getY()).append('\t')
                               .append(e.getKey().getZ()).append('\t')
                               .append(Block.BLOCK_STATE_REGISTRY.getId(e.getValue())).append('\n');
                        }
                        out.append("COUNT\t").append(caseId).append('\t').append(placed.size()).append('\n');
                        caseId++;
                    }
                }
            }
        }
        jar.close();
        System.out.print(out);
    }

    // A java.lang.reflect.Proxy ServerLevelAccessor: setBlock records (last-write-wins), reads
    // return air / EMPTY fluid / null block-entity, height = overworld span; everything else a default.
    private static ServerLevelAccessor makeCapturingLevel(final Map<BlockPos, BlockState> placed,
                                                          final RegistryAccess registryAccess) {
        final BlockState air = Blocks.AIR.defaultBlockState();
        InvocationHandler h = (proxy, method, mArgs) -> {
            switch (method.getName()) {
                case "setBlock":
                    placed.put(((BlockPos) mArgs[0]).immutable(), (BlockState) mArgs[1]);
                    return Boolean.TRUE;
                case "getBlockState": {
                    BlockState st = placed.get(mArgs[0]);
                    return st != null ? st : air;
                }
                case "getFluidState":   return Fluids.EMPTY.defaultFluidState();
                case "getBlockEntity":  return null;
                case "registryAccess":  return registryAccess;
                case "getMinY": case "getMinBuildHeight": return -64;
                case "getMaxY":         return 319;
                case "getHeight":       return 384;
                case "isClientSide":    return Boolean.FALSE;
                case "toString":        return "CapturingLevel";
                case "hashCode":        return System.identityHashCode(proxy);
                case "equals":          return proxy == mArgs[0];
                default:
                    Class<?> rt = method.getReturnType();
                    if (rt == boolean.class) return Boolean.FALSE;
                    if (rt == int.class) return 0;
                    if (rt == long.class) return 0L;
                    if (rt == float.class) return 0f;
                    if (rt == double.class) return 0d;
                    if (rt.isPrimitive()) return 0;
                    return null;
            }
        };
        return (ServerLevelAccessor) Proxy.newProxyInstance(
            ServerLevelAccessor.class.getClassLoader(), new Class[]{ServerLevelAccessor.class}, h);
    }
}
