// Ground truth for the private enum TABLES of net.minecraft.client.renderer.block.BlockModelLighter
// (AdjacencyInfo / AmbientVertexRemap / SizeInfo). Reflection-dumps the REAL enum constants' fields
// so the C++ transcription (render/block/BlockModelLighterTables.h) is verified bit-for-bit.
//
//   tools/run_groundtruth.ps1 -Tool BlockModelLighterTablesParity -Out mcpp/build/bml_tables.tsv
//
// Rows (tab-separated, ints):
//   SIZE  <name>  <index>
//   ADJ   <dirOrdinal>  c0 c1 c2 c3  doNonCubic  v0[0..7]  v1[0..7]  v2[0..7]  v3[0..7]
//   REMAP <dirOrdinal>  vert0 vert1 vert2 vert3

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.core.Direction;

public class BlockModelLighterTablesParity {
    static final java.io.PrintStream O = System.out;
    static final String PKG = "net.minecraft.client.renderer.block.BlockModelLighter$";

    static int sizeIndex(Object sizeInfo, Field idxField) throws Exception {
        return idxField.getInt(sizeInfo);
    }

    public static void main(String[] args) throws Exception {
        Class<?> sizeCls = Class.forName(PKG + "SizeInfo");
        Class<?> adjCls = Class.forName(PKG + "AdjacencyInfo");
        Class<?> remapCls = Class.forName(PKG + "AmbientVertexRemap");

        Field sizeIdxField = sizeCls.getDeclaredField("index");
        sizeIdxField.setAccessible(true);

        // SIZE: dump each SizeInfo constant's index.
        for (Object c : sizeCls.getEnumConstants()) {
            O.println("SIZE\t" + ((Enum<?>) c).name() + "\t" + sizeIdxField.getInt(c));
        }

        // AdjacencyInfo fields.
        Method adjFromFacing = adjCls.getDeclaredMethod("fromFacing", Direction.class);
        adjFromFacing.setAccessible(true);
        Field cornersF = adjCls.getDeclaredField("corners");
        Field doNonCubicF = adjCls.getDeclaredField("doNonCubicWeight");
        Field[] vertF = new Field[]{
            adjCls.getDeclaredField("vert0Weights"), adjCls.getDeclaredField("vert1Weights"),
            adjCls.getDeclaredField("vert2Weights"), adjCls.getDeclaredField("vert3Weights")};
        cornersF.setAccessible(true); doNonCubicF.setAccessible(true);
        for (Field vf : vertF) vf.setAccessible(true);

        Method remapFromFacing = remapCls.getDeclaredMethod("fromFacing", Direction.class);
        remapFromFacing.setAccessible(true);
        Field[] vertRemapF = new Field[]{
            remapCls.getDeclaredField("vert0"), remapCls.getDeclaredField("vert1"),
            remapCls.getDeclaredField("vert2"), remapCls.getDeclaredField("vert3")};
        for (Field vf : vertRemapF) vf.setAccessible(true);

        Direction[] dirs = Direction.values();  // DOWN..EAST = ordinals 0..5
        for (Direction d : dirs) {
            Object adj = adjFromFacing.invoke(null, d);
            Direction[] corners = (Direction[]) cornersF.get(adj);
            boolean doNonCubic = doNonCubicF.getBoolean(adj);
            StringBuilder sb = new StringBuilder("ADJ\t").append(d.ordinal());
            for (Direction c : corners) sb.append('\t').append(c.ordinal());
            sb.append('\t').append(doNonCubic ? 1 : 0);
            for (Field vf : vertF) {
                Object[] weights = (Object[]) vf.get(adj);
                for (Object w : weights) sb.append('\t').append(sizeIdxField.getInt(w));
            }
            O.println(sb);

            Object remap = remapFromFacing.invoke(null, d);
            O.println("REMAP\t" + d.ordinal()
                + "\t" + vertRemapF[0].getInt(remap) + "\t" + vertRemapF[1].getInt(remap)
                + "\t" + vertRemapF[2].getInt(remap) + "\t" + vertRemapF[3].getInt(remap));
        }
    }
}
