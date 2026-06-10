import com.mojang.math.Transformation;
import org.joml.Matrix4f;
import org.joml.Matrix4fc;
import org.joml.Quaternionf;
import org.joml.Vector3f;
import java.lang.reflect.Field;

// Ground truth for com.mojang.math.Transformation matrix-level methods.
//
// Emits tab-separated rows. Floats are encoded as %08x of Float.floatToRawIntBits.
// A Matrix4f is serialized column-major as its 16 elements m00..m33 (the order
// JOML's get(float[]) / our struct uses), followed by its `properties` int so the
// C++ side can reconstruct the EXACT same matrix (properties drive mul-dispatch).
//
// TAGS:
//   GETMATRIX  <16 srcBits> <srcProps>                          -> <16 outBits> <outProps>
//   COMPOSE    <16 aBits> <aProps> <16 bBits> <bProps>          -> <16 outBits> <outProps>
//   INVERSE    <16 srcBits> <srcProps>                          -> <finite> <16 outBits> <outProps>
//   IDENTITY   (no input)                                       -> <16 outBits> <outProps>
//
// We call the REAL Transformation/Matrix4f methods. `compose(Transformation)`,
// the (Matrix4fc) constructor, getMatrix(), getMatrixCopy(), and Matrix4f.invertAffine()
// are all exercised directly. invertAffine() + isFinite() reproduce inverse()'s body
// (the IDENTITY object-identity short-circuit only affects the decomposed cache, not
// the returned matrix value, so we do not need a live IDENTITY here).
public class TransformationParity {
   static final java.io.PrintStream O = System.out;

   static Field PROPS;
   static {
      try {
         PROPS = Matrix4f.class.getDeclaredField("properties");
         PROPS.setAccessible(true);
      } catch (Exception e) { throw new RuntimeException(e); }
   }

   static int props(Matrix4fc m) {
      try { return PROPS.getInt(m); } catch (Exception e) { throw new RuntimeException(e); }
   }

   // append the 16 elements (m00..m33, column-major) as hex-float bits
   static String matBits(Matrix4fc m) {
      float[] a = new float[16];
      m.get(a); // column-major: a[0]=m00, a[1]=m01, ... a[15]=m33
      StringBuilder sb = new StringBuilder();
      for (int i = 0; i < 16; i++) {
         if (i > 0) sb.append('\t');
         sb.append(String.format("%08x", Float.floatToRawIntBits(a[i])));
      }
      return sb.toString();
   }

   static String matLine(Matrix4fc m) { return matBits(m) + "\t" + props(m); }

   static void emitGetMatrix(Matrix4f src) {
      Transformation t = new Transformation(src);
      // getMatrix returns internal ref; getMatrixCopy returns a copy. Both equal in value.
      Matrix4fc got = t.getMatrix();
      Matrix4f copy = t.getMatrixCopy();
      // sanity: emit src in, copy out (the copy path is the one used by compose/inverse)
      O.println("GETMATRIX\t" + matLine(src) + "\t" + matLine(copy));
      // also verify getMatrix() ref carries identical value+props
      O.println("GETMATRIX\t" + matLine(src) + "\t" + matLine(new Matrix4f(got)));
   }

   static void emitCompose(Matrix4f a, Matrix4f b) {
      Transformation ta = new Transformation(a);
      Transformation tb = new Transformation(b);
      Transformation r = ta.compose(tb);
      O.println("COMPOSE\t" + matLine(a) + "\t" + matLine(b) + "\t" + matLine(r.getMatrix()));
   }

   static void emitInverse(Matrix4f a) {
      Transformation t = new Transformation(a);
      // reproduce inverse() body: getMatrixCopy().invertAffine(); finite ? new T : null
      Matrix4f inv = t.getMatrixCopy().invertAffine();
      int finite = inv.isFinite() ? 1 : 0;
      O.println("INVERSE\t" + matLine(a) + "\t" + finite + "\t" + matLine(inv));
   }

   // ── matrix builders (deterministic float ops; no live sin/cos) ─────────────
   // Quaternion from EXACT float components (already-normalized literals so the
   // ground truth never calls sin/cos at build time).
   static Matrix4f rot(float qx, float qy, float qz, float qw) {
      return new Matrix4f().rotation(new Quaternionf(qx, qy, qz, qw));
   }
   static Matrix4f trans(float x, float y, float z) {
      return new Matrix4f().translation(x, y, z);
   }
   static Matrix4f scl(float x, float y, float z) {
      return new Matrix4f().scale(x, y, z);
   }

   public static void main(String[] args) throws Exception {
      // A battery of matrices spanning the property/dispatch space.
      // Quaternion literals: exact-float, pre-normalized so no sin/cos is invoked.
      java.util.ArrayList<Matrix4f> mats = new java.util.ArrayList<>();

      mats.add(new Matrix4f());                                  // identity
      mats.add(trans(2.5f, -3.0f, 7.25f));                       // pure translation
      mats.add(trans(0.0f, 0.0f, 0.0f));                         // translation==identity values
      mats.add(scl(2.0f, 0.5f, -1.0f));                          // scale (mixed signs)
      mats.add(scl(1.0f, 1.0f, 1.0f));                           // unit scale (orthonormal)
      mats.add(scl(-1.0f, -1.0f, -1.0f));                        // |scale|==1 each (orthonormal)
      mats.add(scl(3.0f, 3.0f, 3.0f));                           // uniform scale 3

      // rotations from exact unit quaternions:
      mats.add(rot(0.0f, 0.0f, 0.0f, 1.0f));                     // identity rotation
      mats.add(rot(1.0f, 0.0f, 0.0f, 0.0f));                     // 180° about X
      mats.add(rot(0.0f, 1.0f, 0.0f, 0.0f));                     // 180° about Y
      mats.add(rot(0.0f, 0.0f, 1.0f, 0.0f));                     // 180° about Z
      mats.add(rot(0.70710677f, 0.0f, 0.0f, 0.70710677f));      // 90° about X (sqrt2/2)
      mats.add(rot(0.0f, 0.70710677f, 0.0f, 0.70710677f));      // 90° about Y
      mats.add(rot(0.0f, 0.0f, 0.70710677f, 0.70710677f));      // 90° about Z
      mats.add(rot(0.5f, 0.5f, 0.5f, 0.5f));                     // 120° about (1,1,1)
      mats.add(rot(0.5f, -0.5f, 0.5f, -0.5f));                   // mixed-sign unit quat
      mats.add(rot(0.18257418f, 0.36514837f, 0.5477226f, 0.73029673f)); // normalized (1,2,3,4)/sqrt30

      // composite affine (rotation then scale then translate) built via JOML:
      Matrix4f comp = new Matrix4f();
      comp.translation(1.0f, 2.0f, 3.0f);
      comp.rotate(new Quaternionf(0.70710677f, 0.0f, 0.0f, 0.70710677f));
      comp.scale(2.0f, 3.0f, 4.0f);
      mats.add(comp);

      Matrix4f comp2 = new Matrix4f();
      comp2.rotation(new Quaternionf(0.5f, 0.5f, 0.5f, 0.5f));
      comp2.scale(0.5f, 0.25f, 2.0f);
      mats.add(comp2);

      // a degenerate (singular) matrix to drive inverse()'s non-finite path:
      Matrix4f singular = new Matrix4f().scale(0.0f, 1.0f, 1.0f);
      mats.add(singular);

      // GETMATRIX + INVERSE over every matrix
      for (Matrix4f m : mats) {
         emitGetMatrix(new Matrix4f(m));
         emitInverse(new Matrix4f(m));
      }

      // COMPOSE over a representative cross-product (every A with several Bs)
      Matrix4f[] bs = new Matrix4f[] {
         new Matrix4f(),
         trans(1.0f, -2.0f, 0.5f),
         scl(2.0f, 2.0f, 2.0f),
         scl(-1.0f, 1.0f, -1.0f),
         rot(0.70710677f, 0.0f, 0.0f, 0.70710677f),
         rot(0.5f, 0.5f, 0.5f, 0.5f),
         comp,
         comp2
      };
      for (Matrix4f a : mats) {
         for (Matrix4f b : bs) {
            emitCompose(new Matrix4f(a), new Matrix4f(b));
         }
      }

      // IDENTITY
      O.println("IDENTITY\t" + matLine(Transformation.IDENTITY.getMatrix()));
   }
}
