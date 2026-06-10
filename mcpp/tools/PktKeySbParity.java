// Ground truth for net.minecraft.network.protocol.login.ServerboundKeyPacket.
//
// The packet carries two raw byte[] fields. Its STREAM_CODEC (built via
// Packet.codec(write, new)) writes ONLY:
//     output.writeByteArray(this.keybytes);            // VarInt(len) + raw bytes
//     output.writeByteArray(this.encryptedChallenge);  // VarInt(len) + raw bytes
// and reads back input.readByteArray() then input.readByteArray(). No packet-type
// id is part of the codec bytes (that framing lives outside the StreamCodec).
//
// Source (26.1.2/src/net/minecraft/network/protocol/login/ServerboundKeyPacket.java):
//     private void write(final FriendlyByteBuf output) {
//        output.writeByteArray(this.keybytes);
//        output.writeByteArray(this.encryptedChallenge);
//     }
//     private ServerboundKeyPacket(final FriendlyByteBuf input) {
//        this.keybytes = input.readByteArray();
//        this.encryptedChallenge = input.readByteArray();
//     }
//
// writeByteArray (FriendlyByteBuf.java) = VarInt.write(bytes.length) + the raw
// bytes. readByteArray() reads a VarInt length then that many bytes.
//
// The PUBLIC constructor ENCRYPTS its inputs with an RSA public key, which is
// non-deterministic (RSA/ECB/PKCS1Padding random padding) and irrelevant to the
// WIRE FORMAT we are certifying — the codec writes whatever raw bytes already sit
// in the two fields. We therefore construct each packet via the PRIVATE
// (FriendlyByteBuf) constructor (reflection + setAccessible) fed a buffer holding
// our chosen, deterministic raw field bytes, then encode it through the REAL
// STREAM_CODEC. This isolates the byte[]/VarInt wire format exactly.
//
//   ENC <name>\t<keybytes_hex>\t<encryptedChallenge_hex>\t<readableBytes>\t<hex>
//
// keybytes/encryptedChallenge are emitted as lowercase UTF-8/binary HEX so the
// raw (possibly non-ASCII / 0x00) bytes survive the ASCII TSV transport
// (run_groundtruth.ps1 writes the TSV as ASCII); byte parity is this gate's whole
// point. The C++ side decodes the hex to the exact bytes before writeByteArray.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Constructor;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.login.ServerboundKeyPacket;

public class PktKeySbParity {
    static final java.io.PrintStream O = System.out;

    // Build a real ServerboundKeyPacket with exactly the given raw field bytes by
    // invoking its private (FriendlyByteBuf) constructor on a buffer we encode
    // ourselves (writeByteArray == VarInt len + bytes, the same form the codec
    // uses to read). This avoids the public ctor's RSA encryption entirely.
    static ServerboundKeyPacket makePacket(byte[] keybytes, byte[] encryptedChallenge) throws Exception {
        FriendlyByteBuf seed = new FriendlyByteBuf(Unpooled.buffer());
        seed.writeByteArray(keybytes);
        seed.writeByteArray(encryptedChallenge);
        Constructor<ServerboundKeyPacket> ctor =
            ServerboundKeyPacket.class.getDeclaredConstructor(FriendlyByteBuf.class);
        ctor.setAccessible(true);
        return ctor.newInstance(seed);
    }

    static byte[] seq(int len) {
        byte[] b = new byte[len];
        for (int i = 0; i < len; i++) b[i] = (byte) (i & 0xff);
        return b;
    }

    static byte[] fill(int len, int v) {
        byte[] b = new byte[len];
        for (int i = 0; i < len; i++) b[i] = (byte) v;
        return b;
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundKeyPacket> codec =
            ServerboundKeyPacket.STREAM_CODEC;

        // Finite/physical cases. Two independent VarInt-length-prefixed byte[]s.
        // We sweep: empty arrays; tiny arrays; arrays whose LENGTH crosses every
        // VarInt boundary for the length prefix (127->1B, 128->2B, 16383->2B,
        // 16384->3B); typical RSA-1024 (128B) and RSA-2048 (256B) ciphertext sizes
        // (these are the real-world keybytes/challenge sizes); arrays containing
        // 0x00 and 0xff bytes; and asymmetric lengths between the two fields.
        Object[][] cases = {
            { "both_empty",      new byte[0],     new byte[0] },
            { "key1_chal0",      fill(1, 0xab),   new byte[0] },
            { "key0_chal1",      new byte[0],     fill(1, 0xcd) },
            { "small_seq",       seq(4),          seq(8) },
            { "zeros",           fill(16, 0x00),  fill(16, 0x00) },
            { "ffs",             fill(16, 0xff),  fill(16, 0xff) },
            { "len127",          seq(127),        fill(1, 0x01) },   // key len prefix = 1 byte (0x7f)
            { "len128",          seq(128),        fill(1, 0x01) },   // key len prefix = 2 bytes (0x80 0x01)
            { "len16383",        fill(16383, 0x5a), fill(2, 0x02) }, // 2-byte prefix (0xff 0x7f)
            { "len16384",        fill(16384, 0x3c), fill(2, 0x02) }, // 3-byte prefix (0x80 0x80 0x01)
            { "rsa1024",         seq(128),        seq(128) },        // RSA-1024 ciphertext
            { "rsa2048",         seq(256),        seq(256) },        // RSA-2048 ciphertext (2-byte prefix)
            { "asym",            seq(256),        seq(16) },
            { "key_zeros_chal_seq", fill(64, 0x00), seq(64) },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            byte[] keybytes = (byte[]) c[1];
            byte[] encryptedChallenge = (byte[]) c[2];

            ServerboundKeyPacket pkt = makePacket(keybytes, encryptedChallenge);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ServerboundKeyPacket back = codec.decode(buf);
            byte[] backKey = (byte[]) keyField(back, "keybytes");
            byte[] backChal = (byte[]) keyField(back, "encryptedChallenge");
            if (!java.util.Arrays.equals(backKey, keybytes)
                || !java.util.Arrays.equals(backChal, encryptedChallenge)) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(toHex(keybytes));
            O.print('\t');
            O.print(toHex(encryptedChallenge));
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }

    static Object keyField(ServerboundKeyPacket pkt, String fieldName) throws Exception {
        java.lang.reflect.Field f = ServerboundKeyPacket.class.getDeclaredField(fieldName);
        f.setAccessible(true);
        return f.get(pkt);
    }

    static String toHex(byte[] b) {
        StringBuilder s = new StringBuilder(b.length * 2);
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }
}
