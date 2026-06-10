import net.minecraft.core.UUIDUtil;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.UUID;

// Ground-truth dumper for net.minecraft.core.UUIDUtil (MC 26.1.2).
// Calls the REAL net.minecraft methods (private leastMostToIntArray via reflection)
// and emits tab-separated rows consumed by UuidUtilParityTest.cpp.
//
// All longs/ints emitted in DECIMAL (signed, Java semantics). String inputs to
// createOfflinePlayerUUID are emitted as LOWERCASE HEX of their raw UTF-8 bytes so
// the value survives the ASCII-encoded TSV byte-for-byte (non-ASCII names included).
//
// TAGS:
//   FROMARR  <i0> <i1> <i2> <i3> <msb> <lsb>
//        UUIDUtil.uuidFromIntArray({i0,i1,i2,i3}) -> (msb,lsb)
//   TOARR    <msb> <lsb> <i0> <i1> <i2> <i3>
//        UUIDUtil.uuidToIntArray(new UUID(msb,lsb)) -> {i0..i3}
//   LMTOARR  <msb> <lsb> <i0> <i1> <i2> <i3>
//        UUIDUtil.leastMostToIntArray(msb,lsb) -> {i0..i3}  (private, reflection)
//   OFFLINE  <nameHexUtf8> <msb> <lsb>
//        UUIDUtil.createOfflinePlayerUUID(name) -> (msb,lsb)
public class UuidUtilParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (byte x : b) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
    }

    public static void main(String[] args) throws Exception {
        // UUIDUtil's ported methods are pure; bootstrap defensively in case
        // classloading pulls in registry-touching static initializers.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — the ported methods do not require bootstrap
        }

        Method leastMost = UUIDUtil.class.getDeclaredMethod(
                "leastMostToIntArray", long.class, long.class);
        leastMost.setAccessible(true);

        // Finite/physical int-array inputs: a spread of signed 32-bit values
        // exercising sign-bit, low-word vs high-word, and arbitrary mixes.
        int[] vals = {
            0, 1, -1, 2, -2, 7, -7, 255, 256, -256,
            65535, 65536, 32767, -32768, 0x12345678, 0x7fffffff,
            0x80000000, 0x0badf00d, -559038737, 1431655765, -1431655766,
            0x00ff00ff, 0xff00ff00, 1000000007, -1000000007,
            123456789, -123456789, 2002, -2002
        };

        // FROMARR + (round-trip) TOARR/LMTOARR over a battery of 4-int combos.
        // Use a sweep that pairs each value with rotations to cover all slots.
        for (int s = 0; s < vals.length; s++) {
            int i0 = vals[s];
            int i1 = vals[(s + 7) % vals.length];
            int i2 = vals[(s + 13) % vals.length];
            int i3 = vals[(s + 19) % vals.length];

            UUID u = UUIDUtil.uuidFromIntArray(new int[]{i0, i1, i2, i3});
            long msb = u.getMostSignificantBits();
            long lsb = u.getLeastSignificantBits();
            O.println("FROMARR\t" + i0 + "\t" + i1 + "\t" + i2 + "\t" + i3
                    + "\t" + msb + "\t" + lsb);

            int[] back = UUIDUtil.uuidToIntArray(u);
            O.println("TOARR\t" + msb + "\t" + lsb + "\t"
                    + back[0] + "\t" + back[1] + "\t" + back[2] + "\t" + back[3]);

            int[] lm = (int[]) leastMost.invoke(null, msb, lsb);
            O.println("LMTOARR\t" + msb + "\t" + lsb + "\t"
                    + lm[0] + "\t" + lm[1] + "\t" + lm[2] + "\t" + lm[3]);
        }

        // Direct long inputs for leastMostToIntArray / toIntArray edge longs.
        long[] longs = {
            0L, 1L, -1L, Long.MIN_VALUE, Long.MAX_VALUE,
            0x0123456789abcdefL, 0xfedcba9876543210L, -0x0123456789abcdefL,
            0x00000000ffffffffL, 0xffffffff00000000L, 4294967296L, -4294967296L,
            0x7fffffff00000001L, 0x80000000ffffffffL
        };
        for (long m : longs) {
            for (long l : longs) {
                int[] lm = (int[]) leastMost.invoke(null, m, l);
                O.println("LMTOARR\t" + m + "\t" + l + "\t"
                        + lm[0] + "\t" + lm[1] + "\t" + lm[2] + "\t" + lm[3]);
                int[] ta = UUIDUtil.uuidToIntArray(new UUID(m, l));
                O.println("TOARR\t" + m + "\t" + l + "\t"
                        + ta[0] + "\t" + ta[1] + "\t" + ta[2] + "\t" + ta[3]);
            }
        }

        // createOfflinePlayerUUID: real offline-player MD5/name UUIDs.
        // Battery of physical player names (ASCII + UTF-8 multibyte + empty +
        // length boundaries crossing MD5's 64-byte block).
        String[] names = {
            "",
            "Notch",
            "jeb_",
            "Dinnerbone",
            "Steve",
            "Alex",
            "Player1",
            "a",
            "A",
            "0",
            "_",
            "test",
            "TEST",
            "The_Quick_Brown_Fox",
            "x123456789012345",       // 14 -> total 28 with prefix
            "0123456789012345678901234567890123456789012345678901234", // 55 -> 69 with prefix (>64)
            "01234567890123456789012345678901234567890123456789",      // 50 -> 64-ish boundary
            "MaximumLength16c",       // 16
            "café",                   // UTF-8 multibyte (é = 0xc3 0xa9)
            "naïve_steve",            // ï = 0xc3 0xaf
            "日本語プレイヤー",         // CJK multibyte
            "emoji_😀_face", // surrogate pair -> 4-byte UTF-8
            "Ünïcödë"
        };
        for (String n : names) {
            UUID u = UUIDUtil.createOfflinePlayerUUID(n);
            byte[] raw = n.getBytes(StandardCharsets.UTF_8);
            O.println("OFFLINE\t" + hex(raw) + "\t"
                    + u.getMostSignificantBits() + "\t" + u.getLeastSignificantBits());
        }
    }
}
