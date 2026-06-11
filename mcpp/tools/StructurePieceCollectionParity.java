// Ground-truth generator for the PURE list/bbox aggregation helpers in the REAL
// decompiled 26.1.2 sources:
//   net.minecraft.world.level.levelgen.structure.StructurePiece.createBoundingBox(Stream)
//   net.minecraft.world.level.levelgen.structure.StructurePiece.findCollisionPiece(List, box)
//   net.minecraft.world.level.levelgen.structure.BoundingBox.encapsulatingBoxes(Iterable)
//
// These are exactly what StructurePiecesBuilder.getBoundingBox() and
// .findCollisionPiece(box) call (and through them every StructureStart). All
// three are pure: they fold over an ordered sequence of bounding boxes. No world
// writes, no RandomSource, no registry/datapack.
//
// StructurePiece is abstract and its <clinit> builds an ImmutableSet of Blocks,
// so we Bootstrap and define a trivial concrete subclass to carry each box. The
// helpers under test are public static, so they are invoked directly (no
// reflection needed). encapsulatingBoxes is invoked directly too.
//
//   tools/run_groundtruth.ps1 -Tool StructurePieceCollectionParity -Out mcpp/build/structure_piece_collection.tsv
//
// Output columns are decimal ints throughout (pure integer geometry). Row tags:
//   CREATEBB <n> <box6>*n | <minX minY minZ maxX maxY maxZ>   -- createBoundingBox over n piece boxes
//   CREATEBB_EMPTY | THROW                                    -- createBoundingBox over 0 pieces throws
//   ENCB <n> <box6>*n | <PRESENT minX minY minZ maxX maxY maxZ | EMPTY>
//   FINDCP <n> <box6>*n <query6> | <index or -1>              -- findCollisionPiece first intersect

import java.util.ArrayList;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceType;

public class StructurePieceCollectionParity {
    static final java.io.PrintStream OUT = System.out;
    static StructurePieceType ANY_TYPE;

    // Minimal concrete StructurePiece — only its BoundingBox is read by the
    // helpers under test; postProcess/addAdditionalSaveData are never called.
    static final class TestPiece extends StructurePiece {
        TestPiece(StructurePieceType type, BoundingBox bb) { super(type, 0, bb); }
        @Override protected void addAdditionalSaveData(StructurePieceSerializationContext c, CompoundTag t) {}
        @Override public void postProcess(net.minecraft.world.level.WorldGenLevel l,
            net.minecraft.world.level.StructureManager s, ChunkGenerator g,
            net.minecraft.util.RandomSource r, BoundingBox cb, ChunkPos cp, BlockPos rp) {}
    }

    static TestPiece piece(BoundingBox bb) { return new TestPiece(ANY_TYPE, bb); }

    static String box6(BoundingBox b) {
        return b.minX() + "\t" + b.minY() + "\t" + b.minZ() + "\t" + b.maxX() + "\t" + b.maxY() + "\t" + b.maxZ();
    }

    // A representative palette of boxes spanning negatives, zero, overlap, and
    // large magnitudes (kept away from Integer.MIN/MAX so Math.min/max never wrap).
    static final BoundingBox[] PALETTE = {
        new BoundingBox(0, 0, 0, 15, 15, 15),
        new BoundingBox(-7, -3, -11, 5, 9, 20),
        new BoundingBox(-100, 60, -200, -50, 120, -120),
        new BoundingBox(1000000, 0, 1000000, 1000050, 30, 1000050),
        new BoundingBox(-2000000, -64, -2000000, -1999900, 200, -1999900),
        new BoundingBox(16, 0, 16, 31, 64, 31),
        new BoundingBox(-8, 0, -8, 8, 32, 8),
        new BoundingBox(48, -40, 48, 80, 16, 80),
        new BoundingBox(5, 5, 5, 5, 5, 5),
        new BoundingBox(-50000, -100, 70000, -49000, 100, 71000),
    };

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Any valid StructurePieceType for the ctor (irrelevant to the geometry).
        ANY_TYPE = StructurePieceType.MINE_SHAFT_ROOM;

        emitCreateAndEncapsulate();
        emitFindCollisionPiece();

        OUT.flush();
    }

    // createBoundingBox(stream) and encapsulatingBoxes(iterable) over many ordered
    // subsequences of the palette, including the single-box and empty cases.
    static void emitCreateAndEncapsulate() {
        // Index sets defining ordered sequences of palette boxes. Order matters:
        // the seed is the first box and encapsulate folds the rest in order.
        int[][] seqs = {
            {0},
            {8},                         // degenerate 1x1x1 box
            {0, 1},
            {1, 0},                      // reversed order (result identical, but proves order-independence of min/max)
            {0, 1, 2},
            {2, 1, 0},
            {0, 5, 6, 7},
            {3, 4},                      // far-apart positives/negatives
            {4, 3},
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
            {9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
            {2, 4, 9},                   // all-negative-ish span
            {0, 0, 0},                   // duplicates
            {5, 5, 6, 6},
        };

        for (int[] seq : seqs) {
            List<StructurePiece> pieces = new ArrayList<>();
            List<BoundingBox> boxes = new ArrayList<>();
            StringBuilder boxCols = new StringBuilder();
            for (int idx : seq) {
                BoundingBox b = PALETTE[idx];
                pieces.add(piece(b));
                boxes.add(b);
                boxCols.append('\t').append(box6(b));
            }

            // StructurePiece.createBoundingBox(Stream<StructurePiece>)
            BoundingBox created = StructurePiece.createBoundingBox(pieces.stream());
            OUT.println("CREATEBB\t" + seq.length + boxCols + "\t" + box6(created));

            // BoundingBox.encapsulatingBoxes(Iterable<BoundingBox>) directly.
            java.util.Optional<BoundingBox> enc = BoundingBox.encapsulatingBoxes(boxes);
            if (enc.isPresent()) {
                OUT.println("ENCB\t" + seq.length + boxCols + "\tPRESENT\t" + box6(enc.get()));
            } else {
                OUT.println("ENCB\t" + seq.length + boxCols + "\tEMPTY");
            }
        }

        // Empty-input branches: encapsulatingBoxes -> Optional.empty;
        // createBoundingBox -> IllegalStateException.
        java.util.Optional<BoundingBox> encEmpty = BoundingBox.encapsulatingBoxes(new ArrayList<>());
        OUT.println("ENCB\t0\t" + (encEmpty.isPresent() ? "PRESENT" : "EMPTY"));

        boolean threw = false;
        try {
            StructurePiece.createBoundingBox(new ArrayList<StructurePiece>().stream());
        } catch (IllegalStateException e) {
            threw = true;
        }
        OUT.println("CREATEBB_EMPTY\t" + (threw ? "THROW" : "NOTHROW"));
    }

    // findCollisionPiece(list, box): first piece (by list order) whose box
    // intersects the query, else null (-> index -1).
    static void emitFindCollisionPiece() {
        // Several ordered piece lists.
        int[][] lists = {
            {0, 1, 2, 3, 4},
            {4, 3, 2, 1, 0},
            {0, 6, 5},
            {5, 6, 0},                   // overlapping boxes in different order -> different first match
            {3, 4, 9, 2},
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
        };
        // Query boxes: some intersect multiple list members, some none.
        BoundingBox[] queries = {
            new BoundingBox(0, 0, 0, 0, 0, 0),        // hits palette[0]
            new BoundingBox(10, 10, 10, 20, 20, 20),  // hits palette[0] and palette[5]
            new BoundingBox(-9, 0, -9, -8, 0, -8),    // edge-touch palette[6]
            new BoundingBox(2000, 2000, 2000, 2001, 2001, 2001), // hits nothing in palette
            new BoundingBox(1000010, 5, 1000010, 1000020, 10, 1000020), // hits palette[3]
            new BoundingBox(-60, 70, -150, -55, 80, -130), // hits palette[2]
            new BoundingBox(5, 5, 5, 5, 5, 5),        // hits palette[8] (and overlaps 0)
            new BoundingBox(-2000000, -64, -2000000, -1999950, 0, -1999950), // hits palette[4]
        };

        for (int[] listIdx : lists) {
            List<StructurePiece> pieces = new ArrayList<>();
            StringBuilder boxCols = new StringBuilder();
            for (int idx : listIdx) {
                BoundingBox b = PALETTE[idx];
                pieces.add(piece(b));
                boxCols.append('\t').append(box6(b));
            }
            for (BoundingBox q : queries) {
                StructurePiece hit = StructurePiece.findCollisionPiece(pieces, q);
                int hitIndex = -1;
                if (hit != null) {
                    for (int i = 0; i < pieces.size(); i++) {
                        if (pieces.get(i) == hit) { hitIndex = i; break; }
                    }
                }
                OUT.println("FINDCP\t" + listIdx.length + boxCols + "\t" + box6(q) + "\t" + hitIndex);
            }
        }
    }
}
