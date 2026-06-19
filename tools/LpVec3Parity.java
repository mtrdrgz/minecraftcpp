// Ground truth for net.minecraft.network.LpVec3 — "low-precision Vec3" network
// packing (entity velocity / movement deltas). Calls the REAL LpVec3.write/read
// via Netty ByteBuf and emits:
//
//   WRITE <ix> <iy> <iz>(double bits) <hex>(encoded bytes)
//   READ  <hex>(input bytes) <ox> <oy> <oz>(decoded double bits)
//
// Doubles are emitted as %016x of Double.doubleToRawLongBits. The C++
// lp_vec3_parity must reproduce every encoded byte and every decoded bit.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.LpVec3;
import net.minecraft.world.phys.Vec3;

public class LpVec3Parity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ── WRITE cases: a battery of finite, physical Vec3 inputs ───────────
        // Covers: exact zero, sub-ABS_MIN (single 0 byte), small scale (no
        // continuation), large scale (partial -> continuation VarInt), the
        // clamp boundary (ABS_MAX), mixed signs, asymmetric magnitudes, and
        // realistic entity velocities.
        double[][] wcases = {
            {0.0, 0.0, 0.0},
            {1e-9, 0.0, 0.0},                 // below ABS_MIN -> single byte
            {3.0519e-5, 0.0, 0.0},            // just below ABS_MIN (3.0519440e-5)
            {3.052e-5, 0.0, 0.0},             // just above ABS_MIN
            {0.0001, -0.0002, 0.00005},
            {0.1, 0.2, -0.3},
            {0.5, 0.5, 0.5},
            {0.9999, -0.9999, 0.5},
            {1.0, 0.0, 0.0},
            {1.0, 1.0, 1.0},
            {-1.0, -1.0, -1.0},
            {1.5, -2.25, 3.75},
            {2.9, -3.1, 0.0},                 // scale 4 -> partial/continuation
            {3.0, 3.0, 3.0},                  // scale 3 -> fits in 2 bits
            {4.0, 4.0, 4.0},                  // scale 4 -> continuation
            {7.5, -7.5, 7.5},
            {10.0, 20.0, -30.0},
            {100.0, -50.0, 25.0},
            {1000.0, 1000.0, 1000.0},
            {65536.0, -65536.0, 32768.0},
            {1000000.0, -2000000.0, 500000.0},
            {1.7179869183E10, 0.0, 0.0},      // ABS_MAX exactly
            {2.0E10, -2.0E10, 1.0E10},        // beyond ABS_MAX -> clamped
            {1.7179869183E10, -1.7179869183E10, 1.7179869183E10},
            // realistic per-tick entity velocities (blocks/tick)
            {0.0784, -0.0784, 0.0},
            {0.42, 0.0, 0.0},                 // jump-ish vertical
            {0.21585, -0.0784, -0.21585},
            {-0.05, 0.3, 0.12},
            {0.003, -0.003, 0.003},
            {0.123456789, -0.987654321, 0.55555},
        };
        for (double[] c : wcases) {
            Vec3 v = new Vec3(c[0], c[1], c[2]);
            ByteBuf b = Unpooled.buffer();
            LpVec3.write(b, v);
            StringBuilder hex = new StringBuilder();
            while (b.isReadable()) hex.append(String.format("%02x", b.readByte() & 0xFF));
            O.print("WRITE\t");
            O.print(hex16(c[0]) + "\t");
            O.print(hex16(c[1]) + "\t");
            O.print(hex16(c[2]) + "\t");
            O.println(hex);
        }

        // ── READ cases: drive decode from raw input bytes ────────────────────
        // (1) round-trip every WRITE case (re-encode, then decode).
        for (double[] c : wcases) {
            Vec3 v = new Vec3(c[0], c[1], c[2]);
            ByteBuf enc = Unpooled.buffer();
            LpVec3.write(enc, v);
            byte[] bytes = new byte[enc.readableBytes()];
            enc.getBytes(enc.readerIndex(), bytes);
            emitRead(bytes);
        }
        // (2) a few hand-built buffers exercising the continuation path and
        //     boundary masks directly (bytes that encode big scales).
        // Build them by writing extreme-but-finite vectors so the bytes are valid.
        double[][] extra = {
            {5.0E9, -5.0E9, 5.0E9},
            {1.234E8, -5.678E7, 9.0E6},
            {16384.0, 16384.0, 16384.0},
            {32767.0, -32767.0, 32767.0},
        };
        for (double[] c : extra) {
            ByteBuf enc = Unpooled.buffer();
            LpVec3.write(enc, new Vec3(c[0], c[1], c[2]));
            byte[] bytes = new byte[enc.readableBytes()];
            enc.getBytes(enc.readerIndex(), bytes);
            emitRead(bytes);
        }
        // (3) the trivial single-zero-byte buffer.
        emitRead(new byte[] { 0 });
    }

    static void emitRead(byte[] bytes) {
        ByteBuf in = Unpooled.wrappedBuffer(bytes);
        Vec3 out = LpVec3.read(in);
        StringBuilder hex = new StringBuilder();
        for (byte x : bytes) hex.append(String.format("%02x", x & 0xFF));
        O.print("READ\t");
        O.print(hex + "\t");
        O.print(hex16(out.x) + "\t");
        O.print(hex16(out.y) + "\t");
        O.println(hex16(out.z));
    }

    static String hex16(double d) {
        return String.format("%016x", Double.doubleToRawLongBits(d));
    }
}
