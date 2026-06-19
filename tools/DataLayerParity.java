// Ground-truth generator for net.minecraft.world.level.chunk.DataLayer — the 4-bit-per-
// cell nibble array (2048 bytes / 4096 cells) used for block-light & sky-light. Pure
// int/byte; no Bootstrap. Calls the REAL class (reflection for the private statics
// getIndex/get(int)/set(int,int)/getNibbleIndex/getByteIndex/packFilled). Emits, per
// case, getIndex/getNibbleIndex/getByteIndex/packFilled values, the homogenous-default
// get() sweep, the byte[]-ctor get()+raw-byte dump, a set()-sequence get()+raw dump, and
// the homogeneity predicates. The C++ test recomputes and must match bit-for-bit.
//
//   tools/run_groundtruth.ps1 -Tool DataLayerParity -Out mcpp/build/data_layer.tsv

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import net.minecraft.world.level.chunk.DataLayer;

public class DataLayerParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // ---- reflection handles --------------------------------------------------
        Method getIndexM   = DataLayer.class.getDeclaredMethod("getIndex", int.class, int.class, int.class);
        Method nibbleIdxM  = DataLayer.class.getDeclaredMethod("getNibbleIndex", int.class);
        Method byteIdxM    = DataLayer.class.getDeclaredMethod("getByteIndex", int.class);
        Method packFilledM = DataLayer.class.getDeclaredMethod("packFilled", int.class);
        getIndexM.setAccessible(true);
        nibbleIdxM.setAccessible(true);
        byteIdxM.setAccessible(true);
        packFilledM.setAccessible(true);

        Constructor<DataLayer> ctorBytes   = DataLayer.class.getConstructor(byte[].class);
        Constructor<DataLayer> ctorDefault = DataLayer.class.getConstructor(int.class);
        Constructor<DataLayer> ctorEmpty   = DataLayer.class.getConstructor();

        Method getXYZ = DataLayer.class.getMethod("get", int.class, int.class, int.class);
        Method setXYZ = DataLayer.class.getMethod("set", int.class, int.class, int.class, int.class);
        Method getDataM   = DataLayer.class.getMethod("getData");
        Method isHomoM    = DataLayer.class.getMethod("isDefinitelyHomogenous");
        Method isFilledM  = DataLayer.class.getMethod("isDefinitelyFilledWith", int.class);
        Method isEmptyM   = DataLayer.class.getMethod("isEmpty");

        // ---- GETIDX: getIndex(x,y,z) over the valid 0..15 cube plus a few extremes -
        // (also exercises the bit packing with out-of-range inputs).
        int[] coords = { 0, 1, 2, 7, 8, 14, 15 };
        for (int x : coords)
            for (int y : coords)
                for (int z : coords)
                    O.println("GETIDX\t" + x + "\t" + y + "\t" + z + "\t" + (int) getIndexM.invoke(null, x, y, z));
        int[] edge = { -1, 16, 31, 255, 256 };
        for (int x : edge)
            for (int z : edge)
                O.println("GETIDX\t" + x + "\t0\t" + z + "\t" + (int) getIndexM.invoke(null, x, 0, z));

        // ---- NIBBLE / BYTEIDX over all 4096 cell indices --------------------------
        for (int idx = 0; idx < 4096; idx++) {
            O.println("NIBBLE\t" + idx + "\t" + (int) nibbleIdxM.invoke(null, idx));
            O.println("BYTEIDX\t" + idx + "\t" + (int) byteIdxM.invoke(null, idx));
        }

        // ---- PACK: packFilled(value) — signed byte, decimal --------------------------
        for (int v = 0; v <= 15; v++)
            O.println("PACK\t" + v + "\t" + (byte) packFilledM.invoke(null, v));
        for (int v : new int[]{ -1, 16, 127, -128, 255, 256 })
            O.println("PACK\t" + v + "\t" + (byte) packFilledM.invoke(null, v));

        // ---- DEFAULT: homogenous DataLayer(defaultValue) — get() over all 4096, then
        // force getData() and dump raw bytes (tests the packFilled fill + read path).
        for (int dv : new int[]{ 0, 1, 7, 15 }) {
            DataLayer dl = ctorDefault.newInstance(dv);
            O.println("DHOMO\t" + dv + "\t" + (boolean) isHomoM.invoke(dl));
            O.println("DEMPTY\t" + dv + "\t" + (boolean) isEmptyM.invoke(dl));
            O.println("DFILL\t" + dv + "\t" + dv + "\t" + (boolean) isFilledM.invoke(dl, dv));
            for (int idx = 0; idx < 4096; idx++) {
                int gx = idx & 15, gz = (idx >> 4) & 15, gy = (idx >> 8) & 15;
                O.println("DGET\t" + dv + "\t" + idx + "\t" + (int) getXYZ.invoke(dl, gx, gy, gz));
            }
            byte[] raw = (byte[]) getDataM.invoke(dl);
            O.println("DRAWLEN\t" + dv + "\t" + raw.length);
            for (int b = 0; b < raw.length; b++)
                O.println("DRAW\t" + dv + "\t" + b + "\t" + raw[b]);
            // after getData() it is no longer homogenous
            O.println("DHOMO2\t" + dv + "\t" + (boolean) isHomoM.invoke(dl));
        }

        // ---- CTOR: DataLayer(byte[]) from a deterministic 2048-byte pattern --------
        byte[] pattern = new byte[2048];
        for (int b = 0; b < 2048; b++)
            pattern[b] = (byte) ((b * 31 + 7) & 0xFF);   // full 0..255 signed-byte range
        DataLayer dlc = ctorBytes.newInstance((Object) pattern);
        for (int idx = 0; idx < 4096; idx++) {
            int gx = idx & 15, gz = (idx >> 4) & 15, gy = (idx >> 8) & 15;
            O.println("CGET\t" + idx + "\t" + (int) getXYZ.invoke(dlc, gx, gy, gz));
        }
        byte[] rawc = (byte[]) getDataM.invoke(dlc);
        for (int b = 0; b < rawc.length; b++)
            O.println("CRAW\t" + b + "\t" + rawc[b]);

        // ---- SETSEQ: empty DataLayer, deterministic set(x,y,z,val) sequence over all
        // 4096 cells (lazy alloc + nibble masking), then get() sweep + raw byte dump.
        DataLayer dls = ctorEmpty.newInstance();
        O.println("SHOMO\t" + (boolean) isHomoM.invoke(dls));   // true before any set
        for (int idx = 0; idx < 4096; idx++) {
            int gx = idx & 15, gz = (idx >> 4) & 15, gy = (idx >> 8) & 15;
            int val = (idx * 13 + 5) & 15;                       // 0..15
            setXYZ.invoke(dls, gx, gy, gz, val);
        }
        for (int idx = 0; idx < 4096; idx++) {
            int gx = idx & 15, gz = (idx >> 4) & 15, gy = (idx >> 8) & 15;
            O.println("SGET\t" + idx + "\t" + (int) getXYZ.invoke(dls, gx, gy, gz));
        }
        byte[] raws = (byte[]) getDataM.invoke(dls);
        for (int b = 0; b < raws.length; b++)
            O.println("SRAW\t" + b + "\t" + raws[b]);
        O.println("SHOMO2\t" + (boolean) isHomoM.invoke(dls));  // false after set

        // ---- SETOVR: overwrite the same cell repeatedly (masking must clear nibble) -
        DataLayer dlo = ctorEmpty.newInstance();
        int[] ovrVals = { 0, 15, 1, 8, 5, 0, 12, 3, 15, 7 };
        for (int v : ovrVals) {
            setXYZ.invoke(dlo, 3, 9, 6, v);                      // a fixed (x,y,z)
            O.println("SETOVR\t" + v + "\t" + (int) getXYZ.invoke(dlo, 3, 9, 6));
        }
        // neighbour cell (shares a byte: index differs only in low bit) stays 0
        O.println("SETNBR\t" + (int) getXYZ.invoke(dlo, 2, 9, 6));

        // ---- val>15 wraps via (val & 15) in set() ---------------------------------
        DataLayer dlw = ctorEmpty.newInstance();
        for (int v : new int[]{ 16, 17, 31, 255, -1, -16, 256 }) {
            setXYZ.invoke(dlw, 0, 0, 0, v);
            O.println("SETWRAP\t" + v + "\t" + (int) getXYZ.invoke(dlw, 0, 0, 0));
        }
    }
}
