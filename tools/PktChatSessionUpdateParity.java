// Ground truth for net.minecraft.network.protocol.game.ServerboundChatSessionUpdatePacket.
//
// Pure-wire packet (plain FriendlyByteBuf, NO registry/world state):
//   STREAM_CODEC = Packet.codec(write, ServerboundChatSessionUpdatePacket::new)
//   write (ServerboundChatSessionUpdatePacket.java:18-20):
//       RemoteChatSession.Data.write(output, this.chatSession);
//   RemoteChatSession.Data.write (RemoteChatSession.java:32-35):
//       output.writeUUID(data.sessionId);          // writeLong(msb) + writeLong(lsb)
//       data.profilePublicKey.write(output);
//   ProfilePublicKey.Data.write (ProfilePublicKey.java:52-56):
//       output.writeInstant(this.expiresAt);        // writeLong(epochMilli)  (8 BE bytes)
//       output.writePublicKey(this.key);            // writeByteArray(key.getEncoded())
//       output.writeByteArray(this.keySignature);   // writeByteArray(sig)
//   writeUUID  (FriendlyByteBuf.java:498-501) = writeLong(msb) + writeLong(lsb)  (16 BE bytes)
//   writeInstant (FriendlyByteBuf.java:608-610) = writeLong(toEpochMilli())       (8 BE bytes)
//   writePublicKey (FriendlyByteBuf.java:620-623) = writeByteArray(key.getEncoded())
//   writeByteArray (FriendlyByteBuf.java:289-292) = VarInt.write(len) + raw bytes
//
//   So the body is exactly:
//       long  msb                                  (writeLong, 8 BE bytes)
//       long  lsb                                  (writeLong, 8 BE bytes)
//       long  expiresAt.toEpochMilli()             (writeLong, 8 BE bytes)
//       VarInt(keyLen)  keyBytes  (raw)            (the X.509 encoded public key)
//       VarInt(sigLen)  sigBytes  (raw)            (the key signature)
//
// The wire codec writes both byte arrays VERBATIM, so the C++ side never does any crypto:
// it just rebuilds the exact bytes from the columns. We still construct the REAL packet from
// a REAL java.security.PublicKey and let the REAL STREAM_CODEC produce the bytes.
//
// Row format (tab separated):
//   ENC <msb-dec> <lsb-dec> <instantMillis-dec> <keyHex> <sigHex-or-_> <readableBytes-dec> <hex>
//   sigHex token "_" means a zero-length signature.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.PublicKey;
import java.time.Instant;
import java.util.UUID;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.chat.RemoteChatSession;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundChatSessionUpdatePacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.entity.player.ProfilePublicKey;

public class PktChatSessionUpdateParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (plain FriendlyByteBuf, no registry).
        StreamCodec<FriendlyByteBuf, ServerboundChatSessionUpdatePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundChatSessionUpdatePacket>)
                ServerboundChatSessionUpdatePacket.STREAM_CODEC;

        // Two REAL public keys so the encoded-key byte array (and its VarInt length) varies.
        // Vanilla uses RSA-2048; we exercise that plus a smaller modulus to vary the length.
        KeyPairGenerator kpg = KeyPairGenerator.getInstance("RSA");
        kpg.initialize(2048);
        PublicKey key2048 = kpg.generateKeyPair().getPublic();
        kpg.initialize(1024);
        PublicKey key1024 = kpg.generateKeyPair().getPublic();

        // sessionId (msb,lsb), expiresAt millis, which key, and the raw signature bytes.
        // The signature is an opaque byte array on the wire, so we drive it with crafted
        // byte patterns that pin the VarInt length boundaries (0, 1, 127, 128, ...) and a
        // realistic 256-byte RSA-2048 signature length.
        Object[][] cases = {
            // {msb(long), lsb(long), instantMillis(long), key, sigLen(int)}
            {0L, 0L, 0L, key2048, 256},
            {1L, 2L, 1700000000000L, key2048, 256},
            {-1L, -1L, 9223372036854775807L, key2048, 0},          // empty signature
            {0x0123456789abcdefL, 0xfedcba9876543210L, 1L, key1024, 1},
            {Long.MIN_VALUE, Long.MAX_VALUE, -1L, key2048, 127},   // 1->2 VarInt boundary
            {Long.MAX_VALUE, Long.MIN_VALUE, 1234567890123L, key1024, 128}, // 2-byte VarInt len
            {123456789L, 987654321L, 0L, key2048, 300},            // >256 sig
            {-9223372036854775808L, 0L, 4102444800000L, key1024, 16},
        };

        for (Object[] c : cases) {
            long msb = (Long) c[0];
            long lsb = (Long) c[1];
            long millis = (Long) c[2];
            PublicKey key = (PublicKey) c[3];
            int sigLen = (Integer) c[4];

            // Deterministic signature bytes (pattern depends on length so each case differs).
            byte[] sig = new byte[sigLen];
            for (int i = 0; i < sigLen; i++) sig[i] = (byte) ((i * 31 + sigLen) & 0xff);

            UUID sessionId = new UUID(msb, lsb);
            Instant expiresAt = Instant.ofEpochMilli(millis);
            ProfilePublicKey.Data keyData = new ProfilePublicKey.Data(expiresAt, key, sig);
            RemoteChatSession.Data session = new RemoteChatSession.Data(sessionId, keyData);
            ServerboundChatSessionUpdatePacket pkt = new ServerboundChatSessionUpdatePacket(session);

            // ENCODE through the real codec, dump bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality of fields.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundChatSessionUpdatePacket dec = CODEC.decode(rbuf);
            RemoteChatSession.Data ds = dec.chatSession();
            if (ds.sessionId().getMostSignificantBits() != msb
                || ds.sessionId().getLeastSignificantBits() != lsb
                || ds.profilePublicKey().expiresAt().toEpochMilli() != millis
                || !ds.profilePublicKey().key().equals(key)
                || !java.util.Arrays.equals(ds.profilePublicKey().keySignature(), sig)) {
                throw new IllegalStateException("round-trip mismatch for msb=" + msb);
            }

            String keyHex = bytesToHex(key.getEncoded());
            String sigHex = sigLen == 0 ? "_" : bytesToHex(sig);

            O.print("ENC\t");
            O.print(msb);
            O.print('\t');
            O.print(lsb);
            O.print('\t');
            O.print(millis);
            O.print('\t');
            O.print(keyHex);
            O.print('\t');
            O.print(sigHex);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }

    static String bytesToHex(byte[] b) {
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (byte x : b) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
    }

    static String toHex(FriendlyByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
