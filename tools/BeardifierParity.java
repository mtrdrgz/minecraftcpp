// Ground truth for the C++ Beardifier port. Drives the REAL
// net.minecraft.world.level.levelgen.Beardifier (its @VisibleForTesting constructor)
// with randomly generated Rigid pieces + JigsawJunctions + an affected box, samples
// compute() over a grid, and emits the cases + bit-exact expected values as TSV.
//
//   javac -cp client.jar -d out tools/BeardifierParity.java
//   java -cp out:client.jar BeardifierParity > build/beardifier.tsv
//
// Row formats:
//   CASE <aMinX aMinY aMinZ aMaxX aMaxY aMaxZ> <nPieces> [<minX minY minZ maxX maxY maxZ adj gld>]... <nJunc> [<jx jgy jz>]...
//   S <x> <y> <z> <Double.doubleToRawLongBits(compute)>
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import net.minecraft.world.level.levelgen.Beardifier;
import net.minecraft.world.level.levelgen.DensityFunction;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.TerrainAdjustment;
import net.minecraft.world.level.levelgen.structure.pools.JigsawJunction;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;

public class BeardifierParity {
   static final TerrainAdjustment[] ADJ = {
      TerrainAdjustment.NONE, TerrainAdjustment.BURY, TerrainAdjustment.BEARD_THIN,
      TerrainAdjustment.BEARD_BOX, TerrainAdjustment.ENCAPSULATE
   };

   public static void main(String[] args) {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();
      Random rng = new Random(0xBEA12DL);
      StringBuilder sb = new StringBuilder();
      int cases = 400;

      for (int c = 0; c < cases; c++) {
         // a piece cluster around a random centre
         int cx = rng.nextInt(64) - 32;
         int cy = 60 + rng.nextInt(20);
         int cz = rng.nextInt(64) - 32;

         int nPieces = 1 + rng.nextInt(3);
         List<Beardifier.Rigid> pieces = new ArrayList<>();
         BoundingBox any = null;
         for (int p = 0; p < nPieces; p++) {
            int x0 = cx + rng.nextInt(16) - 8;
            int y0 = cy + rng.nextInt(8) - 4;
            int z0 = cz + rng.nextInt(16) - 8;
            int bx = x0 + 1 + rng.nextInt(10);
            int by = y0 + 1 + rng.nextInt(8);
            int bz = z0 + 1 + rng.nextInt(10);
            BoundingBox box = new BoundingBox(x0, y0, z0, bx, by, bz);
            TerrainAdjustment adj = ADJ[1 + rng.nextInt(4)]; // exclude NONE
            int gld = rng.nextInt(6) - 1;
            pieces.add(new Beardifier.Rigid(box, adj, gld));
            any = any == null ? box : BoundingBox.encapsulating(any, box);
         }

         int nJunc = rng.nextInt(3);
         List<JigsawJunction> junctions = new ArrayList<>();
         for (int jn = 0; jn < nJunc; jn++) {
            int jx = cx + rng.nextInt(16) - 8;
            int jgy = cy + rng.nextInt(8) - 4;
            int jz = cz + rng.nextInt(16) - 8;
            junctions.add(new JigsawJunction(jx, jgy, jz, 0, StructureTemplatePool.Projection.RIGID));
            BoundingBox jb = new BoundingBox(new net.minecraft.core.BlockPos(jx, jgy, jz));
            any = any == null ? jb : BoundingBox.encapsulating(any, jb);
         }

         BoundingBox affected = any.inflatedBy(24);
         Beardifier beard = new Beardifier(List.copyOf(pieces), List.copyOf(junctions), affected);

         sb.append("CASE\t").append(affected.minX()).append('\t').append(affected.minY()).append('\t')
           .append(affected.minZ()).append('\t').append(affected.maxX()).append('\t').append(affected.maxY())
           .append('\t').append(affected.maxZ()).append('\t').append(pieces.size());
         for (Beardifier.Rigid r : pieces) {
            sb.append('\t').append(r.box().minX()).append('\t').append(r.box().minY()).append('\t').append(r.box().minZ())
              .append('\t').append(r.box().maxX()).append('\t').append(r.box().maxY()).append('\t').append(r.box().maxZ())
              .append('\t').append(adjIndex(r.terrainAdjustment())).append('\t').append(r.groundLevelDelta());
         }
         sb.append('\t').append(junctions.size());
         for (JigsawJunction j : junctions) {
            sb.append('\t').append(j.getSourceX()).append('\t').append(j.getSourceGroundY()).append('\t').append(j.getSourceZ());
         }
         sb.append('\n');

         // sample inside + edges of the affected box
         for (int s = 0; s < 20; s++) {
            int x = affected.minX() + rng.nextInt(Math.max(1, affected.maxX() - affected.minX() + 1));
            int y = affected.minY() + rng.nextInt(Math.max(1, affected.maxY() - affected.minY() + 1));
            int z = affected.minZ() + rng.nextInt(Math.max(1, affected.maxZ() - affected.minZ() + 1));
            double v = beard.compute(new DensityFunction.SinglePointContext(x, y, z));
            sb.append("S\t").append(x).append('\t').append(y).append('\t').append(z).append('\t')
              .append(Double.doubleToRawLongBits(v)).append('\n');
         }
      }
      System.out.print(sb);
   }

   static int adjIndex(TerrainAdjustment a) {
      for (int i = 0; i < ADJ.length; i++) if (ADJ[i] == a) return i;
      return 0;
   }
}
