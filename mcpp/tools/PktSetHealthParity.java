// Ground truth for net.minecraft.network.protocol.game.ClientboundSetHealthPacket.
//
// The packet carries three fields and its body is exactly (real 26.1.2 source,
// ClientboundSetHealthPacket lines 22-32):
//   write : FriendlyByteBuf.writeFloat(this.health)     // raw int bits, big-endian
//           FriendlyByteBuf.writeVarInt(this.food)      // LEB128, no zig-zag
//           FriendlyByteBuf.writeFloat(this.saturation) // raw int bits, big-endian
//   read  : float health = input.readFloat();
//           int   food   = input.readVarInt();
//           float saturation = input.readFloat();
// STREAM_CODEC = Packet.codec(write, new) -> StreamCodec.ofMember: NO packet-id
// prefix, just the body. So the wire payload is: 4B float + VarInt(food) + 4B float.
//
// Row format (tab separated), TAG = ENC:
//   ENC <healthBits-08x> <food-dec> <saturationBits-08x> <readableBytes-dec> <hex>
// where healthBits / saturationBits are Float.floatToRawIntBits(...) as lowercase
// 8-hex (the EXACT big-endian wire bytes for the float), food is decimal, <hex> is
// the full packet payload lowercase hex.
//
// We round-trip-decode every case through the SAME codec and assert bit-exact field
// equality (raw int bits for floats) as a sanity check before emitting. The C++
// pkt_set_health_parity rebuilds the packet from these fields, re-encodes via
// PacketBuffer (writeFloat/writeVarInt/writeFloat), and must match <hex> byte-for-byte
// (+ readableBytes); it also decodes <hex> and checks the recovered fields bit-exactly.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetHealthPacket;
import net.minecraft.server.Bootstrap;

public class PktSetHealthParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundSetHealthPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetHealthPacket>)
                ClientboundSetHealthPacket.STREAM_CODEC;

        // Finite/physical input battery.
        // health/saturation are floats; in vanilla health is 0..20 and saturation 0..20,
        // but the codec is a plain writeFloat so we also pin signs, fractions, zero,
        // negative-zero and a few special-but-finite values to stress the raw-bits path.
        float[] floats = {
            0.0f, -0.0f, 1.0f, -1.0f,
            20.0f, 0.5f, 19.5f, 6.0f, 18.0f, 0.025f,
            3.4028235E38f, -3.4028235E38f,   // Float.MAX_VALUE / -MAX_VALUE
            1.4E-45f,                        // Float.MIN_VALUE (smallest positive subnormal)
            0.123456f, -0.123456f, 123456.78f, -123456.78f,
            2.0f, 4.0f, 16.0f, 17.5f
        };

        // food is a VarInt (vanilla 0..20) — pin the common range AND every LEB128 byte
        // boundary (1->2->3->4->5 bytes) plus the int extremes (negatives encode as 5
        // bytes since writeVarInt does NOT zig-zag).
        int[] foods = {
            0, 1, 2, 7, 10, 17, 20,
            127, 128, 129,                   // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,             // 2->3 byte boundary
            2097151, 2097152, 2097153,       // 3->4 byte boundary
            268435455, 268435456, 268435457, // 4->5 byte boundary
            123456789,
            Integer.MAX_VALUE,               // 0x7fffffff -> 5 bytes
            -1, -2, -128, -16384, -2097152,
            Integer.MIN_VALUE                // 0x80000000 -> 5 bytes
        };

        // (A) Physical sweep: vanilla-realistic combos (health, food, saturation) over a
        //     clamped index walk so floats and foods rotate through their batteries.
        int n = Math.max(floats.length, foods.length);
        for (int i = 0; i < n; i++) {
            float health = floats[i % floats.length];
            int food = foods[i % foods.length];
            float sat = floats[(i + 3) % floats.length];
            emit(CODEC, health, food, sat);
        }

        // (B) Cross product of float extremes x food boundaries to fully exercise the
        //     float-bit path independently of the VarInt path.
        float[] keyFloats = { 0.0f, -0.0f, 20.0f, -1.0f, 3.4028235E38f, 1.4E-45f };
        for (float h : keyFloats) {
            for (int food : foods) {
                emit(CODEC, h, food, 0.0f);
            }
        }
        // (C) A few hand-picked exact vanilla-on-death / full-health states.
        emit(CODEC, 20.0f, 20, 5.0f);  // full health, full food
        emit(CODEC, 0.0f, 0, 0.0f);    // dead
        emit(CODEC, 1.0f, 18, 2.5f);   // hurt + partial saturation
    }

    static void emit(StreamCodec<FriendlyByteBuf, ClientboundSetHealthPacket> CODEC,
                     float health, int food, float saturation) {
        // ENC: construct the real packet, encode through the real codec, dump bytes.
        ClientboundSetHealthPacket pkt = new ClientboundSetHealthPacket(health, food, saturation);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec; assert bit-exact equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundSetHealthPacket dec = CODEC.decode(rbuf);
        if (Float.floatToRawIntBits(dec.getHealth()) != Float.floatToRawIntBits(health)
            || dec.getFood() != food
            || Float.floatToRawIntBits(dec.getSaturation()) != Float.floatToRawIntBits(saturation)) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + health + "," + food + "," + saturation
                + ") out=(" + dec.getHealth() + "," + dec.getFood() + "," + dec.getSaturation() + ")");
        }

        O.print("ENC\t");
        O.print(String.format("%08x", Float.floatToRawIntBits(health)));
        O.print('\t');
        O.print(food);
        O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(saturation)));
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
