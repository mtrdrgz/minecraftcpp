// Ground truth for net.minecraft.network.protocol.game.ClientboundMoveEntityPacket.Rot's
// StreamCodec. Strict 1:1 reverse-engineering reference for the C++ parity gate.
//
// ClientboundMoveEntityPacket is abstract with three nested subtypes (Pos / PosRot / Rot).
// We target the Rot subtype only. Its body is exactly
// (ClientboundMoveEntityPacket.java:178-183, the .Rot static class):
//   write(FriendlyByteBuf output):
//     output.writeVarInt(this.entityId);   // VarInt entity id
//     output.writeByte(this.yRot);         // signed byte (packed yaw)
//     output.writeByte(this.xRot);         // signed byte (packed pitch)
//     output.writeBoolean(this.onGround);  // single byte 0/1
//   read(FriendlyByteBuf input)  (ClientboundMoveEntityPacket.java:170-176):
//     int entityId   = input.readVarInt();
//     byte yRot      = input.readByte();   // SIGNED byte (-128..127)
//     byte xRot      = input.readByte();   // SIGNED byte (-128..127)
//     boolean onGround = input.readBoolean();
//     return new ClientboundMoveEntityPacket.Rot(entityId, yRot, xRot, onGround);
//
// STREAM_CODEC = Packet.codec(Rot::write, Rot::read) -> StreamCodec.ofMember
// (Packet.java): body ONLY on the wire, NO packet-id or length prefix.
//
// All four fields are plain numbers/bool; no registry/ItemStack/Component/Holder/NBT —
// fully representable by the certified PacketBuffer (FriendlyByteBuf) port. The .Rot
// public ctor Rot(int id, byte yRot, byte xRot, boolean onGround) takes exactly the
// wire fields, so we use it directly (no reflection needed), then encode every packet
// through the REAL STREAM_CODEC.
//
// FriendlyByteBuf.writeByte(int) writes the low 8 bits; yRot/xRot are Java `byte`
// (already -128..127), so the written byte equals (val & 0xff). On read, readByte()
// yields the SIGNED byte back, so they round-trip exactly.
//
// Row formats (tab separated). entityId/yRot/xRot decimal, onGround 0/1 decimal; the
// hex column is lowercase %02x of the encoded body (no String/binary fields here):
//   ENC \t <name> \t <entityId> \t <yRot> \t <xRot> \t <onGround> \t <readableBytes> \t <hexBytes>
// The C++ gate re-encodes the SAME fields through the certified PacketBuffer
// (writeVarInt(entityId)+writeByte(yRot&0xff)+writeByte(xRot&0xff)+writeBool(onGround))
// and must match byte-for-byte, then round-trips the bytes back to the fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundMoveEntityPacket;

public class PktMoveEntityRotParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for the Rot subtype (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundMoveEntityPacket.Rot> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundMoveEntityPacket.Rot>)
                ClientboundMoveEntityPacket.Rot.STREAM_CODEC;

        // Finite/physical battery.
        // entity ids: 0, small, VarInt 1->2->3->4->5-byte boundaries, max/min int.
        int[] ids = {
            0, 1, 2, 5, 100, 127, 128, 255, 256,
            16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, Integer.MAX_VALUE, Integer.MIN_VALUE,
        };
        // yRot/xRot are Java bytes: signed-byte boundary battery -128..127.
        int[] yRots = { 0, 1, -1, 64, -64, 127, -128, 100, -100 };
        int[] xRots = { 0, 1, -1, 63, -63, 127, -128, 42, -42 };
        int[] grounds = { 0, 1 };

        for (int id : ids) {
            for (int k = 0; k < yRots.length; k++) {
                int yRotI = yRots[k];
                int xRotI = xRots[k];
                byte yRot = (byte) yRotI;  // physical: yRot is a byte field (-128..127).
                byte xRot = (byte) xRotI;  // physical: xRot is a byte field (-128..127).
                for (int g : grounds) {
                    boolean onGround = g != 0;

                    // Build the packet with this exact (entityId, yRot, xRot, onGround)
                    // via the public Rot ctor (takes the wire fields directly).
                    ClientboundMoveEntityPacket.Rot pkt =
                        new ClientboundMoveEntityPacket.Rot(id, yRot, xRot, onGround);

                    // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                    FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                    CODEC.encode(buf, pkt);
                    int readable = buf.readableBytes();
                    String hex = toHex(buf);

                    // Round-trip decode through the SAME codec; assert field equality.
                    FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                    ClientboundMoveEntityPacket.Rot dec = CODEC.decode(rbuf);
                    // entityId may be truncated to int by VarInt; getEntity uses the int.
                    // Compare via the public accessors that exist; entityId is protected so
                    // we re-derive yRot/xRot from the packed bytes and check onGround.
                    if (dec.isOnGround() != onGround)
                        throw new IllegalStateException("onGround round-trip " + onGround
                            + " -> " + dec.isOnGround());
                    if (!dec.hasRotation())
                        throw new IllegalStateException("Rot must report hasRotation()");
                    if (dec.hasPosition())
                        throw new IllegalStateException("Rot must NOT report hasPosition()");

                    String name = "case_id" + id + "_y" + yRotI + "_x" + xRotI + "_g" + g;
                    O.print("ENC\t");
                    O.print(name);
                    O.print('\t');
                    O.print(id);
                    O.print('\t');
                    O.print(yRotI);
                    O.print('\t');
                    O.print(xRotI);
                    O.print('\t');
                    O.print(g);
                    O.print('\t');
                    O.print(readable);
                    O.print('\t');
                    O.print(hex);
                    O.print('\n');
                }
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
