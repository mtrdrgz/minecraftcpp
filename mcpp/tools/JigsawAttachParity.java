// Ground-truth generator for the jigsaw ATTACHMENT predicate
//   net.minecraft.world.level.block.JigsawBlock.canAttach(JigsawBlockInfo, JigsawBlockInfo)
// and the rotation shuffle
//   net.minecraft.world.level.block.Rotation.getShuffled(RandomSource)
// (Minecraft Java Edition 26.1.2), used by the jigsaw structure-assembly placer.
//
// It drives the REAL classes:
//   * Each JigsawBlockInfo is built the vanilla way: a real jigsaw BlockState with a
//     chosen FrontAndTop ORIENTATION, wrapped in a StructureTemplate.StructureBlockInfo
//     together with a CompoundTag carrying name / target / joint (stored via the same
//     Identifier.CODEC / JointType.CODEC the engine round-trips), then resolved with
//     StructureTemplate.JigsawBlockInfo.of(info).  canAttach is then the real static.
//   * Rotation.getShuffled is called for ~200 seeds over a LegacyRandomSource, dumping
//     the 4-rotation order (by ordinal).
//
// Row format (tab-separated, leading TAG):
//   ROTORD   <name>  <ordinal>                         -- Rotation enum ordinals (lock order)
//   JOINTORD <name>  <ordinal>                         -- JointType enum ordinals (lock order)
//   FAT      <name>  <frontOrdinal> <topOrdinal>       -- FrontAndTop front()/top() Directions
//   ATTACH   <sFrontOrd> <sTopOrd> <sJointOrd> <sTarget> <tFrontOrd> <tTopOrd> <tName>  <result01>
//   SHUF     <seed>  <ord0> <ord1> <ord2> <ord3>       -- getShuffled(LegacyRandomSource(seed))
//
// Direction ordinals: DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5.
//
//   tools/run_groundtruth.ps1 -Tool JigsawAttachParity -Out mcpp/build/jigsaw_attach.tsv

import java.io.PrintStream;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.FrontAndTop;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.JigsawBlock;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.entity.JigsawBlockEntity;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;

public class JigsawAttachParity {
    static final PrintStream O = System.out;

    // Build a real JigsawBlockInfo for the given ORIENTATION + joint + name + target,
    // exactly as the engine resolves one from a placed jigsaw block: a real jigsaw
    // BlockState with ORIENTATION set, wrapped with a CompoundTag carrying
    // joint/name/target through the same codecs JigsawBlockInfo.of(...) reads.
    @SuppressWarnings({"unchecked", "deprecation"})
    static StructureTemplate.JigsawBlockInfo build(FrontAndTop orientation,
                                                   JigsawBlockEntity.JointType joint,
                                                   String name, String target) {
        BlockState state = Blocks.JIGSAW.defaultBlockState().setValue(JigsawBlock.ORIENTATION, orientation);
        CompoundTag nbt = new CompoundTag();
        nbt.store("name", Identifier.CODEC, Identifier.parse(name));
        nbt.store("target", Identifier.CODEC, Identifier.parse(target));
        nbt.store("joint", JigsawBlockEntity.JointType.CODEC, joint);
        StructureTemplate.StructureBlockInfo info =
                new StructureTemplate.StructureBlockInfo(BlockPos.ZERO, state, nbt);
        return StructureTemplate.JigsawBlockInfo.of(info);
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // (1) Enum ordinals — lock the orderings the TSV exchanges depend on.
        for (Rotation r : Rotation.values()) {
            O.println("ROTORD\t" + r.name() + "\t" + r.ordinal());
        }
        for (JigsawBlockEntity.JointType j : JigsawBlockEntity.JointType.values()) {
            O.println("JOINTORD\t" + j.name() + "\t" + j.ordinal());
        }

        // (2) FrontAndTop front()/top() — every constant.
        for (FrontAndTop fat : FrontAndTop.values()) {
            O.println("FAT\t" + fat.name() + "\t" + fat.front().ordinal() + "\t" + fat.top().ordinal());
        }

        // (3) canAttach — full battery over the orientation x joint x name/target space.
        // Identifiers used for source.target() vs target.name() comparisons; pick a
        // small set with matches and mismatches across namespaces and paths.
        FrontAndTop[] FATS = FrontAndTop.values();
        JigsawBlockEntity.JointType[] JOINTS = JigsawBlockEntity.JointType.values();
        String[] IDS = {
            "minecraft:empty",
            "minecraft:village/plains/houses",
            "minecraft:village/plains/streets",
            "foo:bar",
        };

        for (FrontAndTop sFat : FATS) {
            for (JigsawBlockEntity.JointType sJoint : JOINTS) {
                for (String sTarget : IDS) {
                    for (FrontAndTop tFat : FATS) {
                        for (String tName : IDS) {
                            // target's joint is irrelevant to canAttach (only source.jointType()
                            // is read); use ROLLABLE and an irrelevant target/name on the target.
                            StructureTemplate.JigsawBlockInfo source =
                                    build(sFat, sJoint, "minecraft:src_name", sTarget);
                            StructureTemplate.JigsawBlockInfo target =
                                    build(tFat, JigsawBlockEntity.JointType.ROLLABLE, tName, "minecraft:tgt_target");
                            boolean res = JigsawBlock.canAttach(source, target);

                            Direction sFront = JigsawBlock.getFrontFacing(source.info().state());
                            Direction sTop = JigsawBlock.getTopFacing(source.info().state());
                            Direction tFront = JigsawBlock.getFrontFacing(target.info().state());
                            Direction tTop = JigsawBlock.getTopFacing(target.info().state());

                            O.println("ATTACH\t" + sFront.ordinal() + "\t" + sTop.ordinal()
                                    + "\t" + source.jointType().ordinal() + "\t" + sTarget
                                    + "\t" + tFront.ordinal() + "\t" + tTop.ordinal() + "\t" + tName
                                    + "\t" + (res ? 1 : 0));
                        }
                    }
                }
            }
        }

        // (4) Rotation.getShuffled over ~200 seeds (LegacyRandomSource), dumping the order.
        for (long seed = 0; seed < 200; seed++) {
            RandomSource random = RandomSource.create(seed);
            java.util.List<Rotation> order = Rotation.getShuffled(random);
            StringBuilder sb = new StringBuilder("SHUF\t").append(seed);
            for (Rotation r : order) {
                sb.append('\t').append(r.ordinal());
            }
            O.println(sb.toString());
        }
    }
}
