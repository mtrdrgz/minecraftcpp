// Ground truth for net.minecraft.network.protocol.game.ClientboundSetExperiencePacket.
//
// Source (26.1.2/src/net/minecraft/network/protocol/game/ClientboundSetExperiencePacket.java):
//   ctor       : ClientboundSetExperiencePacket(float experienceProgress, int totalExperience, int experienceLevel)
//   STREAM_CODEC: Packet.codec(ClientboundSetExperiencePacket::write, ClientboundSetExperiencePacket::new)
//   write(buf) : buf.writeFloat(this.experienceProgress);
//                buf.writeVarInt(this.experienceLevel);   // LEVEL written FIRST
//                buf.writeVarInt(this.totalExperience);   // TOTAL written SECOND
//   read(buf)  : experienceProgress = buf.readFloat();
//                experienceLevel     = buf.readVarInt();
//                totalExperience     = buf.readVarInt();
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body. So the whole
// wire payload is exactly: FLOAT(experienceProgress, 4B BE IEEE-754) ++ VARINT(level) ++
// VARINT(total). NOTE the ON-WIRE order is progress, LEVEL, TOTAL (the constructor arg
// order is total before level, but write() emits level before total).
//
// Row format (tab separated), TAG = ENC:
//   ENC <progressBits-08x> <level-dec> <total-dec> <readableBytes-dec> <hex>
// where <progressBits-08x> is Float.floatToRawIntBits(experienceProgress) as lowercase
// %08x (so the exact float is reproduced bit-for-bit without text rounding), <level> and
// <total> are decimal signed ints, and <hex> is the full packet payload, lowercase hex.
//
// Every case is round-trip-decoded through the SAME codec and asserted equal (sanity)
// before emitting. The C++ pkt_set_experience_parity rebuilds the packet from these
// fields, re-encodes via PacketBuffer (writeFloat + writeVarInt + writeVarInt in the same
// order), and must match <hex> byte-for-byte (+ readableBytes); it also decodes <hex> via
// readFloat + readVarInt + readVarInt and checks the recovered fields bit-exact.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetExperiencePacket;
import net.minecraft.server.Bootstrap;

public class PktSetExperienceParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundSetExperiencePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetExperiencePacket>)
                ClientboundSetExperiencePacket.STREAM_CODEC;

        // experienceProgress: vanilla sends Player.experienceProgress, a 0..1 bar fill.
        // We cover the physical [0,1] band plus exact bit-pattern edges (+/-0, 1, sub-bar
        // fractions) and a few out-of-band finite floats to prove the raw 4-byte float is
        // copied verbatim (no clamping in the codec).
        float[] progresses = {
            0.0f, -0.0f, 1.0f, 0.5f, 0.25f, 0.75f, 0.1f, 0.333333f, 0.999999f,
            0.0009765625f, 0.123456f, 2.0f, -1.0f, 3.4028235e38f, 1.4e-45f
        };

        // experienceLevel: a player level (0..21863+ in practice). VarInt (LEB128, no
        // zig-zag). Pin every 1->2->3->4->5 byte boundary and the int extremes; negatives
        // encode as 5 bytes (the protocol never sends them, but the codec must round-trip).
        int[] levels = {
            0, 1, 5, 30, 100,
            127, 128, 129,                    // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,              // 2->3 byte boundary
            2097151, 2097152, 2097153,        // 3->4 byte boundary
            268435455, 268435456, 268435457,  // 4->5 byte boundary
            21863,
            Integer.MAX_VALUE,                // 0x7fffffff -> 5 bytes
            -1, -128, Integer.MIN_VALUE       // 5 bytes (no zig-zag)
        };

        // totalExperience: cumulative XP (large positives in practice). Independent VarInt.
        int[] totals = {
            0, 1, 7, 42, 100, 550, 1395,
            127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            123456789,
            Integer.MAX_VALUE,
            -1, Integer.MIN_VALUE
        };

        // (A) sweep progress with a fixed nominal level/total (the common wire case).
        for (float p : progresses) {
            emit(CODEC, p, 30, 1395);
        }
        // (B) sweep level (VarInt boundaries) with fixed progress/total.
        for (int lvl : levels) {
            emit(CODEC, 0.5f, lvl, 1395);
        }
        // (C) sweep total (VarInt boundaries) with fixed progress/level.
        for (int tot : totals) {
            emit(CODEC, 0.25f, 30, tot);
        }
        // (D) a few combined extremes to exercise level+total VarInts together.
        emit(CODEC, 0.0f, 0, 0);
        emit(CODEC, 1.0f, Integer.MAX_VALUE, Integer.MAX_VALUE);
        emit(CODEC, -0.0f, Integer.MIN_VALUE, Integer.MIN_VALUE);
        emit(CODEC, 0.999999f, 16384, 2097152);
        emit(CODEC, 0.0009765625f, 128, 16384);
    }

    // Construct the real packet, encode through the real STREAM_CODEC, sanity round-trip,
    // and emit the ENC row. Constructor arg order is (progress, total, level).
    static void emit(StreamCodec<FriendlyByteBuf, ClientboundSetExperiencePacket> CODEC,
                     float progress, int level, int total) {
        ClientboundSetExperiencePacket pkt =
            new ClientboundSetExperiencePacket(progress, total, level);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert field equality
        // (float compared by raw bits to catch -0.0 / NaN-style differences).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundSetExperiencePacket dec = CODEC.decode(rbuf);
        if (Float.floatToRawIntBits(dec.getExperienceProgress()) != Float.floatToRawIntBits(progress)
                || dec.getExperienceLevel() != level
                || dec.getTotalExperience() != total) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + progress + "," + level + "," + total + ")"
                + " out=(" + dec.getExperienceProgress() + "," + dec.getExperienceLevel()
                + "," + dec.getTotalExperience() + ")");
        }

        O.print("ENC\t");
        O.print(String.format("%08x", Float.floatToRawIntBits(progress)));
        O.print('\t');
        O.print(level);
        O.print('\t');
        O.print(total);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
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
