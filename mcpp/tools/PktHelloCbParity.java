// Ground truth for net.minecraft.network.protocol.login.ClientboundHelloPacket.
//
// The packet has four fields (String serverId, byte[] publicKey, byte[] challenge,
// boolean shouldAuthenticate). Its STREAM_CODEC is built via Packet.codec(write, new)
// and write(FriendlyByteBuf) is EXACTLY (verbatim from 26.1.2/src):
//     output.writeUtf(this.serverId);          // writeUtf(value, 32767): VarInt UTF-8 byte length + UTF-8 bytes
//     output.writeByteArray(this.publicKey);    // VarInt length + raw bytes
//     output.writeByteArray(this.challenge);    // VarInt length + raw bytes
//     output.writeBoolean(this.shouldAuthenticate);
// read(FriendlyByteBuf) is: readUtf(20); readByteArray(); readByteArray(); readBoolean().
// No packet-type id is part of the codec bytes (that framing lives outside the StreamCodec).
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf and dump
// readableBytes() + the raw hex; we also decode the bytes back through the SAME codec and
// re-emit serverId/publicKey/challenge/shouldAuthenticate so the C++ side proves read parity.
//
//   ENC \t <name> \t <serverIdHex> \t <publicKeyHex> \t <challengeHex> \t <shouldAuthenticate> \t <readableBytes> \t <hex>
//
// serverId, publicKey, and challenge are emitted as LOWERCASE UTF-8/binary HEX so the exact
// bytes survive the ASCII TSV transport (run_groundtruth.ps1 writes the TSV as ASCII and would
// mangle raw multi-byte UTF-8 / binary). shouldAuthenticate is decimal (0/1).
//
// NOTE: serverId is read back via readUtf(20), so the *decoded UTF-16 length* must be <= 20.
// All test serverId strings are kept within that bound. The encode side uses writeUtf (max
// 32767) so the encode is never the limiting factor; we keep them short to stay round-trippable.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.Arrays;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.login.ClientboundHelloPacket;

public class PktHelloCbParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }

    // The packet's publicKey is a private byte[] with no byte[] getter (getPublicKey()
    // decodes RSA and would reject our synthetic bytes). Read the field reflectively so
    // the round-trip sanity check compares the exact decoded byte[].
    static byte[] readPublicKeyField(ClientboundHelloPacket pkt) throws Exception {
        java.lang.reflect.Field f = ClientboundHelloPacket.class.getDeclaredField("publicKey");
        f.setAccessible(true);
        return (byte[]) f.get(pkt);
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundHelloPacket> codec =
            ClientboundHelloPacket.STREAM_CODEC;

        // Finite/physical cases. serverId kept within the readUtf(20) UTF-16 bound.
        // publicKey/challenge sweep byte-array lengths (incl. empty, single, and a length
        // crossing the VarInt 1->2 byte boundary at 128) plus full 0x00..0xff byte coverage.
        byte[] empty = new byte[0];
        byte[] oneByte = new byte[] { (byte) 0x7f };
        byte[] signByte = new byte[] { (byte) 0x80, (byte) 0xff, (byte) 0x00 };
        byte[] allBytes = new byte[256];
        for (int i = 0; i < 256; i++) allBytes[i] = (byte) i;       // length 256 -> VarInt 2 bytes
        byte[] len127 = new byte[127];
        for (int i = 0; i < 127; i++) len127[i] = (byte) (i + 1);    // length 127 -> VarInt 1 byte
        byte[] len128 = new byte[128];
        for (int i = 0; i < 128; i++) len128[i] = (byte) (i & 0xff); // length 128 -> VarInt 2 bytes (boundary)
        byte[] challenge4 = new byte[] { (byte) 0xde, (byte) 0xad, (byte) 0xbe, (byte) 0xef };

        Object[][] cases = {
            // name, serverId, publicKey, challenge, shouldAuthenticate
            { "empty_all_false",   "",                   empty,     empty,      false },
            { "empty_all_true",    "",                   empty,     empty,      true },
            { "ascii_server",      "MyServerId12345",    challenge4, challenge4, true },
            { "max20_ascii",       "01234567890123456789", allBytes, challenge4, false },
            { "unicode_server",    "niñoäö", challenge4, signByte, true },
            { "one_byte_arrays",   "x",                  oneByte,   oneByte,    false },
            { "sign_bytes",        "s",                  signByte,  signByte,   true },
            { "allbytes_pk",       "k",                  allBytes,  challenge4, true },
            { "len127_pk",         "a",                  len127,    challenge4, false },
            { "len128_pk_boundary","b",                  len128,    len128,     true },
            { "empty_pk_full_ch",  "c",                  empty,     allBytes,   false },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            String serverId = (String) c[1];
            byte[] publicKey = (byte[]) c[2];
            byte[] challenge = (byte[]) c[3];
            boolean shouldAuthenticate = (Boolean) c[4];

            ClientboundHelloPacket pkt =
                new ClientboundHelloPacket(serverId, publicKey, challenge, shouldAuthenticate);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ClientboundHelloPacket back = codec.decode(buf);
            if (!back.getServerId().equals(serverId)
                    || !Arrays.equals(readPublicKeyField(back), publicKey)
                    || !Arrays.equals(back.getChallenge(), challenge)
                    || back.shouldAuthenticate() != shouldAuthenticate) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            // serverId as UTF-8 HEX; publicKey/challenge as binary HEX (ASCII-safe TSV transport).
            String serverIdHex = hex(serverId.getBytes(java.nio.charset.StandardCharsets.UTF_8));

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(serverIdHex);
            O.print('\t');
            O.print(hex(publicKey));
            O.print('\t');
            O.print(hex(challenge));
            O.print('\t');
            O.print(shouldAuthenticate ? 1 : 0);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
