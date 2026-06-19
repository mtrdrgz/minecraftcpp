import net.minecraft.client.renderer.culling.Frustum;
import net.minecraft.world.phys.AABB;
import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import java.lang.reflect.Field;

// Ground truth for net.minecraft.client.renderer.culling.Frustum (MC 26.1.2) and
// the org.joml.FrustumIntersection plane extraction / AABB test it drives.
//
// For each (projection, modelView) pair we build a REAL Frustum, then for a
// battery of AABBs emit the REAL cubeInFrustum() result + isVisible(). We also
// reflect out the COMBINED matrix (Frustum.matrix == projection.mul(modelView))
// and the 6 extracted+normalized frustum planes so the C++ port can be verified
// bit-for-bit at the plane level, not only on the integer test result.
//
// Floats -> %08x of Float.floatToRawIntBits; doubles -> %016x of
// Double.doubleToRawLongBits; ints decimal. Tab-separated.
//
// TAGS:
//   PLANES <16 combinedMatBits> <24 planeBits>
//       combinedMat = m00..m33 (column-major) of Frustum.matrix
//       planes      = (nxX,nxY,nxZ,nxW, pxX..pxW, nyX..nyW, pyX..pyW,
//                      nzX..nzW, pzX..pzW)  -- the normalized FrustumIntersection
//   TEST   <16 combinedMatBits> <camX d> <camY d> <camZ d>
//          <minX d><minY d><minZ d><maxX d><maxY d><maxZ d>
//          <cubeInFrustum int> <isVisible 0/1>
public class FrustumParity {
   static final java.io.PrintStream O = System.out;

   static Field F_MATRIX;          // Frustum.matrix : org.joml.Matrix4f
   static Field FI_INTERSECTION;   // Frustum.intersection : org.joml.FrustumIntersection
   static Field[] PLANE_FIELDS;    // 24 scalar plane fields of FrustumIntersection
   static java.lang.reflect.Method M_CUBE; // Frustum.cubeInFrustum(double x6) : int

   static final String[] PLANE_NAMES = {
      "nxX","nxY","nxZ","nxW",
      "pxX","pxY","pxZ","pxW",
      "nyX","nyY","nyZ","nyW",
      "pyX","pyY","pyZ","pyW",
      "nzX","nzY","nzZ","nzW",
      "pzX","pzY","pzZ","pzW",
   };

   static {
      try {
         F_MATRIX = Frustum.class.getDeclaredField("matrix");
         F_MATRIX.setAccessible(true);
         FI_INTERSECTION = Frustum.class.getDeclaredField("intersection");
         FI_INTERSECTION.setAccessible(true);
         Class<?> fiClass = Class.forName("org.joml.FrustumIntersection");
         PLANE_FIELDS = new Field[PLANE_NAMES.length];
         for (int i = 0; i < PLANE_NAMES.length; i++) {
            PLANE_FIELDS[i] = fiClass.getDeclaredField(PLANE_NAMES[i]);
            PLANE_FIELDS[i].setAccessible(true);
         }
         M_CUBE = Frustum.class.getDeclaredMethod(
            "cubeInFrustum", double.class, double.class, double.class,
                             double.class, double.class, double.class);
         M_CUBE.setAccessible(true);
      } catch (Exception e) { throw new RuntimeException(e); }
   }

   static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }
   static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

   // 16 column-major floats m00..m33 of a Matrix4fc as hex bits, tab-joined.
   static String matBits(Matrix4fc m) {
      float[] a = new float[16];
      m.get(a); // a[0]=m00, a[1]=m01, ... a[15]=m33 (column-major)
      StringBuilder sb = new StringBuilder();
      for (int i = 0; i < 16; i++) { if (i > 0) sb.append('\t'); sb.append(f(a[i])); }
      return sb.toString();
   }

   static String planeBits(Frustum fr) throws Exception {
      Object fi = FI_INTERSECTION.get(fr);
      StringBuilder sb = new StringBuilder();
      for (int i = 0; i < PLANE_FIELDS.length; i++) {
         if (i > 0) sb.append('\t');
         sb.append(f(PLANE_FIELDS[i].getFloat(fi)));
      }
      return sb.toString();
   }

   static int cubeInFrustum(Frustum fr, double mnx, double mny, double mnz,
                                        double mxx, double mxy, double mxz) throws Exception {
      return (Integer) M_CUBE.invoke(fr, mnx, mny, mnz, mxx, mxy, mxz);
   }

   // Emit a PLANES row (combined matrix + normalized planes) once per Frustum.
   static void emitPlanes(Frustum fr) throws Exception {
      Matrix4f mat = (Matrix4f) F_MATRIX.get(fr);
      O.println("PLANES\t" + matBits(mat) + "\t" + planeBits(fr));
   }

   // Emit a TEST row for one AABB against a Frustum prepared at (cx,cy,cz).
   static void emitTest(Frustum fr, double cx, double cy, double cz, AABB bb) throws Exception {
      Matrix4f mat = (Matrix4f) F_MATRIX.get(fr);
      int cube = cubeInFrustum(fr, bb.minX, bb.minY, bb.minZ, bb.maxX, bb.maxY, bb.maxZ);
      boolean vis = fr.isVisible(bb);
      O.println("TEST\t" + matBits(mat)
         + "\t" + d(cx) + "\t" + d(cy) + "\t" + d(cz)
         + "\t" + d(bb.minX) + "\t" + d(bb.minY) + "\t" + d(bb.minZ)
         + "\t" + d(bb.maxX) + "\t" + d(bb.maxY) + "\t" + d(bb.maxZ)
         + "\t" + cube + "\t" + (vis ? 1 : 0));
   }

   // ── matrix builders (deterministic float ops; NO live sin/cos) ──────────────
   // perspective via setPerspective would call tan(); instead we use frustum()
   // with explicit edge planes (pure float arithmetic) and ortho()/translate()/
   // scale() — none of which invoke trig.
   static Matrix4f persp(float l, float r, float b, float t, float n, float fa) {
      // org.joml.Matrix4f.frustum(left,right,bottom,top,zNear,zFar) — affine-free
      // perspective build, no trig.
      return new Matrix4f().frustum(l, r, b, t, n, fa);
   }
   static Matrix4f ortho(float l, float r, float b, float t, float n, float fa) {
      return new Matrix4f().ortho(l, r, b, t, n, fa);
   }

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      // ── projection / modelView batteries (exact-float, no live trig) ──────────
      java.util.ArrayList<Matrix4f[]> pairs = new java.util.ArrayList<>();

      // identity modelView, several projections
      Matrix4f mvId = new Matrix4f();
      pairs.add(new Matrix4f[]{ persp(-0.5f, 0.5f, -0.5f, 0.5f, 0.05f, 1024.0f), new Matrix4f(mvId) });
      pairs.add(new Matrix4f[]{ persp(-1.0f, 1.0f, -0.75f, 0.75f, 0.1f, 256.0f), new Matrix4f(mvId) });
      pairs.add(new Matrix4f[]{ ortho(-10.0f, 10.0f, -10.0f, 10.0f, -50.0f, 50.0f), new Matrix4f(mvId) });
      pairs.add(new Matrix4f[]{ ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 100.0f), new Matrix4f(mvId) });

      // modelView with a translation (camera-space offset) — affine
      Matrix4f mvT = new Matrix4f().translation(0.0f, 0.0f, -10.0f);
      pairs.add(new Matrix4f[]{ persp(-0.5f, 0.5f, -0.5f, 0.5f, 0.05f, 1024.0f), new Matrix4f(mvT) });
      pairs.add(new Matrix4f[]{ ortho(-10.0f, 10.0f, -10.0f, 10.0f, -50.0f, 50.0f), new Matrix4f(mvT) });

      // modelView with a 180° flip about Y (exact-float rotation matrix set by
      // scaling -1,1,-1 which is orthonormal and affine, no trig)
      Matrix4f mvFlip = new Matrix4f().scaling(-1.0f, 1.0f, -1.0f);
      pairs.add(new Matrix4f[]{ persp(-0.75f, 0.75f, -0.5f, 0.5f, 0.1f, 512.0f), new Matrix4f(mvFlip) });

      // modelView translate+scale (affine, no trig)
      Matrix4f mvTS = new Matrix4f().translation(3.0f, -2.0f, -20.0f).scale(1.0f, 1.0f, 1.0f);
      pairs.add(new Matrix4f[]{ persp(-0.5f, 0.5f, -0.5f, 0.5f, 0.05f, 1024.0f), new Matrix4f(mvTS) });

      // asymmetric perspective (off-center)
      pairs.add(new Matrix4f[]{ persp(-0.3f, 0.7f, -0.4f, 0.6f, 0.05f, 2048.0f), new Matrix4f(mvId) });

      // ── camera positions to prepare() with ──────────────────────────────────
      double[][] cams = {
         { 0.0, 0.0, 0.0 },
         { 8.0, 64.0, -8.0 },
         { -123.5, 70.25, 456.75 },
         { 1000000.0, -32.0, -1000000.0 },
         { 0.5, 0.5, 0.5 },
      };

      // ── AABB battery (camera-relative span: inside / outside / straddling) ────
      // After prepare(cam), the box is tested in camera-relative float space, so
      // these are authored relative to where the camera will be placed; we add the
      // camera offset per-case so the relative geometry sweeps the frustum.
      double[][] relBoxes = {
         // small unit cube straight ahead (-Z is forward for the persp frustums)
         { -0.5, -0.5, -5.0,   0.5, 0.5, -4.0 },
         // big cube enclosing origin
         { -2.0, -2.0, -2.0,   2.0, 2.0, 2.0 },
         // far ahead
         { -1.0, -1.0, -500.0, 1.0, 1.0, -499.0 },
         // behind the camera (+Z) — should be culled for the persp frustums
         { -1.0, -1.0, 5.0,    1.0, 1.0, 6.0 },
         // far to the right
         { 100.0, -1.0, -10.0, 101.0, 1.0, -9.0 },
         // far up
         { -1.0, 100.0, -10.0, 1.0, 101.0, -9.0 },
         // straddling the left clip edge
         { -8.0, -0.5, -10.0,  -2.0, 0.5, -9.0 },
         // a thin slab spanning a huge x-range (straddles many planes)
         { -1000.0, -0.5, -10.0, 1000.0, 0.5, -9.0 },
         // degenerate (zero-volume) box at a point ahead
         { 0.0, 0.0, -8.0,     0.0, 0.0, -8.0 },
         // inverted-ish (min>max) edge case to exercise corner selection
         { 1.0, 1.0, -4.0,    -1.0, -1.0, -6.0 },
         // negative-coordinate cube up-left-forward
         { -20.0, -20.0, -20.0, -18.0, -18.0, -18.0 },
         // large block ahead spanning near→far
         { -16.0, -16.0, -200.0, 16.0, 16.0, -1.0 },
      };

      for (Matrix4f[] pm : pairs) {
         Matrix4f proj = pm[0];
         Matrix4fc mv = pm[1];
         // Frustum(modelView, projection) -> calculateFrustum(modelView, projection)
         // === projection.mul(modelView, this.matrix); intersection.set(matrix)
         Frustum fr = new Frustum(mv, proj);
         emitPlanes(fr);
         for (double[] cam : cams) {
            fr.prepare(cam[0], cam[1], cam[2]);
            for (double[] rb : relBoxes) {
               AABB bb = new AABB(
                  cam[0] + rb[0], cam[1] + rb[1], cam[2] + rb[2],
                  cam[0] + rb[3], cam[1] + rb[4], cam[2] + rb[5]);
               emitTest(fr, cam[0], cam[1], cam[2], bb);
            }
         }
      }
   }
}
