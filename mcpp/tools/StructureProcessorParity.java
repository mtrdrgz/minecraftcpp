// Ground truth for net.minecraft...templatesystem RuleTest.test(BlockState, RandomSource) —
// the input-predicate logic of RuleProcessor, the dominant StructureProcessor used by every
// jigsaw structure (mossify, degradation, street/zombie/farm variants, ...). RULE #0: we drive
// the REAL RuleTest.test over the REAL processor-list registry; we never reimplement it Java-side.
//
// For every top-level RuleProcessor in the vanilla PROCESSOR_LIST registry, we emit each rule's
// input_predicate config + drive test() over a battery of block states x seeds. The C++ port
// reproduces test() from the emitted config (no JSON parsing needed):
//   always_true        -> true
//   block_match        -> state.is(block)
//   blockstate_match   -> state == blockState           (plain state-id equality)
//   tag_match          -> state.is(tag)                 (BlockTags)
//   random_block_match -> state.is(block) && random.nextFloat() < probability
//
// Rows:
//   BATTERY <stateId>
//   RULE <listKey> <procIdx> <ruleIdx> <type> [arg...]
//        block_match <blockKey> | blockstate_match <stateId> | tag_match <tagKey> |
//        random_block_match <blockKey> <probability> | always_true
//   TEST <listKey> <procIdx> <ruleIdx> <stateId> <seed> <result 0/1>
//   OTHER <typeKey>                                       (input_predicate type NOT yet covered)

import com.google.common.collect.ImmutableList;
import java.lang.reflect.Field;
import java.util.LinkedHashSet;
import java.util.Optional;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.core.HolderLookup;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.templatesystem.BlockMatchTest;
import net.minecraft.world.level.levelgen.structure.templatesystem.BlockRotProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.BlockStateMatchTest;
import net.minecraft.world.level.levelgen.structure.templatesystem.ProcessorRule;
import net.minecraft.world.level.levelgen.structure.templatesystem.RandomBlockMatchTest;
import net.minecraft.world.level.levelgen.structure.templatesystem.RuleProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.RuleTest;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessor;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureProcessorList;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.TagMatchTest;

public final class StructureProcessorParity {
    @SuppressWarnings("unchecked")
    private static <T> T getField(Class<?> owner, Object target, String name) throws Exception {
        Field f = owner.getDeclaredField(name);
        f.setAccessible(true);
        return (T) f.get(target);
    }

    @SuppressWarnings({"deprecation", "unchecked"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        final StringBuilder out = new StringBuilder(1 << 24);

        HolderLookup.Provider holders = VanillaRegistries.createLookup();
        HolderLookup.RegistryLookup<StructureProcessorList> plReg =
            holders.lookupOrThrow(Registries.PROCESSOR_LIST);

        // battery: default state of every block + the exact states referenced by blockstate_match.
        LinkedHashSet<Integer> battery = new LinkedHashSet<>();
        for (Block b : BuiltInRegistries.BLOCK)
            battery.add(Block.BLOCK_STATE_REGISTRY.getId(b.defaultBlockState()));

        // pass 1: collect blockstate_match exact states into the battery.
        for (var holder : plReg.listElements().toList()) {
            for (StructureProcessor proc : holder.value().list()) {
                if (!(proc instanceof RuleProcessor rp)) continue;
                ImmutableList<ProcessorRule> rules = getField(RuleProcessor.class, rp, "rules");
                for (ProcessorRule rule : rules) {
                    RuleTest input = getField(ProcessorRule.class, rule, "inputPredicate");
                    if (input instanceof BlockStateMatchTest bsm) {
                        BlockState bs = getField(BlockStateMatchTest.class, bsm, "blockState");
                        battery.add(Block.BLOCK_STATE_REGISTRY.getId(bs));
                    }
                }
            }
        }
        for (int id : battery) out.append("BATTERY\t").append(id).append('\n');

        final long[] seeds = {0L, 1L, 1337L};
        LinkedHashSet<String> uncovered = new LinkedHashSet<>();

        for (var holder : plReg.listElements().toList()) {
            String listKey = holder.key().identifier().toString();
            int procIdx = -1;
            for (StructureProcessor proc : holder.value().list()) {
                procIdx++;
                if (proc instanceof BlockRotProcessor brp) {
                    // BlockRotProcessor.processBlock (:41-53): removes (null) iff
                    //   (rottable absent || originalState.is(rottable)) && !(getRandom(pos).nextFloat() <= integrity).
                    // settings.random is null here -> getRandom(pos) = RandomSource.create(Mth.getSeed(pos)).
                    float integrity = getField(BlockRotProcessor.class, brp, "integrity");
                    Optional<HolderSet<Block>> rb = getField(BlockRotProcessor.class, brp, "rottableBlocks");
                    String filter;
                    if (rb.isEmpty()) {
                        filter = "-";
                    } else if (rb.get().unwrapKey().isPresent()) {
                        filter = "tag:" + rb.get().unwrapKey().get().location();
                    } else {
                        StringBuilder sb = new StringBuilder("list:");
                        boolean first = true;
                        for (Holder<Block> h : rb.get()) { if (!first) sb.append(','); sb.append(BuiltInRegistries.BLOCK.getKey(h.value())); first = false; }
                        filter = sb.toString();
                    }
                    out.append("ROTCFG\t").append(listKey).append('\t').append(procIdx).append('\t')
                       .append(integrity).append('\t').append(filter).append('\n');
                    StructurePlaceSettings settings = new StructurePlaceSettings();
                    final int[][] positions = {{0,0,0},{1,2,3},{5,-3,7},{100,64,-50},{-17,200,33}};
                    for (int[] p : positions) {
                        BlockPos pos = new BlockPos(p[0], p[1], p[2]);
                        for (int id : battery) {
                            BlockState st = Block.BLOCK_STATE_REGISTRY.byId(id);
                            var info = new StructureTemplate.StructureBlockInfo(pos, st, null);
                            var res = brp.processBlock(null, pos, pos, info, info, settings);
                            out.append("ROTTEST\t").append(listKey).append('\t').append(procIdx).append('\t')
                               .append(id).append('\t').append(p[0]).append('\t').append(p[1]).append('\t')
                               .append(p[2]).append('\t').append(res != null ? 1 : 0).append('\n');
                        }
                    }
                    continue;
                }
                if (!(proc instanceof RuleProcessor rp)) continue;
                ImmutableList<ProcessorRule> rules = getField(RuleProcessor.class, rp, "rules");
                int ruleIdx = -1;
                for (ProcessorRule rule : rules) {
                    ruleIdx++;
                    RuleTest input = getField(ProcessorRule.class, rule, "inputPredicate");
                    String cfg;
                    if (input.getClass().getSimpleName().equals("AlwaysTrueTest")) {
                        cfg = "always_true";
                    } else if (input instanceof BlockMatchTest bm) {
                        Block b = getField(BlockMatchTest.class, bm, "block");
                        cfg = "block_match\t" + BuiltInRegistries.BLOCK.getKey(b);
                    } else if (input instanceof BlockStateMatchTest bsm) {
                        BlockState bs = getField(BlockStateMatchTest.class, bsm, "blockState");
                        cfg = "blockstate_match\t" + Block.BLOCK_STATE_REGISTRY.getId(bs);
                    } else if (input instanceof TagMatchTest tm) {
                        TagKey<Block> tag = getField(TagMatchTest.class, tm, "tag");
                        cfg = "tag_match\t" + tag.location();
                    } else if (input instanceof RandomBlockMatchTest rbm) {
                        Block b = getField(RandomBlockMatchTest.class, rbm, "block");
                        float p = getField(RandomBlockMatchTest.class, rbm, "probability");
                        cfg = "random_block_match\t" + BuiltInRegistries.BLOCK.getKey(b) + "\t" + p;
                    } else {
                        uncovered.add(input.getClass().getSimpleName());
                        continue;  // not yet covered -> no RULE/TEST rows (honest, not silent pass)
                    }
                    out.append("RULE\t").append(listKey).append('\t').append(procIdx).append('\t')
                       .append(ruleIdx).append('\t').append(cfg).append('\n');
                    for (int id : battery) {
                        BlockState st = Block.BLOCK_STATE_REGISTRY.byId(id);
                        for (long seed : seeds) {
                            RandomSource r = RandomSource.create(seed);
                            boolean res = input.test(st, r);
                            out.append("TEST\t").append(listKey).append('\t').append(procIdx).append('\t')
                               .append(ruleIdx).append('\t').append(id).append('\t').append(seed)
                               .append('\t').append(res ? 1 : 0).append('\n');
                        }
                    }
                }
            }
        }
        for (String u : uncovered) out.append("OTHER\t").append(u).append('\n');
        System.out.print(out);
    }
}
