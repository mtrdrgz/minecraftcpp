// Ground truth for net.minecraft.network.protocol.game.ServerboundTeleportToEntityPacket
// (formerly ServerboundTeleportToEntityPacket / "spectate" packet — sent when a spectator
//  clicks a player in the spectate menu to teleport to that entity).
//
// The packet carries a single UUID `uuid`. Its STREAM_CODEC is Packet.codec(write, new),
// where write/read are exactly:
//   write : FriendlyByteBuf.writeUUID(this.uuid)
//   read  : input.readUUID()
// (net.minecraft.network.protocol.game.ServerboundTeleportToEntityPacket lines 13-28.)
//
// FriendlyByteBuf.writeUUID(uuid) (FriendlyByteBuf.java:498-501) is exactly:
//   output.writeLong(uuid.getMostSignificantBits());   // big-endian 8 bytes
//   output.writeLong(uuid.getLeastSignificantBits());  // big-endian 8 bytes
// readUUID = new UUID(readLong(), readLong()).
// Packet.codec -> StreamCodec.ofMember: NO packet-id / length prefix, just the body, so the
// whole wire payload is 16 bytes = MSB(BE long) ++ LSB(BE long). Always 16 bytes.
//
// Row format (tab separated):
//   ENC <name> <msb-dec-signed> <lsb-dec-signed> <readableBytes-dec> <hex>
// where msb/lsb are the signed decimal longs of the UUID's most/least significant bits
// (the exact arguments writeLong receives). hex is the lowercase wire bytes.
//
// We round-trip-decode every case through the SAME codec and assert UUID equality as a
// sanity check before emitting. The C++ pkt_spectate_entity_sb_parity rebuilds the wire
// from (msb,lsb) via PacketBuffer.writeUUID and must match <hex> byte-for-byte (+
// readableBytes); it also decodes <hex> via readUUID and checks the recovered (msb,lsb).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.UUID;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundTeleportToEntityPacket;
import net.minecraft.server.Bootstrap;

public class PktTeleportToEntitySbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundTeleportToEntityPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundTeleportToEntityPacket>)
                ServerboundTeleportToEntityPacket.STREAM_CODEC;

        // Finite/physical input battery for the UUID. A UUID is two signed longs (MSB,LSB)
        // each emitted verbatim as a big-endian 8-byte long. We pin: all-zero, all-one
        // (0xffff... per long -> -1), sign-bit boundaries, byte-pattern UUIDs, and a couple
        // of real random ones. No zig-zag / no VarInt here — fixed 16 bytes always.
        long[][] cases = {
            {0L, 0L},                                       // nil UUID
            {-1L, -1L},                                     // 0xffff...ffff both longs
            {0L, 1L},                                       // smallest non-zero LSB
            {1L, 0L},                                       // smallest non-zero MSB
            {Long.MIN_VALUE, Long.MIN_VALUE},               // 0x8000...0000 (sign bit set)
            {Long.MAX_VALUE, Long.MAX_VALUE},               // 0x7fff...ffff
            {Long.MIN_VALUE, Long.MAX_VALUE},               // mixed extremes
            {Long.MAX_VALUE, Long.MIN_VALUE},
            {0x0123456789abcdefL, 0xfedcba9876543210L},     // ascending / descending nibbles
            {0x00000000000000ffL, 0xff00000000000000L},     // single-byte at each end
            {-2L, -128L},                                   // small negatives
            {0xdeadbeefcafebabeL, 0x1122334455667788L},
            // A few real-generated UUIDs (deterministic via fixed bit patterns above is
            // enough, but include genuine randomUUID() to exercise the type-4 variant bits).
        };

        // Emit the fixed-pattern cases.
        for (int i = 0; i < cases.length; i++) {
            emit(CODEC, "fixed" + i, new UUID(cases[i][0], cases[i][1]));
        }

        // Plus a handful of genuine random UUIDs (type-4) for good measure.
        java.util.Random rng = new java.util.Random(26012L); // deterministic seed
        for (int i = 0; i < 4; i++) {
            long msb = rng.nextLong();
            long lsb = rng.nextLong();
            // Force RFC-4122 type-4 variant/version bits like UUID.randomUUID() would.
            msb &= ~0xF000L; msb |= 0x4000L;                // version 4
            lsb &= ~0xC000000000000000L; lsb |= 0x8000000000000000L; // variant 2
            emit(CODEC, "rand" + i, new UUID(msb, lsb));
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundTeleportToEntityPacket> CODEC,
                     String name, UUID uuid) throws Exception {
        // ENC: construct the real packet (public ctor), encode through the real codec, dump.
        ServerboundTeleportToEntityPacket pkt = new ServerboundTeleportToEntityPacket(uuid);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec; recover the UUID via reflection
        // (the field is private) and assert equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundTeleportToEntityPacket dec = CODEC.decode(rbuf);
        UUID got = readUuidField(dec);
        if (!got.equals(uuid)) {
            throw new IllegalStateException("round-trip mismatch: in=" + uuid + " out=" + got);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(uuid.getMostSignificantBits());   // signed decimal long (writeLong arg)
        O.print('\t');
        O.print(uuid.getLeastSignificantBits());   // signed decimal long (writeLong arg)
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
    }

    static UUID readUuidField(ServerboundTeleportToEntityPacket p) throws Exception {
        java.lang.reflect.Field f = ServerboundTeleportToEntityPacket.class.getDeclaredField("uuid");
        f.setAccessible(true);
        return (UUID) f.get(p);
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
