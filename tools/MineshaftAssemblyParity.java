// MineshaftAssemblyParity.java — ground truth for the FULL recursive mineshaft
// assembly (not just the per-box helpers already covered by
// MineShaftCorridorParity / MineShaftCrossingBox / MineShaftRoomBox /
// MineshaftStairsBox). Drives the REAL MineshaftPieces classes:
//
//   random = new WorldgenRandom(new LegacyRandomSource(0)); setLargeFeatureSeed(seed,cx,cz)
//   random.nextDouble();                                   // MineshaftStructure.findGenerationPoint head
//   builder = new StructurePiecesBuilder();
//   room    = new MineShaftRoom(0, random, cx*16+2, cz*16+2, type);
//   builder.addPiece(room); room.addChildren(room, builder, random);   // depth-first recursion
//   dy = builder.moveBelowSeaLevel(seaLevel=63, minY=-64, random, 10);  // NORMAL adjust
//
// then dumps every piece (post-adjust) as a TSV the C++ mineshaft_assembly_parity
// test byte-matches. This certifies the RNG-exact recursion (createRandomShaftPiece
// + generateAndAddPiece + the four addChildren), which the block-placement port
// then builds on.
//
//   javac -cp "<cp>" -d <classes> tools/MineshaftAssemblyParity.java
//   java  -cp "<cp>" MineshaftAssemblyParity <seed> <cx,cz> [<cx,cz> ...]
//        (NORMAL type; one "CASE\tseed\tcx\tcz" header precedes each assembly)
//
// Output columns (TSV): idx  kind  minX minY minZ maxX maxY maxZ  orient  genDepth  flagA flagB flagC
//   kind: ROOM|CORRIDOR|CROSSING|STAIRS
//   orient: Direction.ordinal() of getOrientation(), or -1 if null
//   ROOM     -> flags: -1 -1 -1
//   CORRIDOR -> flags: hasRails(0/1) spiderCorridor(0/1) numSections
//   CROSSING -> flags: isTwoFloored(0/1) directionOrdinal -1
//   STAIRS   -> flags: -1 -1 -1
import java.lang.reflect.Field;
import java.util.List;
import net.minecraft.server.Bootstrap;
import net.minecraft.SharedConstants;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.levelgen.structure.structures.MineshaftStructure;
import net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces;

public class MineshaftAssemblyParity {
    static int reflectInt(Object o, String field) throws Exception {
        Class<?> c = o.getClass();
        while (c != null) {
            try { Field f = c.getDeclaredField(field); f.setAccessible(true); return f.getInt(o); }
            catch (NoSuchFieldException e) { c = c.getSuperclass(); }
        }
        throw new NoSuchFieldException(field);
    }
    static boolean reflectBool(Object o, String field) throws Exception {
        Class<?> c = o.getClass();
        while (c != null) {
            try { Field f = c.getDeclaredField(field); f.setAccessible(true); return f.getBoolean(o); }
            catch (NoSuchFieldException e) { c = c.getSuperclass(); }
        }
        throw new NoSuchFieldException(field);
    }
    static Object reflectObj(Object o, String field) throws Exception {
        Class<?> c = o.getClass();
        while (c != null) {
            try { Field f = c.getDeclaredField(field); f.setAccessible(true); return f.get(o); }
            catch (NoSuchFieldException e) { c = c.getSuperclass(); }
        }
        throw new NoSuchFieldException(field);
    }

    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();
        java.io.PrintStream out = new java.io.PrintStream(new java.io.FileOutputStream(java.io.FileDescriptor.out), false, "US-ASCII");

        long seed = args.length > 0 ? Long.parseLong(args[0]) : 1L;
        java.util.List<int[]> cases = new java.util.ArrayList<>();
        for (int ai = 1; ai < args.length; ai++) {
            String[] p = args[ai].split(",");
            cases.add(new int[]{ Integer.parseInt(p[0].trim()), Integer.parseInt(p[1].trim()) });
        }
        if (cases.isEmpty()) cases.add(new int[]{ 0, 0 });

        MineshaftStructure.Type type = MineshaftStructure.Type.NORMAL;
        for (int[] c : cases) {
            dumpCase(out, seed, c[0], c[1], type);
        }
        out.flush();
    }

    static void dumpCase(java.io.PrintStream out, long seed, int cx, int cz,
                         MineshaftStructure.Type type) throws Exception {
        WorldgenRandom random = new WorldgenRandom(new LegacyRandomSource(0L));
        random.setLargeFeatureSeed(seed, cx, cz);
        random.nextDouble();  // MineshaftStructure.findGenerationPoint: context.random().nextDouble()

        StructurePiecesBuilder builder = new StructurePiecesBuilder();
        MineshaftPieces.MineShaftRoom room =
            new MineshaftPieces.MineShaftRoom(0, random, (cx << 4) + 2, (cz << 4) + 2, type);
        builder.addPiece(room);
        room.addChildren(room, builder, random);

        // NORMAL adjust path (generatePiecesAndAdjust else-branch): seaLevel=63, minY=-64.
        builder.moveBelowSeaLevel(63, -64, random, 10);

        out.println("CASE\t" + seed + "\t" + cx + "\t" + cz);
        @SuppressWarnings("unchecked")
        List<StructurePiece> pieces = (List<StructurePiece>) reflectObj(builder, "pieces");
        int idx = 0;
        for (StructurePiece p : pieces) {
            BoundingBox b = p.getBoundingBox();
            String cls = p.getClass().getSimpleName();   // MineShaftRoom / MineShaftCorridor / ...
            String kind = cls.replace("MineShaft", "").toUpperCase();
            Direction orient = p.getOrientation();
            int orientOrd = orient == null ? -1 : orient.ordinal();
            int genDepth = reflectInt(p, "genDepth");
            int fa = -1, fb = -1, fc = -1;
            if (cls.equals("MineShaftCorridor")) {
                fa = reflectBool(p, "hasRails") ? 1 : 0;
                fb = reflectBool(p, "spiderCorridor") ? 1 : 0;
                fc = reflectInt(p, "numSections");
            } else if (cls.equals("MineShaftCrossing")) {
                fa = reflectBool(p, "isTwoFloored") ? 1 : 0;
                Direction d = (Direction) reflectObj(p, "direction");
                fb = d == null ? -1 : d.ordinal();
            }
            out.println(idx + "\t" + kind + "\t"
                + b.minX() + "\t" + b.minY() + "\t" + b.minZ() + "\t"
                + b.maxX() + "\t" + b.maxY() + "\t" + b.maxZ() + "\t"
                + orientOrd + "\t" + genDepth + "\t" + fa + "\t" + fb + "\t" + fc);
            idx++;
        }
    }
}
