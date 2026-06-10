// Ground-truth generator for net.minecraft.world.entity.InterpolationHandler (26.1.2).
//
// The class is coupled to a live Entity + Level (it calls entity.position(),
// entity.level().noCollision(...), entity.setPos/setRot, etc.), so a full instance
// of InterpolationHandler cannot be exercised here without standing up a server
// world. Instead this tool exercises the PORTABLE, decoupled surface against the
// REAL net.minecraft helpers:
//
//   1. The REAL private inner class InterpolationHandler$InterpolationData and its
//      mutators decrease() / addDelta(Vec3) / addRotation(float,float), driven via
//      reflection (the ctor + methods are private/package — setAccessible).
//   2. The per-tick replay arithmetic of interpolate() (:77-107), reproduced
//      VERBATIM here but calling the REAL Mth.lerp(double,double,double),
//      Mth.rotLerp(double,double,double) and REAL Vec3.subtract/add — i.e. the exact
//      helpers the real method calls. The (float) narrowing matches Java's cast.
//
// Rows (tab-separated; doubles=%016x raw bits, floats=%08x raw bits, ints decimal):
//
//   DATA   <steps0>(int) <pos0 x y z>(double) <yRot0 xRot0>(float)
//          <delta x y z>(double) <addYRot addXRot>(float)
//          -> <stepsAfter>(int) <pos x y z>(double) <yRot xRot>(float)
//          == new InterpolationData(steps0,pos0,yRot0,xRot0); addDelta(delta);
//             addRotation(addYRot,addXRot); decrease();
//
//   STEP   <steps>(int) <tgt x y z>(double) <tgtYRot tgtXRot>(float)
//          <ePos x y z>(double) <eYRot eXRot>(float)
//          <hasPrevPos>(int) <prevPos x y z>(double)
//          <hasPrevRot>(int) <prevRotX prevRotY>(float)   // Vec2(xRot,yRot)
//          <noCollision>(int)
//          -> <newPos x y z>(double) <newYRot newXRot>(float)
//             <dataPos x y z>(double) <dataYRot dataXRot>(float) <dataSteps>(int)
//             <prevTickPos x y z>(double) <prevTickRotX prevTickRotY>(float)
//          == one tick of interpolate().
//
//   REPLAY <NT>(int) <stepsInit>(int) <tgt x y z>(double) <tgtYRot tgtXRot>(float)
//          <startPos x y z>(double) <startYRot startXRot>(float)
//          then for each of NT ticks: <eMove x y z>(double) <eYaw eXRot>(float) <noColl>(int)
//          -> for each tick: <newPos x y z>(double) <newYRot newXRot>(float)
//          == NT successive interpolate() ticks where between ticks the entity is
//             nudged by eMove and re-rotated to (eYaw,eXRot) (simulating external
//             movement); each tick consults the supplied noCollision boolean.
//
// Run via mcpp/tools/run_groundtruth.ps1 -Tool InterpolationHandlerParity -Out mcpp/build/interpolation_handler.tsv

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class InterpolationHandlerParity {
   // Capture stdout at class-load so bootstrap chatter cannot pollute the TSV.
   static final java.io.PrintStream O = System.out;

   static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }
   static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

   // ── reflection handles (resolved in main) ────────────────────────────────────
   static Class<?> vec3C;
   static Constructor<?> vecCtor;
   static Method vSubtract;     // Vec3 subtract(Vec3)
   static Method vAdd;          // Vec3 add(Vec3)
   static Field fx, fy, fz;     // Vec3 public x,y,z

   static Class<?> dataC;
   static Constructor<?> dataCtor;   // (int, Vec3, float, float)
   static Method mDecrease, mAddDelta, mAddRotation;
   static Field fSteps, fPos, fYRot, fXRot;

   static Method mthLerp;       // Mth.lerp(double,double,double)
   static Method mthRotLerp;    // Mth.rotLerp(double,double,double)

   static Object vec3(double x, double y, double z) throws Exception {
      return vecCtor.newInstance(x, y, z);
   }
   static double vx(Object v) throws Exception { return (Double) fx.get(v); }
   static double vy(Object v) throws Exception { return (Double) fy.get(v); }
   static double vz(Object v) throws Exception { return (Double) fz.get(v); }

   static double lerp(double a, double p0, double p1) throws Exception {
      return (Double) mthLerp.invoke(null, a, p0, p1);
   }
   static double rotLerp(double a, double from, double to) throws Exception {
      return (Double) mthRotLerp.invoke(null, a, from, to);
   }

   // A small mutable holder mirroring InterpolationData's fields, but every mutation
   // is performed by the REAL InterpolationData object via reflection so we are
   // testing real Mojang code.
   static Object newData(int steps, Object pos, float yRot, float xRot) throws Exception {
      return dataCtor.newInstance(steps, pos, yRot, xRot);
   }

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      vec3C   = Class.forName("net.minecraft.world.phys.Vec3");
      vecCtor = vec3C.getDeclaredConstructor(double.class, double.class, double.class);
      vecCtor.setAccessible(true);
      vSubtract = vec3C.getMethod("subtract", vec3C);
      vAdd      = vec3C.getMethod("add", vec3C);
      fx = vec3C.getField("x"); fy = vec3C.getField("y"); fz = vec3C.getField("z");

      dataC = Class.forName("net.minecraft.world.entity.InterpolationHandler$InterpolationData");
      dataCtor = dataC.getDeclaredConstructor(int.class, vec3C, float.class, float.class);
      dataCtor.setAccessible(true);
      mDecrease    = dataC.getDeclaredMethod("decrease");                       mDecrease.setAccessible(true);
      mAddDelta    = dataC.getDeclaredMethod("addDelta", vec3C);                mAddDelta.setAccessible(true);
      mAddRotation = dataC.getDeclaredMethod("addRotation", float.class, float.class); mAddRotation.setAccessible(true);
      fSteps = dataC.getDeclaredField("steps"); fSteps.setAccessible(true);
      fPos   = dataC.getDeclaredField("position"); fPos.setAccessible(true);
      fYRot  = dataC.getDeclaredField("yRot"); fYRot.setAccessible(true);
      fXRot  = dataC.getDeclaredField("xRot"); fXRot.setAccessible(true);

      Class<?> mthC = Class.forName("net.minecraft.util.Mth");
      mthLerp    = mthC.getMethod("lerp", double.class, double.class, double.class);
      mthRotLerp = mthC.getMethod("rotLerp", double.class, double.class, double.class);

      emitData();
      emitStep();
      emitReplay();
   }

   // ── DATA: real InterpolationData mutators ────────────────────────────────────
   static void emitData() throws Exception {
      double[][] poss = {
         {0, 0, 0}, {1.5, 64.0, -3.25}, {-128.0, 70.5, 256.0}, {0.1, -0.1, 0.0}
      };
      float[] yrots = { 0.0F, 45.0F, 179.0F, -135.0F, 359.0F };
      float[] xrots = { 0.0F, 30.0F, -45.0F, 89.0F, -89.0F };
      double[][] deltas = {
         {0, 0, 0}, {0.2, -0.5, 0.1}, {-1.0, 0.3, 2.0}, {12345.0, -6789.0, 0.5}
      };
      float[] addY = { 0.0F, 5.0F, -10.0F, 200.0F, -370.0F };
      float[] addX = { 0.0F, 3.0F, -7.0F, 95.0F };
      int[] steps0 = { 1, 2, 3, 10 };

      for (int si = 0; si < steps0.length; si++) {
         for (double[] p : poss) {
            for (float yr : yrots) {
               for (float xr : xrots) {
                  int di = (Math.abs((int) yr) + Math.abs((int) xr)) % deltas.length;
                  int ayi = (Math.abs((int) yr)) % addY.length;
                  int axi = (Math.abs((int) xr) + 1) % addX.length;
                  double[] dl = deltas[di];
                  float ay = addY[ayi];
                  float ax = addX[axi];

                  Object pos0 = vec3(p[0], p[1], p[2]);
                  Object data = newData(steps0[si], pos0, yr, xr);
                  Object delta = vec3(dl[0], dl[1], dl[2]);
                  mAddDelta.invoke(data, delta);
                  mAddRotation.invoke(data, ay, ax);
                  mDecrease.invoke(data);

                  int sAfter = (Integer) fSteps.get(data);
                  Object oPos = fPos.get(data);
                  float oYR = (Float) fYRot.get(data);
                  float oXR = (Float) fXRot.get(data);

                  StringBuilder sb = new StringBuilder("DATA\t");
                  sb.append(steps0[si]).append('\t');
                  sb.append(d(p[0])).append('\t').append(d(p[1])).append('\t').append(d(p[2])).append('\t');
                  sb.append(f(yr)).append('\t').append(f(xr)).append('\t');
                  sb.append(d(dl[0])).append('\t').append(d(dl[1])).append('\t').append(d(dl[2])).append('\t');
                  sb.append(f(ay)).append('\t').append(f(ax)).append('\t');
                  // outputs
                  sb.append(sAfter).append('\t');
                  sb.append(d(vx(oPos))).append('\t').append(d(vy(oPos))).append('\t').append(d(vz(oPos))).append('\t');
                  sb.append(f(oYR)).append('\t').append(f(oXR));
                  O.println(sb.toString());
               }
            }
         }
      }
   }

   // Replicates one interpolate() tick verbatim against the real InterpolationData +
   // real Mth/Vec3 helpers. Returns the freshly built output row's arrays.
   // Mutates `data` (a real InterpolationData) exactly as interpolate() would.
   //   newPose[0..2] = newPos x,y,z ; newPose as doubles
   //   newRot[0] = newYRot ; newRot[1] = newXRot  (floats)
   static double[] newPose = new double[3];
   static float[]  newRot  = new float[2];

   static void doTick(Object data,
                      double ex, double ey, double ez, float eYRot, float eXRot,
                      boolean hasPrevPos, double ppx, double ppy, double ppz,
                      boolean hasPrevRot, float prevRotX, float prevRotY,
                      boolean noCollision) throws Exception {
      int steps = (Integer) fSteps.get(data);
      double alpha = 1.0 / steps;

      if (hasPrevPos) {
         Object ePos = vec3(ex, ey, ez);
         Object prevPos = vec3(ppx, ppy, ppz);
         Object delta = vSubtract.invoke(ePos, prevPos);
         if (noCollision) {
            mAddDelta.invoke(data, delta);
         }
      }
      if (hasPrevRot) {
         float dY = eYRot - prevRotY;
         float dX = eXRot - prevRotX;
         mAddRotation.invoke(data, dY, dX);
      }

      Object tgt = fPos.get(data);
      float tgtYRot = (Float) fYRot.get(data);
      float tgtXRot = (Float) fXRot.get(data);

      double x = lerp(alpha, ex, vx(tgt));
      double y = lerp(alpha, ey, vy(tgt));
      double z = lerp(alpha, ez, vz(tgt));
      float newYRot = (float) rotLerp(alpha, (double) eYRot, (double) tgtYRot);
      float newXRot = (float) lerp(alpha, (double) eXRot, (double) tgtXRot);

      mDecrease.invoke(data);

      newPose[0] = x; newPose[1] = y; newPose[2] = z;
      newRot[0] = newYRot; newRot[1] = newXRot;
   }

   // ── STEP: one interpolate() tick ─────────────────────────────────────────────
   static void emitStep() throws Exception {
      int[] steps = { 1, 2, 3, 5 };
      double[][] tgts = { {0, 0, 0}, {4.0, 65.0, -2.0}, {-50.0, 71.0, 300.0} };
      float[] tgtY = { 0.0F, 90.0F, 178.0F, -170.0F };
      float[] tgtX = { 0.0F, 20.0F, -80.0F };
      double[][] ePoss = { {0, 0, 0}, {3.5, 64.0, -1.5}, {-48.0, 70.0, 298.0} };
      float[] eY = { 0.0F, 30.0F, -160.0F };
      float[] eX = { 0.0F, 15.0F, -70.0F };
      double[][] prevPoss = { {0, 0, 0}, {3.0, 63.5, -1.0} };
      float[][] prevRots = { {0.0F, 0.0F}, {10.0F, 25.0F} }; // (xRot, yRot) as Vec2 stores
      int[] prevPosFlags = { 0, 1 };
      int[] prevRotFlags = { 0, 1 };
      int[] noColls = { 0, 1 };

      for (int s : steps) {
         for (double[] tg : tgts) {
            for (float ty : tgtY) {
               for (float tx : tgtX) {
                  for (double[] ep : ePoss) {
                     for (float ey : eY) {
                        for (float ex : eX) {
                           // bound the cross-product: pick prev/flags deterministically
                           int pf = (Math.abs((int) ty) + Math.abs((int) tx)) % prevPosFlags.length;
                           int rf = (Math.abs((int) ey) + Math.abs((int) ex)) % prevRotFlags.length;
                           int ppi = (Math.abs((int) ty)) % prevPoss.length;
                           int pri = (Math.abs((int) ex)) % prevRots.length;
                           int nci = (Math.abs((int) tx) + Math.abs((int) ey)) % noColls.length;
                           boolean hasPrevPos = prevPosFlags[pf] == 1;
                           boolean hasPrevRot = prevRotFlags[rf] == 1;
                           double[] pp = prevPoss[ppi];
                           float[] pr = prevRots[pri]; // pr[0]=xRot, pr[1]=yRot
                           boolean noColl = noColls[nci] == 1;

                           Object pos0 = vec3(tg[0], tg[1], tg[2]);
                           Object data = newData(s, pos0, ty, tx);
                           doTick(data, ep[0], ep[1], ep[2], ey, ex,
                                  hasPrevPos, pp[0], pp[1], pp[2],
                                  hasPrevRot, pr[0], pr[1], noColl);

                           int dataSteps = (Integer) fSteps.get(data);
                           Object dPos = fPos.get(data);
                           float dYR = (Float) fYRot.get(data);
                           float dXR = (Float) fXRot.get(data);

                           // previousTickRot = Vec2(newXRot, newYRot) (x=xRot, y=yRot)
                           float prevTickRotX = newRot[1]; // newXRot
                           float prevTickRotY = newRot[0]; // newYRot

                           StringBuilder sb = new StringBuilder("STEP\t");
                           sb.append(s).append('\t');
                           sb.append(d(tg[0])).append('\t').append(d(tg[1])).append('\t').append(d(tg[2])).append('\t');
                           sb.append(f(ty)).append('\t').append(f(tx)).append('\t');
                           sb.append(d(ep[0])).append('\t').append(d(ep[1])).append('\t').append(d(ep[2])).append('\t');
                           sb.append(f(ey)).append('\t').append(f(ex)).append('\t');
                           sb.append(hasPrevPos ? 1 : 0).append('\t');
                           sb.append(d(pp[0])).append('\t').append(d(pp[1])).append('\t').append(d(pp[2])).append('\t');
                           sb.append(hasPrevRot ? 1 : 0).append('\t');
                           sb.append(f(pr[0])).append('\t').append(f(pr[1])).append('\t');
                           sb.append(noColl ? 1 : 0).append('\t');
                           // outputs
                           sb.append(d(newPose[0])).append('\t').append(d(newPose[1])).append('\t').append(d(newPose[2])).append('\t');
                           sb.append(f(newRot[0])).append('\t').append(f(newRot[1])).append('\t');
                           sb.append(d(vx(dPos))).append('\t').append(d(vy(dPos))).append('\t').append(d(vz(dPos))).append('\t');
                           sb.append(f(dYR)).append('\t').append(f(dXR)).append('\t').append(dataSteps).append('\t');
                           sb.append(d(newPose[0])).append('\t').append(d(newPose[1])).append('\t').append(d(newPose[2])).append('\t');
                           sb.append(f(prevTickRotX)).append('\t').append(f(prevTickRotY));
                           O.println(sb.toString());
                        }
                     }
                  }
               }
            }
         }
      }
   }

   // ── REPLAY: a full multi-tick interpolation sequence ─────────────────────────
   // Mirrors how interpolate() is called once per game tick. Between ticks the
   // "entity" is nudged by a per-tick movement delta and re-rotated, exactly as an
   // externally-moved entity would be; previousTickPosition/Rot are carried as the
   // real interpolate() carries them (set to the new pose at the end of each tick).
   //
   // On the FIRST tick after interpolateTo, previousTickPosition/Rot were set to the
   // entity's pre-interpolation pose (interpolateTo, :61-62). We model that: the
   // initial previousTickPosition = startPos, previousTickRot = Vec2(startXRot,startYRot).
   static void emitReplay() throws Exception {
      // Each scenario: stepsInit, target pose, start pose, and a script of NT ticks
      // (each = entity move delta + new rotation + noCollision flag).
      // entity pose at tick start = previous entity pose + move; rotation = given.
      double[][][] moveScripts = {
         { {0, 0, 0}, {0, 0, 0}, {0, 0, 0} },
         { {0.1, 0.0, 0.05}, {0.1, 0.0, 0.05}, {0.1, 0.0, 0.05} },
         { {0.0, -0.2, 0.0}, {0.3, 0.0, -0.1}, {0.0, 0.0, 0.0} },
      };
      float[][] yawScripts = {
         { 0.0F, 0.0F, 0.0F },
         { 10.0F, 20.0F, 30.0F },
         { -5.0F, -170.0F, 175.0F },
      };
      float[][] pitchScripts = {
         { 0.0F, 0.0F, 0.0F },
         { 5.0F, -10.0F, 15.0F },
         { -80.0F, 80.0F, 0.0F },
      };
      int[][] collScripts = {
         { 1, 1, 1 },
         { 1, 0, 1 },
         { 0, 0, 1 },
      };

      int[] stepsInits = { 3, 5 };
      double[][] tgts = { {10.0, 70.0, -5.0}, {-20.0, 64.0, 100.0} };
      float[] tgtYs = { 90.0F, -135.0F };
      float[] tgtXs = { 30.0F, -60.0F };
      double[][] startPoss = { {0.0, 64.0, 0.0}, {-22.0, 63.0, 98.0} };
      float[] startYs = { 0.0F, 45.0F };
      float[] startXs = { 0.0F, -20.0F };

      for (int sInit : stepsInits) {
         for (int ti = 0; ti < tgts.length; ti++) {
            for (int yi = 0; yi < tgtYs.length; yi++) {
               for (int xi = 0; xi < tgtXs.length; xi++) {
                  for (int spi = 0; spi < startPoss.length; spi++) {
                     for (int msi = 0; msi < moveScripts.length; msi++) {
                        double[] tg = tgts[ti];
                        float tgtY = tgtYs[yi];
                        float tgtX = tgtXs[xi];
                        double[] sp = startPoss[spi];
                        float startY = startYs[spi % startYs.length];
                        float startX = startXs[spi % startXs.length];
                        double[][] moves = moveScripts[msi];
                        float[] yaws = yawScripts[msi];
                        float[] pitches = pitchScripts[msi];
                        int[] colls = collScripts[msi];
                        int NT = moves.length;

                        // Build a fresh real InterpolationData (post-interpolateTo state).
                        Object pos0 = vec3(tg[0], tg[1], tg[2]);
                        Object data = newData(sInit, pos0, tgtY, tgtX);

                        // previousTickPosition / Rot start at the pre-interp pose.
                        boolean hasPrevPos = true, hasPrevRot = true;
                        double ppx = sp[0], ppy = sp[1], ppz = sp[2];
                        float prevRotX = startX, prevRotY = startY; // Vec2(xRot,yRot)

                        // entity pose tracker.
                        double curX = sp[0], curY = sp[1], curZ = sp[2];

                        StringBuilder sb = new StringBuilder("REPLAY\t");
                        sb.append(NT).append('\t').append(sInit).append('\t');
                        sb.append(d(tg[0])).append('\t').append(d(tg[1])).append('\t').append(d(tg[2])).append('\t');
                        sb.append(f(tgtY)).append('\t').append(f(tgtX)).append('\t');
                        sb.append(d(sp[0])).append('\t').append(d(sp[1])).append('\t').append(d(sp[2])).append('\t');
                        sb.append(f(startY)).append('\t').append(f(startX));

                        // append the per-tick script
                        for (int t = 0; t < NT; t++) {
                           sb.append('\t').append(d(moves[t][0])).append('\t').append(d(moves[t][1])).append('\t').append(d(moves[t][2]));
                           sb.append('\t').append(f(yaws[t])).append('\t').append(f(pitches[t]));
                           sb.append('\t').append(colls[t]);
                        }

                        // run NT ticks, appending each tick's resulting pose.
                        for (int t = 0; t < NT; t++) {
                           // entity moved this tick:
                           curX += moves[t][0]; curY += moves[t][1]; curZ += moves[t][2];
                           float eYRot = yaws[t];
                           float eXRot = pitches[t];
                           boolean noColl = colls[t] == 1;

                           // guard: if steps reached 0 the real handler would cancel()
                           // and stop; here every scenario keeps steps > 0 across NT
                           // ticks (NT <= stepsInit), so interpolate() always runs.
                           int stepsNow = (Integer) fSteps.get(data);
                           if (stepsNow <= 0) {
                              // interpolation finished — emit the entity pose unchanged.
                              sb.append('\t').append(d(curX)).append('\t').append(d(curY)).append('\t').append(d(curZ));
                              sb.append('\t').append(f(eYRot)).append('\t').append(f(eXRot));
                              continue;
                           }

                           doTick(data, curX, curY, curZ, eYRot, eXRot,
                                  hasPrevPos, ppx, ppy, ppz,
                                  hasPrevRot, prevRotX, prevRotY, noColl);

                           // entity.setPos(newPosition): the entity now sits at newPose.
                           curX = newPose[0]; curY = newPose[1]; curZ = newPose[2];
                           // previousTickPosition = newPosition
                           ppx = newPose[0]; ppy = newPose[1]; ppz = newPose[2];
                           // previousTickRot = Vec2(newXRot, newYRot) (read after setRot)
                           prevRotX = newRot[1]; prevRotY = newRot[0];

                           sb.append('\t').append(d(newPose[0])).append('\t').append(d(newPose[1])).append('\t').append(d(newPose[2]));
                           sb.append('\t').append(f(newRot[0])).append('\t').append(f(newRot[1]));
                        }
                        O.println(sb.toString());
                     }
                  }
               }
            }
         }
      }
   }
}
