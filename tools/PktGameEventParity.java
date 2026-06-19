// Ground truth for net.minecraft.network.protocol.game.ClientboundGameEventPacket's
// StreamCodec.
//
// The packet body is exactly (ClientboundGameEventPacket.java:46-49):
//   write : FriendlyByteBuf.writeByte(this.event.id)  // low 8 bits of the Type id (0..13)
//           FriendlyByteBuf.writeFloat(this.param)     // big-endian IEEE-754 float (4 bytes)
//   read  (ClientboundGameEventPacket.java:41-44):
//           this.event = Type.TYPES.get(input.readUnsignedByte()); // 0..255
//           this.param = input.readFloat();
// STREAM_CODEC = Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember,
// so NO packet-id prefix on the wire, just the body: 1 byte(eventType) then 4 bytes(float).
//
// Both fields are plain numbers (unsigned byte + IEEE-754 float); no registry /
// ItemStack / Component / Holder / NBT — fully representable by the certified
// PacketBuffer (FriendlyByteBuf) port. The Type id and param come VERBATIM from the
// real codec. We construct each packet via the PUBLIC ctor (Type, float) using the
// real registered Type constants, encode through the REAL STREAM_CODEC, and round-trip
// decode for sanity.
//
// Row formats (tab separated). eventType is the decimal Type.id written to the wire;
// param is emitted as %08x of Float.floatToRawIntBits (so NaN payload / -0.0 are exact);
// readableBytes decimal; hexBytes lowercase hex:
//   ENC <eventType> <paramRawBits> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <eventType_in> <paramRawBits_in> <eventType_decoded> <paramRawBits_decoded>
//        decode: STREAM_CODEC.decode(buf) -> getEvent().id / getParam() round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Field;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundGameEventPacket;

public class PktGameEventParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundGameEventPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundGameEventPacket>)
                ClientboundGameEventPacket.STREAM_CODEC;

        // The 14 real registered Type constants (ids 0..13). These are the only valid
        // event types — Type.TYPES.get(id) on read returns one of these. We pull them
        // by name so the ids come VERBATIM from the source constants, not a hardcoded list.
        ClientboundGameEventPacket.Type[] types = {
            ClientboundGameEventPacket.NO_RESPAWN_BLOCK_AVAILABLE, // 0
            ClientboundGameEventPacket.START_RAINING,              // 1
            ClientboundGameEventPacket.STOP_RAINING,               // 2
            ClientboundGameEventPacket.CHANGE_GAME_MODE,           // 3
            ClientboundGameEventPacket.WIN_GAME,                   // 4
            ClientboundGameEventPacket.DEMO_EVENT,                 // 5
            ClientboundGameEventPacket.PLAY_ARROW_HIT_SOUND,       // 6
            ClientboundGameEventPacket.RAIN_LEVEL_CHANGE,          // 7
            ClientboundGameEventPacket.THUNDER_LEVEL_CHANGE,       // 8
            ClientboundGameEventPacket.PUFFER_FISH_STING,          // 9
            ClientboundGameEventPacket.GUARDIAN_ELDER_EFFECT,      // 10
            ClientboundGameEventPacket.IMMEDIATE_RESPAWN,          // 11
            ClientboundGameEventPacket.LIMITED_CRAFTING,           // 12
            ClientboundGameEventPacket.LEVEL_CHUNKS_LOAD_START,    // 13
        };

        // private final int Type.id — read it for the expected wire byte / round-trip id.
        Field idField = ClientboundGameEventPacket.Type.class.getDeclaredField("id");
        idField.setAccessible(true);

        // param battery: documented DEMO_PARAM_* constants + game-mode ordinals + the
        // rain/thunder level [0,1] range + IEEE-754 edge cases (zeros, sign, subnormal,
        // min/max, infinities, NaN). param is a float on the wire; these are the
        // physically-meaningful values plus float-codec boundary cases.
        float[] params = {
            // documented demo params
            (float) ClientboundGameEventPacket.DEMO_PARAM_INTRO,  // 0
            (float) ClientboundGameEventPacket.DEMO_PARAM_HINT_1, // 101
            (float) ClientboundGameEventPacket.DEMO_PARAM_HINT_2, // 102
            (float) ClientboundGameEventPacket.DEMO_PARAM_HINT_3, // 103
            (float) ClientboundGameEventPacket.DEMO_PARAM_HINT_4, // 104
            // CHANGE_GAME_MODE param == GameType ordinal (0..3)
            0.0f, 1.0f, 2.0f, 3.0f,
            // RAIN/THUNDER_LEVEL_CHANGE param in [0,1]
            0.5f, 0.25f, 0.75f,
            // sign / zero
            -0.0f, -1.0f, -2.5f, 1.5f,
            // float-codec boundary cases
            Float.MIN_VALUE,            // smallest positive subnormal
            Float.MIN_NORMAL,           // smallest positive normal
            Float.MAX_VALUE,            // largest finite
            Float.POSITIVE_INFINITY,
            Float.NEGATIVE_INFINITY,
            Float.NaN,
            3.1415927f,
        };

        for (ClientboundGameEventPacket.Type type : types) {
            int eventId = idField.getInt(type);
            for (float param : params) {
                int paramBits = Float.floatToRawIntBits(param);

                // Build a packet with this exact (Type, param) via the PUBLIC ctor.
                ClientboundGameEventPacket pkt = new ClientboundGameEventPacket(type, param);

                // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                CODEC.encode(buf, pkt);
                int n = buf.readableBytes();
                String hex = toHex(buf);
                O.print("ENC\t");
                O.print(eventId);
                O.print('\t');
                O.print(paramBits);
                O.print('\t');
                O.print(n);
                O.print('\t');
                O.print(hex);
                O.print('\n');

                // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                ClientboundGameEventPacket dec = CODEC.decode(rbuf);
                int decId = idField.getInt(dec.getEvent());
                int decParamBits = Float.floatToRawIntBits(dec.getParam());
                if (decId != eventId)
                    throw new IllegalStateException("eventType round-trip " + eventId + " -> " + decId);
                if (decParamBits != paramBits)
                    throw new IllegalStateException("param round-trip " + paramBits + " -> " + decParamBits);
                O.print("DEC\t");
                O.print(hex);
                O.print('\t');
                O.print(eventId);
                O.print('\t');
                O.print(paramBits);
                O.print('\t');
                O.print(decId);
                O.print('\t');
                O.print(decParamBits);
                O.print('\n');
            }
        }
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
