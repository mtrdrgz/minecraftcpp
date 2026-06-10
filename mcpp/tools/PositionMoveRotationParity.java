// Ground-truth generator for net.minecraft.world.entity.PositionMoveRotation (26.1.2).
//
// Emits tab-separated rows comparing the REAL record logic against the C++ port
// (mcpp/src/world/entity/PositionMoveRotation.h). Doubles are exchanged as raw
// IEEE-754 bit patterns (%016x), floats as %08x, so the gate is bit-for-bit.
//
//   TAGs (all columns are bit-hex unless noted):
//     ABS    <relMask>(int) <sPx sPy sPz sDx sDy sDz>(double) <sYRot sXRot>(float)
//            <cPx cPy cPz cDx cDy cDz>(double) <cYRot cXRot>(float)
//            -> outputs: <oPx oPy oPz oDx oDy oDz>(double) <oYRot oXRot>(float)
//            == PositionMoveRotation.calculateAbsolute(source, change, unpack(relMask))
//     WROT   <Px Py Pz Dx Dy Dz>(double) <yRot xRot newYRot newXRot>(float)
//            -> outputs: <Px Py Pz Dx Dy Dz>(double) <oYRot oXRot>(float)
//            == new PositionMoveRotation(P,D,yRot,xRot).withRotation(newYRot,newXRot)
//     PACK   <relMask>(int) -> <repackedMask>(int)   == Relative.pack(Relative.unpack(mask))
//
// Run via mcpp/tools/run_groundtruth.ps1 -Tool PositionMoveRotationParity -Out mcpp/build/position_move_rotation.tsv

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.EnumSet;
import java.util.Set;

public class PositionMoveRotationParity {
   // Capture stdout at class-load so bootstrap chatter can't pollute the TSV.
   static final java.io.PrintStream O = System.out;

   static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }
   static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

   public static void main(String[] args) throws Exception {
      // Relative's static init pulls ByteBufCodecs (SET_STREAM_CODEC) — bootstrap so
      // every net.minecraft class resolves cleanly.
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      Class<?> pmrC      = Class.forName("net.minecraft.world.entity.PositionMoveRotation");
      Class<?> relativeC = Class.forName("net.minecraft.world.entity.Relative");
      Class<?> vec3C     = Class.forName("net.minecraft.world.phys.Vec3");

      // Canonical record ctor (Vec3 position, Vec3 deltaMovement, float yRot, float xRot).
      Constructor<?> pmrCtor = pmrC.getDeclaredConstructor(vec3C, vec3C, float.class, float.class);
      pmrCtor.setAccessible(true);
      Constructor<?> vecCtor = vec3C.getDeclaredConstructor(double.class, double.class, double.class);
      vecCtor.setAccessible(true);

      // Record accessors.
      Method mPos   = pmrC.getMethod("position");
      Method mDelta = pmrC.getMethod("deltaMovement");
      Method mYRot  = pmrC.getMethod("yRot");
      Method mXRot  = pmrC.getMethod("xRot");
      // Vec3 public fields x,y,z.
      java.lang.reflect.Field fx = vec3C.getField("x");
      java.lang.reflect.Field fy = vec3C.getField("y");
      java.lang.reflect.Field fz = vec3C.getField("z");

      // calculateAbsolute(PositionMoveRotation, PositionMoveRotation, Set<Relative>)
      Method calcAbs = pmrC.getDeclaredMethod("calculateAbsolute", pmrC, pmrC, Set.class);
      calcAbs.setAccessible(true);
      // withRotation(float, float)
      Method withRot = pmrC.getMethod("withRotation", float.class, float.class);
      // Relative.unpack(int) / pack(Set)
      Method relUnpack = relativeC.getDeclaredMethod("unpack", int.class);
      relUnpack.setAccessible(true);
      Method relPack = relativeC.getDeclaredMethod("pack", Set.class);
      relPack.setAccessible(true);

      // Helpers to build a Vec3 / PMR via reflection.
      // ───────────────────────────────────────────────────────────────────────
      // ── Battery: every Relative-mask regime (each flag independently toggled,
      // plus the named unions ALL/ROTATION/DELTA and the empty set), crossed with
      // physical positions, delta movements, and rotations spanning the clamp
      // bounds (-90..90 pitch) and the ROTATE_DELTA path (which runs Vec3.xRot/yRot
      // through the Mth sin/cos table).

      double[][] sourcePos = {
         {0.0, 0.0, 0.0}, {1.5, 64.0, -3.25}, {-128.0, 70.5, 256.0}, {0.1, -0.1, 0.0}
      };
      double[][] sourceDelta = {
         {0.0, 0.0, 0.0}, {0.2, -0.5, 0.1}, {-1.0, 0.3, 2.0}, {0.05, 0.05, -0.05}
      };
      float[]  sourceYRot = { 0.0F, 45.0F, 180.0F, -135.0F, 359.0F };
      float[]  sourceXRot = { 0.0F, 30.0F, -45.0F, 90.0F, -90.0F };

      double[][] changePos = {
         {0.0, 0.0, 0.0}, {10.0, 0.0, -10.0}, {0.5, -0.5, 0.5}
      };
      double[][] changeDelta = {
         {0.0, 0.0, 0.0}, {0.1, 0.2, 0.3}, {-0.4, 0.0, 0.7}
      };
      float[] changeYRot = { 0.0F, 90.0F, -270.0F, 15.5F };
      float[] changeXRot = { 0.0F, 60.0F, -100.0F, 25.0F }; // -100 forces lower clamp; 60+x exercises upper clamp

      // Relative-mask battery: empty, each single flag, and the named groups.
      int[] masks = {
         0,                              // empty
         1 << 0, 1 << 1, 1 << 2,         // X, Y, Z
         1 << 3, 1 << 4,                 // Y_ROT, X_ROT
         1 << 5, 1 << 6, 1 << 7,         // DELTA_X, DELTA_Y, DELTA_Z
         1 << 8,                         // ROTATE_DELTA
         (1 << 3) | (1 << 4),            // ROTATION
         (1 << 5) | (1 << 6) | (1 << 7) | (1 << 8), // DELTA
         (1 << 0) | (1 << 1) | (1 << 2), // position
         (1 << 8) | (1 << 5),            // ROTATE_DELTA + DELTA_X
         (1 << 3) | (1 << 8),            // Y_ROT + ROTATE_DELTA
         (1 << 4) | (1 << 8),            // X_ROT + ROTATE_DELTA
         (1 << 3) | (1 << 4) | (1 << 8), // ROTATION + ROTATE_DELTA
         511                             // ALL
      };

      // PACK round-trip over every mask (and a couple of out-of-range bits that
      // unpack ignores) — exercises Relative.unpack/pack ordinal iteration.
      for (int m : masks) {
         Object set = relUnpack.invoke(null, m);
         int repacked = (Integer) relPack.invoke(null, set);
         O.println("PACK\t" + m + "\t" + repacked);
      }
      for (int extra : new int[]{ 1 << 9, 1 << 12, 0x7FFFFFFF & ~512 }) {
         Object set = relUnpack.invoke(null, extra);
         int repacked = (Integer) relPack.invoke(null, set);
         O.println("PACK\t" + extra + "\t" + repacked);
      }

      // ABS battery. Keep finite & physical; iterate a representative cross-product.
      for (int mi = 0; mi < masks.length; mi++) {
         int m = masks[mi];
         Object relSet = relUnpack.invoke(null, m);
         // To keep the file finite, vary source fully but pin change to a small set
         // that still hits both clamp bounds and the rotate-delta path.
         for (double[] sp : sourcePos) {
            for (double[] sd : sourceDelta) {
               for (float syr : sourceYRot) {
                  for (float sxr : sourceXRot) {
                     // pick one change combo per (syr,sxr) cycle to bound size
                     int ci = (Math.abs((int) syr) + Math.abs((int) sxr)) % changePos.length;
                     int cdi = (Math.abs((int) sxr)) % changeDelta.length;
                     int cyi = (Math.abs((int) syr)) % changeYRot.length;
                     int cxi = (Math.abs((int) sxr) + 1) % changeXRot.length;
                     double[] cp = changePos[ci];
                     double[] cd = changeDelta[cdi];
                     float cyr = changeYRot[cyi];
                     float cxr = changeXRot[cxi];

                     Object sPos   = vecCtor.newInstance(sp[0], sp[1], sp[2]);
                     Object sDelta = vecCtor.newInstance(sd[0], sd[1], sd[2]);
                     Object cPos   = vecCtor.newInstance(cp[0], cp[1], cp[2]);
                     Object cDelta = vecCtor.newInstance(cd[0], cd[1], cd[2]);
                     Object source = pmrCtor.newInstance(sPos, sDelta, syr, sxr);
                     Object change = pmrCtor.newInstance(cPos, cDelta, cyr, cxr);

                     Object out = calcAbs.invoke(null, source, change, relSet);
                     Object oPos   = mPos.invoke(out);
                     Object oDelta = mDelta.invoke(out);
                     float oyr = (Float) mYRot.invoke(out);
                     float oxr = (Float) mXRot.invoke(out);

                     StringBuilder sb = new StringBuilder("ABS\t");
                     sb.append(m).append('\t');
                     sb.append(d((Double) fx.get(sPos))).append('\t').append(d((Double) fy.get(sPos))).append('\t').append(d((Double) fz.get(sPos))).append('\t');
                     sb.append(d((Double) fx.get(sDelta))).append('\t').append(d((Double) fy.get(sDelta))).append('\t').append(d((Double) fz.get(sDelta))).append('\t');
                     sb.append(f(syr)).append('\t').append(f(sxr)).append('\t');
                     sb.append(d((Double) fx.get(cPos))).append('\t').append(d((Double) fy.get(cPos))).append('\t').append(d((Double) fz.get(cPos))).append('\t');
                     sb.append(d((Double) fx.get(cDelta))).append('\t').append(d((Double) fy.get(cDelta))).append('\t').append(d((Double) fz.get(cDelta))).append('\t');
                     sb.append(f(cyr)).append('\t').append(f(cxr)).append('\t');
                     // outputs
                     sb.append(d((Double) fx.get(oPos))).append('\t').append(d((Double) fy.get(oPos))).append('\t').append(d((Double) fz.get(oPos))).append('\t');
                     sb.append(d((Double) fx.get(oDelta))).append('\t').append(d((Double) fy.get(oDelta))).append('\t').append(d((Double) fz.get(oDelta))).append('\t');
                     sb.append(f(oyr)).append('\t').append(f(oxr));
                     O.println(sb.toString());
                  }
               }
            }
         }
      }

      // WROT battery: position/delta untouched, rotation replaced.
      float[] wYRot = { 0.0F, 90.0F, -45.0F, 270.0F, 12.5F };
      float[] wXRot = { 0.0F, 45.0F, -90.0F, 30.0F };
      for (double[] sp : sourcePos) {
         for (double[] sd : sourceDelta) {
            for (float yr0 : new float[]{ 0.0F, 100.0F }) {
               for (float xr0 : new float[]{ 0.0F, -20.0F }) {
                  for (float nyr : wYRot) {
                     for (float nxr : wXRot) {
                        Object sPos   = vecCtor.newInstance(sp[0], sp[1], sp[2]);
                        Object sDelta = vecCtor.newInstance(sd[0], sd[1], sd[2]);
                        Object pmr    = pmrCtor.newInstance(sPos, sDelta, yr0, xr0);
                        Object out    = withRot.invoke(pmr, nyr, nxr);
                        Object oPos   = mPos.invoke(out);
                        Object oDelta = mDelta.invoke(out);
                        float oyr = (Float) mYRot.invoke(out);
                        float oxr = (Float) mXRot.invoke(out);
                        StringBuilder sb = new StringBuilder("WROT\t");
                        sb.append(d((Double) fx.get(sPos))).append('\t').append(d((Double) fy.get(sPos))).append('\t').append(d((Double) fz.get(sPos))).append('\t');
                        sb.append(d((Double) fx.get(sDelta))).append('\t').append(d((Double) fy.get(sDelta))).append('\t').append(d((Double) fz.get(sDelta))).append('\t');
                        sb.append(f(yr0)).append('\t').append(f(xr0)).append('\t').append(f(nyr)).append('\t').append(f(nxr)).append('\t');
                        sb.append(d((Double) fx.get(oPos))).append('\t').append(d((Double) fy.get(oPos))).append('\t').append(d((Double) fz.get(oPos))).append('\t');
                        sb.append(d((Double) fx.get(oDelta))).append('\t').append(d((Double) fy.get(oDelta))).append('\t').append(d((Double) fz.get(oDelta))).append('\t');
                        sb.append(f(oyr)).append('\t').append(f(oxr));
                        O.println(sb.toString());
                     }
                  }
               }
            }
         }
      }
   }
}
