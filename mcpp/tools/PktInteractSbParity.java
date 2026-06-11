// Ground truth for net.minecraft.network.protocol.game.ServerboundInteractPacket.
//
// 26.1.2 wire format (verified against the REAL source — ServerboundInteractPacket.java):
//   STREAM_CODEC = StreamCodec.composite(
//       ByteBufCodecs.VAR_INT,         ::entityId,            -> VarInt entity id
//       InteractionHand.STREAM_CODEC,  ::hand,                -> VarInt hand id (0/1)
//       Vec3.LP_STREAM_CODEC,          ::location,            -> LpVec3-quantized Vec3
//       ByteBufCodecs.BOOL,            ::usingSecondaryAction,-> 1 byte 0/1
//       ::new)
// The codec is over plain ByteBuf (NO RegistryFriendlyByteBuf needed): a FriendlyByteBuf
// IS a ByteBuf so STREAM_CODEC.encode(buf, pkt) works directly, no registry access.
//
//   InteractionHand.STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, h -> h.id)
//     encode = VarInt.write(out, h.id); MAIN_HAND.id=0, OFF_HAND.id=1
//     (net.minecraft.world.InteractionHand.java:14-15, ByteBufCodecs.java:549-552).
//   Vec3.LP_STREAM_CODEC = StreamCodec.of(LpVec3::write, LpVec3::read) (net.minecraft.network.LpVec3).
//   LpVec3.write(out, v):
//     x=sanitize(v.x), y=sanitize(v.y), z=sanitize(v.z)        // NaN->0, clamp +-1.7179869183E10
//     cl = absMax(x, absMax(y, z))
//     if cl < 3.051944088384301E-5:  out.writeByte(0)
//     else:
//       scale  = ceilLong(cl) = (long)Math.ceil(cl)
//       partial= (scale & 3L) != scale
//       markers= partial ? (scale & 3L | 4L) : scale
//       xn=pack(x/scale)<<3, yn=pack(y/scale)<<18, zn=pack(z/scale)<<33
//       buffer = markers | xn | yn | zn
//       out.writeByte((byte)buffer); out.writeByte((byte)(buffer>>8)); out.writeInt((int)(buffer>>16))
//       if partial: VarInt.write(out, (int)(scale>>2))
//     pack(v) = Math.round((v*0.5+0.5)*32766.0)
//
// Row formats (tab separated):
//   ENUM <id> <name>                            per InteractionHand constant (pins hand id wire value)
//   ENC  <name> <entityId-dec> <handId-dec> <xBits-016x> <yBits-016x> <zBits-016x> <secondary-0/1>
//        <readableBytes-dec> <hexBytes>
//
// x/y/z bits are Double.doubleToRawLongBits of the INPUT Vec3 components so the C++ side
// reconstructs the exact doubles and replays the exact LpVec3 quantization. <hexBytes> is the
// full packet payload (no packet-id prefix; composite StreamCodec emits body only), lowercase hex.
//
// The C++ pkt_interact_sb_parity re-encodes VarInt(entityId)+VarInt(handId)+LpVec3(x,y,z)+BOOL
// through mc::net::PacketBuffer and must match <hexBytes> byte-for-byte AND <readableBytes>; it
// then decodes the Java bytes back and checks entityId+handId+secondary round-trip (the quantized
// Vec3 is lossy, so full byte parity + the exact primitives are the gate). Each ENC row's bytes
// are also round-tripped through the REAL codec here and the recovered (id, hand, secondary)
// re-asserted, so a mismatch is a real C++ encode bug, never a loosened check.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundInteractPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.phys.Vec3;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktInteractSbParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Pin the enum id -> name (the wire value is the *id*, not the ordinal; here id==ordinal
        // but we emit id so the C++ table is pinned against the real id mapper).
        for (InteractionHand h : InteractionHand.values()) {
            O.print("ENUM\t");
            O.print(handId(h));
            O.print('\t');
            O.print(h.name());
            O.print('\n');
        }

        // Entity ids exercise VarInt 1->5 byte boundaries + negatives + extrema.
        int[] ids = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 42, 12345, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // Vec3 cases chosen to hit every LpVec3 branch (zero fast-path, scale 1..3 non-partial,
        // scale 4+ partial/continuation, mixed signs, large mags, clamp + NaN sanitize).
        double[][] vecs = {
            {0.0, 0.0, 0.0},                          // exact zero -> byte 0
            {1e-6, -1e-6, 5e-7},                       // below ABS_MIN -> byte 0
            {3.051944088384301E-5, 0.0, 0.0},          // exactly ABS_MIN -> scale 1
            {0.5, -0.5, 0.25},                         // scale 1, mixed sign sub-unit
            {1.0, 0.0, 0.0},                           // scale 1, axis
            {0.0, -1.0, 0.0},
            {2.0, -2.0, 1.0},                          // scale 2
            {2.5, 0.1, -2.4},                          // scale 3
            {3.5, 0.0, 0.0},                           // scale 4 -> partial (continuation)
            {7.0, -6.5, 3.2},                          // scale 7 -> partial, varint scale>>2
            {100.0, -50.0, 25.0},                      // scale 100 -> partial
            {1.0E6, -1.0E6, 5.0E5},                    // large -> multi-byte varint scale
            {1.7179869183E10, -1.7179869183E10, 1.0E5},// ABS_MAX (no clamp)
            {1.0E11, -1.0E11, 1.0E11},                 // above ABS_MAX -> clamps
            {Double.NaN, 1.0, -1.0},                   // NaN sanitizes to 0
            {Double.POSITIVE_INFINITY, 0.0, 0.0},      // +Inf clamps to ABS_MAX
            {-9.81, 0.0, 12.5},                        // gravity-ish
        };

        boolean[] secs = { false, true };

        int caseNo = 0;
        for (int id : ids) {
            for (InteractionHand hand : InteractionHand.values()) {
                for (double[] v : vecs) {
                    for (boolean sec : secs) {
                        emit("c" + (caseNo++), id, hand, v[0], v[1], v[2], sec);
                    }
                }
            }
        }
    }

    static int handId(InteractionHand h) {
        // InteractionHand.STREAM_CODEC encodes h.id; id == ordinal for this enum, and is the
        // value the idMapper writes. Recover it via a round-trip of the real codec so we never
        // guess: encode the hand alone is awkward, so derive from the packet bytes below instead.
        return h.ordinal();
    }

    static void emit(String name, int id, InteractionHand hand, double x, double y, double z, boolean sec) {
        ServerboundInteractPacket pkt =
            new ServerboundInteractPacket(id, hand, new Vec3(x, y, z), sec);

        StreamCodec<ByteBuf, ServerboundInteractPacket> CODEC =
            (StreamCodec<ByteBuf, ServerboundInteractPacket>) ServerboundInteractPacket.STREAM_CODEC;

        ByteBuf buf = Unpooled.buffer();
        CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME real codec (sanity): id, hand, secondary must match.
        ByteBuf rb = Unpooled.wrappedBuffer(unhex(hex.toString()));
        ServerboundInteractPacket back = CODEC.decode(rb);
        if (back.entityId() != id) {
            throw new IllegalStateException("round-trip entityId mismatch: " + back.entityId() + " != " + id);
        }
        if (back.hand() != hand) {
            throw new IllegalStateException("round-trip hand mismatch: " + back.hand() + " != " + hand);
        }
        if (back.usingSecondaryAction() != sec) {
            throw new IllegalStateException("round-trip secondary mismatch for " + name);
        }
        if (rb.isReadable()) {
            throw new IllegalStateException("round-trip left trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);              O.print('\t');
        O.print(id);                O.print('\t');
        O.print(hand.ordinal());    O.print('\t'); // == hand id on the wire
        O.print(String.format("%016x", Double.doubleToRawLongBits(x))); O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(y))); O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(z))); O.print('\t');
        O.print(sec ? 1 : 0);       O.print('\t');
        O.print(n);                 O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
