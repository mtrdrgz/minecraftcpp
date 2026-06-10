import net.minecraft.util.Crypt;
import java.lang.reflect.Method;
import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

// Ground-truth dumper for the PURE byte helpers of net.minecraft.util.Crypt (MC 26.1.2).
//
// Calls the REAL net.minecraft.util.Crypt.digestData(byte[]...) (private varargs,
// invoked via reflection + setAccessible) and emits tab-separated rows consumed by
// CryptDigestParityTest.cpp.
//
// digestData(byte[]... inputs) = SHA-1 over input[0]++input[1]++...++input[n-1]
//   (MessageDigest.getInstance("SHA-1"), each input update()'d, then digest()).
//
// We also emit the classic "server id hash" exactly as the dedicated server computes
// it at ServerLoginPacketListenerImpl line 182:
//     new BigInteger(Crypt.digestData(...)).toString(16)
// which is BigInteger(signed, big-endian) -> base-16 string (negative -> leading '-').
//
// All byte[] are emitted as LOWERCASE HEX so they survive the TSV byte-for-byte.
//
// TAGS:
//   DIGEST   <nInputs> <hexIn0> <hexIn1> ... <hexIn{n-1}> <hexDigest20> <serverHash>
//        Crypt.digestData(in0, in1, ...) -> 20-byte SHA-1 digest (hex) and the
//        BigInteger(digest).toString(16) server-hash string.
@SuppressWarnings({"unchecked", "deprecation"})
public class CryptDigestParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (byte x : b) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
    }

    // The private varargs Crypt.digestData(byte[]...).
    static Method DIGEST;

    static byte[] digestData(byte[]... inputs) throws Exception {
        // varargs reflective invoke: pass the byte[][] as a single Object argument.
        return (byte[]) DIGEST.invoke(null, (Object) inputs);
    }

    public static void main(String[] args) throws Exception {
        // Crypt has registry-free static initializers, but bootstrap defensively in
        // case classloading pulls in anything that demands it.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — digestData does not require bootstrap
        }

        DIGEST = Crypt.class.getDeclaredMethod("digestData", byte[][].class);
        DIGEST.setAccessible(true);

        // Finite/physical byte inputs: empty, length boundaries crossing SHA-1's
        // 64-byte block (55/56/63/64/65), single bytes covering sign extremes, and
        // realistic protocol material (Latin-1 server ids, AES-128 key sizes,
        // X.509 RSA-1024 public-key-ish lengths).
        byte[] EMPTY = new byte[0];

        // Single-input digests over assorted physical buffers.
        byte[][] singles = {
            EMPTY,
            bytes(0x00),
            bytes(0xff),
            bytes(0x80),
            bytes(0x7f),
            ascii("a"),
            ascii("abc"),
            ascii("The quick brown fox jumps over the lazy dog"),
            // ISO-8859-1 server ids (one byte per char, value & 0xFF)
            latin1(""),
            latin1("Minecraft"),
            latin1("ÿþý"),      // high Latin-1 bytes 0xff 0xfe 0xfd
            repeat((byte) 0x61, 55),           // 55 bytes: just under one block
            repeat((byte) 0x61, 56),           // 56 bytes: forces a second block (len field)
            repeat((byte) 0x61, 63),
            repeat((byte) 0x61, 64),           // exactly one block
            repeat((byte) 0x61, 65),
            repeat((byte) 0x61, 119),          // 119 = 64+55
            repeat((byte) 0x61, 128),          // two full blocks
            range(0, 256),                     // bytes 0x00..0xff
            range(0, 16),                      // AES-128 key length (16)
            range(0, 162),                     // ~X.509 RSA-1024 pubkey length
        };
        for (byte[] s : singles) {
            emit(new byte[][]{ s });
        }

        // Multi-input concatenation: order matters (Crypt feeds serverId, key, key).
        // Mirror the (serverId, sharedKey, publicKey) shape from digestData(String,..).
        byte[] serverId = latin1("");                 // real server id is "" (line 182)
        byte[] serverIdB = latin1("AnotherServer");
        byte[] aesKey = range(0x10, 0x10 + 16);       // 16-byte AES-128 key bytes
        byte[] rsaPub = range(0x30, 0x30 + 162);      // pubkey-sized buffer
        byte[][][] multis = {
            { EMPTY, EMPTY },
            { serverId, aesKey, rsaPub },             // classic auth-hash shape
            { serverIdB, aesKey, rsaPub },
            { ascii("a"), ascii("b"), ascii("c") },
            { ascii("ab"), ascii("c") },              // same concat as {"a","b","c"} ? no: "abc"
            { ascii("abc"), EMPTY },                  // trailing empty -> same as "abc"
            { EMPTY, ascii("abc") },                  // leading empty -> same as "abc"
            { repeat((byte) 0x61, 32), repeat((byte) 0x62, 32) }, // 64 total split 32/32
            { bytes(0x80), bytes(0x00), bytes(0xff) },
            // Known-vector server hashes (Notchian "Notch"/"jeb_"/"simon" examples
            // documented for the protocol; here as raw Latin-1 to digest directly).
            { latin1("Notch") },
            { latin1("jeb_") },
            { latin1("simon") },
        };
        for (byte[][] m : multis) {
            emit(m);
        }
    }

    static void emit(byte[][] inputs) throws Exception {
        byte[] digest = digestData(inputs);
        String serverHash = new BigInteger(digest).toString(16);
        StringBuilder sb = new StringBuilder();
        sb.append("DIGEST\t").append(inputs.length);
        for (byte[] in : inputs) sb.append('\t').append(hex(in));
        sb.append('\t').append(hex(digest));
        sb.append('\t').append(serverHash);
        O.println(sb.toString());
    }

    // --- byte helpers -------------------------------------------------------
    static byte[] bytes(int... v) {
        byte[] b = new byte[v.length];
        for (int i = 0; i < v.length; i++) b[i] = (byte) v[i];
        return b;
    }

    static byte[] ascii(String s) {
        return s.getBytes(StandardCharsets.US_ASCII);
    }

    // ISO-8859-1 / Latin-1: matches serverId.getBytes("ISO_8859_1").
    static byte[] latin1(String s) {
        return s.getBytes(StandardCharsets.ISO_8859_1);
    }

    static byte[] repeat(byte v, int n) {
        byte[] b = new byte[n];
        for (int i = 0; i < n; i++) b[i] = v;
        return b;
    }

    // bytes [lo, hi) modulo 256.
    static byte[] range(int lo, int hi) {
        byte[] b = new byte[hi - lo];
        for (int i = 0; i < b.length; i++) b[i] = (byte) ((lo + i) & 0xff);
        return b;
    }

    // (kept for symmetry; not currently used — direct MessageDigest cross-check)
    @SuppressWarnings("unused")
    static byte[] sha1Direct(byte[] data) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-1");
        md.update(data);
        return md.digest();
    }
}
