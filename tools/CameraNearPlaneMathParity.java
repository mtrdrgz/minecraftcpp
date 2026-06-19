// Ground-truth generator for the pure near-plane geometry of
// net.minecraft.client.Camera (MC 26.1.2), mirrored by
// client/CameraNearPlaneMath.h.
//
// This harness drives the REAL classes end to end:
//   * net.minecraft.client.Camera#setRotation(yRot, xRot)   (sets forwards/up/left)
//   * net.minecraft.client.Camera#getNearPlane(float fov)
//   * net.minecraft.client.Camera.NearPlane#getTopLeft / getTopRight /
//       getBottomLeft / getBottomRight / getPointOnPlane
//   * net.minecraft.client.renderer.Projection#setupPerspective (for width/height/zNear)
//
// Camera's field initialisers call Minecraft.getInstance() and allocate GL
// resources, so we never run a Camera constructor. Instead we obtain a bare
// instance via sun.misc.Unsafe#allocateInstance (reached PURELY REFLECTIVELY so
// no `import sun.misc.Unsafe` and zero javac warnings), then:
//   - install a fresh Projection (its constructor + setupPerspective touch no GL),
//   - call the protected setRotation reflectively (pure: only sets the basis),
//   - call the public getNearPlane + NearPlane getters reflectively.
// No method body is reproduced Java-side — every value comes from the real class.
//
// Doubles exchanged as raw IEEE-754 bits (%016x of Double.doubleToRawLongBits);
// floats as %08x of Float.floatToRawIntBits.
//
// Row types:
//   NEARPLANE <yRotBits> <xRotBits> <fovBits> <wBits> <hBits> <zNearBits>
//             <forward.xyz> <left.xyz> <up.xyz>                        (9 doubles)
//   CORNERS   <... same key ...> <TL.xyz> <TR.xyz> <BL.xyz> <BR.xyz>  (12 doubles)
//   POINT     <... same key ...> <pxBits> <pyBits> <point.xyz>         (3 doubles)
//
//   tools/run_groundtruth.ps1 -Tool CameraNearPlaneMathParity -Out mcpp/build/camera_nearplane_math.tsv

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.client.Camera;
import net.minecraft.client.renderer.Projection;
import net.minecraft.world.phys.Vec3;

public class CameraNearPlaneMathParity {
    static final java.io.PrintStream O = System.out;

    static String bf(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }
    static String bd(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static String v3(Vec3 p) { return bd(p.x) + "\t" + bd(p.y) + "\t" + bd(p.z); }

    // Reflective sun.misc.Unsafe#allocateInstance — no compile-time dependency.
    static Object unsafe;
    static Method allocateInstance;

    static void initUnsafe() throws Exception {
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        unsafe = theUnsafe.get(null);
        allocateInstance = unsafeClass.getMethod("allocateInstance", Class.class);
    }

    static Field cameraForwards, cameraUp, cameraLeft, cameraProjection, cameraRotation;
    static Method cameraSetRotation, cameraGetNearPlane;
    static Method npTopLeft, npTopRight, npBottomLeft, npBottomRight, npPointOnPlane;

    static void initReflection() throws Exception {
        cameraForwards = Camera.class.getDeclaredField("forwards");
        cameraUp = Camera.class.getDeclaredField("up");
        cameraLeft = Camera.class.getDeclaredField("left");
        cameraProjection = Camera.class.getDeclaredField("projection");
        cameraRotation = Camera.class.getDeclaredField("rotation");
        cameraForwards.setAccessible(true);
        cameraUp.setAccessible(true);
        cameraLeft.setAccessible(true);
        cameraProjection.setAccessible(true);
        cameraRotation.setAccessible(true);

        cameraSetRotation = Camera.class.getDeclaredMethod("setRotation", float.class, float.class);
        cameraSetRotation.setAccessible(true);
        cameraGetNearPlane = Camera.class.getDeclaredMethod("getNearPlane", float.class);
        cameraGetNearPlane.setAccessible(true);

        Class<?> nearPlane = Class.forName("net.minecraft.client.Camera$NearPlane");
        npTopLeft = nearPlane.getDeclaredMethod("getTopLeft");
        npTopRight = nearPlane.getDeclaredMethod("getTopRight");
        npBottomLeft = nearPlane.getDeclaredMethod("getBottomLeft");
        npBottomRight = nearPlane.getDeclaredMethod("getBottomRight");
        npPointOnPlane = nearPlane.getDeclaredMethod("getPointOnPlane", float.class, float.class);
        npTopLeft.setAccessible(true);
        npTopRight.setAccessible(true);
        npBottomLeft.setAccessible(true);
        npBottomRight.setAccessible(true);
        npPointOnPlane.setAccessible(true);
    }

    // Build a Camera with the given rotation + projection, without running any
    // Camera constructor. The `forwards`/`up`/`left` Vector3f fields are final but
    // pre-allocated by JOML's no-arg default in the real engine; setRotation mutates
    // them in place. With allocateInstance they start null, so we must seed them
    // with fresh Vector3f objects (the same identity setRotation would mutate).
    static Camera makeCamera(float yRot, float xRot, float fov, float w, float h, float zNear)
            throws Exception {
        Camera cam = (Camera) allocateInstance.invoke(unsafe, Camera.class);

        // Seed the basis vectors + rotation quaternion with fresh instances so the
        // in-place setRotation has live targets to write into (allocateInstance left
        // every field null/default — these are the same identities the real
        // constructor's field initialisers create).
        cameraForwards.set(cam, new org.joml.Vector3f());
        cameraUp.set(cam, new org.joml.Vector3f());
        cameraLeft.set(cam, new org.joml.Vector3f());
        cameraRotation.set(cam, new org.joml.Quaternionf());

        // Install a Projection with the requested perspective dims (no GL touched).
        Projection projection = new Projection();
        projection.setupPerspective(zNear, 1000.0F, fov, w, h);
        cameraProjection.set(cam, projection);

        // Drive the REAL setRotation to populate forwards/up/left.
        cameraSetRotation.invoke(cam, yRot, xRot);
        return cam;
    }

    // (yRotDeg, xRotDeg, fovDeg, width, height, zNear)
    static final float[][] CASES = {
        {0f, 0f, 70f, 1920f, 1080f, 0.05f},
        {90f, 0f, 70f, 1920f, 1080f, 0.05f},
        {-90f, 0f, 70f, 1920f, 1080f, 0.05f},
        {180f, 0f, 70f, 1280f, 720f, 0.05f},
        {-180f, 0f, 90f, 1280f, 720f, 0.05f},
        {0f, 90f, 70f, 1600f, 900f, 0.05f},
        {0f, -90f, 70f, 1600f, 900f, 0.05f},
        {45f, 30f, 70f, 1024f, 768f, 0.05f},
        {-135f, -45f, 110f, 800f, 600f, 0.05f},
        {135.5f, 12.5f, 53.130102f, 2560f, 1440f, 0.05f},
        {12.34f, -56.78f, 70f, 640f, 480f, 0.05f},
        {0f, 0f, 30f, 1920f, 1200f, 0.05f},
        {0f, 0f, 70f, 1f, 1f, 0.05f},        // square aspect
        {179.0f, 89.0f, 70f, 3840f, 2160f, 0.05f},
        {-30.5f, 15.25f, 100f, 1366f, 768f, 0.05f},
    };

    // (x, y) sample points for getPointOnPlane (normalised plane coords).
    static final float[][] POINTS = {
        {0f, 0f},
        {1f, 0f},
        {0f, 1f},
        {1f, 1f},
        {-1f, -1f},
        {0.5f, -0.25f},
        {-0.75f, 0.33333334f},
    };

    public static void main(String[] args) throws Exception {
        initUnsafe();
        initReflection();

        for (float[] c : CASES) {
            float yRot = c[0], xRot = c[1], fov = c[2], w = c[3], h = c[4], zNear = c[5];
            String key = bf(yRot) + "\t" + bf(xRot) + "\t" + bf(fov) + "\t"
                       + bf(w) + "\t" + bf(h) + "\t" + bf(zNear);

            Camera cam = makeCamera(yRot, xRot, fov, w, h, zNear);
            Object plane = cameraGetNearPlane.invoke(cam, fov);

            // Read back the NearPlane's private forward/left/up (== the scaled basis).
            Field fForward = plane.getClass().getDeclaredField("forward");
            Field fLeft = plane.getClass().getDeclaredField("left");
            Field fUp = plane.getClass().getDeclaredField("up");
            fForward.setAccessible(true);
            fLeft.setAccessible(true);
            fUp.setAccessible(true);
            Vec3 forward = (Vec3) fForward.get(plane);
            Vec3 left = (Vec3) fLeft.get(plane);
            Vec3 up = (Vec3) fUp.get(plane);
            O.println("NEARPLANE\t" + key + "\t" + v3(forward) + "\t" + v3(left) + "\t" + v3(up));

            Vec3 tl = (Vec3) npTopLeft.invoke(plane);
            Vec3 tr = (Vec3) npTopRight.invoke(plane);
            Vec3 bl = (Vec3) npBottomLeft.invoke(plane);
            Vec3 br = (Vec3) npBottomRight.invoke(plane);
            O.println("CORNERS\t" + key + "\t" + v3(tl) + "\t" + v3(tr) + "\t" + v3(bl) + "\t" + v3(br));

            for (float[] p : POINTS) {
                Vec3 pt = (Vec3) npPointOnPlane.invoke(plane, p[0], p[1]);
                O.println("POINT\t" + key + "\t" + bf(p[0]) + "\t" + bf(p[1]) + "\t" + v3(pt));
            }
        }
    }
}
