// Ground-truth generator for the C++ 1:1 port of the PURE StructureTemplate .nbt
// loader + jigsaw-block discovery + getShuffledJigsawBlocks ordering. RULE #0: we
// drive the REAL 26.1.2 classes; we NEVER reimplement the loader / shuffle /
// sort Java-side.
//
// For each pillager_outpost template (data/minecraft/structure/pillager_outpost/
// <name>.nbt in 26.1.2/client.jar):
//   1. read the RAW gzip .nbt bytes from the jar (so the C++ test parses the SAME
//      bytes) and emit them base64.
//   2. NbtIo.readCompressed -> CompoundTag, then `new StructureTemplate().load(
//      BuiltInRegistries.BLOCK, tag)` — the REAL loader.
//   3. emit size + total StructureBlockInfo count (the REAL palette block list).
//   4. for a fixed battery of (offset, rotation, seed): reproduce the REAL
//      SinglePoolElement.getShuffledJigsawBlocks pipeline by calling the REAL
//      StructureTemplate.getJigsaws(position, rotation) (which applies the
//      transform + FrontAndTop rotation) then the REAL net.minecraft.util.Util
//      .shuffle(list, RandomSource.create(seed)) then the REAL @VisibleForTesting
//      SinglePoolElement.sortBySelectionPriority. Emit each ordered jigsaw block.
//
// These are the exact three calls SinglePoolElement.getShuffledJigsawBlocks makes
// (SinglePoolElement.java:114-127), so the output is byte-for-byte the assembly
// oracle, decoupled from the StructureTemplateManager.
//
//   tools/run_groundtruth.ps1 -Tool StructureTemplateLoaderParity -Out mcpp/build/structure_template_loader.tsv
//
// TSV rows (leading TAG; all ints decimal; strings carried verbatim — they are
// pure ASCII Identifiers, so no base64 needed):
//   TEMPLATE <name> <base64nbt> <sizeX> <sizeY> <sizeZ> <numBlockInfos>
//   JIGSAW   <name> <rotationOrdinal> <seed> <orderIndex> <localX> <localY> <localZ>
//            <frontDir> <topDir> <jigsawName> <target> <pool> <joint>
//            <placePriority> <selPriority>
// The (offset, rotation, seed) battery is fixed below and mirrored in the C++ test.

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Base64;
import java.util.List;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.util.Util;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.JigsawBlock;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.entity.JigsawBlockEntity;
import net.minecraft.world.level.levelgen.structure.pools.SinglePoolElement;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;

public final class StructureTemplateLoaderParity {

    private static final String[] TEMPLATES = {
        "base_plate",
        "feature_cage1",
        "feature_cage2",
        "feature_cage_with_allays",
        "feature_logs",
        "feature_plate",
        "feature_targets",
        "feature_tent1",
        "feature_tent2",
        "watchtower",
        "watchtower_overgrown",
    };

    // Fixed (offset, rotation, seed) battery — mirrored EXACTLY in the C++ test.
    private record JigsawCase(int offX, int offY, int offZ, Rotation rotation, long seed) {}

    private static List<JigsawCase> cases() {
        List<JigsawCase> list = new ArrayList<>();
        int[][] offsets = {
            {0, 0, 0}, {3, -7, 11}, {-13, 64, -5}, {100, 0, -100}, {-1, -1, -1},
        };
        Rotation[] rots = Rotation.values(); // NONE, CLOCKWISE_90, CLOCKWISE_180, COUNTERCLOCKWISE_90
        long[] seeds = {
            0L, 1L, 2L, 7L, 42L, 99L, 12345L, -1L, -42L, 123456789L,
            9223372036854775807L, -9223372036854775808L,
        };
        for (int[] off : offsets) {
            for (Rotation rot : rots) {
                for (long seed : seeds) {
                    list.add(new JigsawCase(off[0], off[1], off[2], rot, seed));
                }
            }
        }
        return list;
    }

    @SuppressWarnings({"deprecation", "unchecked"})
    public static void main(final String[] args) throws Exception {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        final HolderGetter<Block> blockLookup = BuiltInRegistries.BLOCK;

        // The REAL @VisibleForTesting static SinglePoolElement.sortBySelectionPriority.
        Method sortBySelectionPriority = SinglePoolElement.class.getDeclaredMethod(
            "sortBySelectionPriority", List.class);
        sortBySelectionPriority.setAccessible(true);

        final java.nio.file.Path jarPath = java.nio.file.Path.of("26.1.2", "client.jar");
        try (java.util.zip.ZipFile jar = new java.util.zip.ZipFile(jarPath.toFile())) {
            for (final String name : TEMPLATES) {
                final String entryName = "data/minecraft/structure/pillager_outpost/" + name + ".nbt";
                final java.util.zip.ZipEntry entry = jar.getEntry(entryName);
                if (entry == null) {
                    throw new IllegalStateException("missing structure nbt: " + entryName);
                }

                // Raw gzip bytes -> base64 (the C++ test re-parses these exact bytes).
                final byte[] raw;
                try (java.io.InputStream in = jar.getInputStream(entry)) {
                    raw = in.readAllBytes();
                }
                final String b64 = Base64.getEncoder().encodeToString(raw);

                // The REAL StructureTemplate.load over the SAME bytes.
                final CompoundTag tag;
                try (java.io.InputStream in = jar.getInputStream(entry)) {
                    tag = NbtIo.readCompressed(in, NbtAccounter.unlimitedHeap());
                }
                final StructureTemplate template = new StructureTemplate();
                template.load(blockLookup, tag);

                final net.minecraft.core.Vec3i size = template.getSize();

                // Total StructureBlockInfo count in the (single) palette built by the REAL
                // loader: palettes.get(0).blocks().size() (read reflectively below).
                final int numBlockInfos = totalBlockInfoCount(template);

                out.println("TEMPLATE\t" + name + "\t" + b64
                    + "\t" + size.getX() + "\t" + size.getY() + "\t" + size.getZ()
                    + "\t" + numBlockInfos);

                for (final JigsawCase c : cases()) {
                    final BlockPos position = new BlockPos(c.offX(), c.offY(), c.offZ());

                    // The REAL SinglePoolElement.getShuffledJigsawBlocks pipeline:
                    //   getJigsaws(position, rotation); Util.shuffle(list, random);
                    //   sortBySelectionPriority(list);
                    final List<StructureTemplate.JigsawBlockInfo> jigsaws =
                        template.getJigsaws(position, c.rotation());
                    final RandomSource random = RandomSource.create(c.seed());
                    Util.shuffle(jigsaws, random);
                    sortBySelectionPriority.invoke(null, jigsaws);

                    for (int i = 0; i < jigsaws.size(); i++) {
                        final StructureTemplate.JigsawBlockInfo j = jigsaws.get(i);
                        final StructureTemplate.StructureBlockInfo info = j.info();
                        final BlockPos lp = info.pos();
                        final Direction front = JigsawBlock.getFrontFacing(info.state());
                        final Direction top = JigsawBlock.getTopFacing(info.state());
                        final JigsawBlockEntity.JointType joint = j.jointType();
                        final Identifier jigsawName = j.name();
                        final Identifier target = j.target();
                        final ResourceKey<StructureTemplatePool> pool = j.pool();

                        out.println("JIGSAW\t" + name
                            + "\t" + c.rotation().ordinal()
                            + "\t" + c.seed()
                            + "\t" + i
                            + "\t" + lp.getX() + "\t" + lp.getY() + "\t" + lp.getZ()
                            + "\t" + front.getName()
                            + "\t" + top.getName()
                            + "\t" + jigsawName.toString()
                            + "\t" + target.toString()
                            + "\t" + pool.identifier().toString()
                            + "\t" + joint.getSerializedName()
                            + "\t" + j.placementPriority()
                            + "\t" + j.selectionPriority());
                    }
                }
            }
        }

        out.flush();
    }

    // Count the StructureBlockInfo entries in the template's (single) palette via the
    // REAL StructureTemplate internals (palettes.get(0).blocks().size()). Read through
    // reflection on the private `palettes` field + the package-private Palette.blocks().
    @SuppressWarnings("unchecked")
    private static int totalBlockInfoCount(final StructureTemplate template) throws Exception {
        java.lang.reflect.Field palettesField = StructureTemplate.class.getDeclaredField("palettes");
        palettesField.setAccessible(true);
        List<Object> palettes = (List<Object>) palettesField.get(template);
        if (palettes.isEmpty()) {
            return 0;
        }
        Object palette = palettes.get(0);
        Method blocks = palette.getClass().getDeclaredMethod("blocks");
        blocks.setAccessible(true);
        List<StructureTemplate.StructureBlockInfo> blockList =
            (List<StructureTemplate.StructureBlockInfo>) blocks.invoke(palette);
        return blockList.size();
    }
}
