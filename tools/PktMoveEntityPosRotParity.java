// Ground truth for net.minecraft.network.protocol.game.ClientboundMoveEntityPacket.PosRot.
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src
// net/minecraft/network/protocol/game/ClientboundMoveEntityPacket.java):
//
//   ClientboundMoveEntityPacket.PosRot.STREAM_CODEC = Packet.codec(PosRot::write, PosRot::read)
//   private void write(FriendlyByteBuf output) {
//       output.writeVarInt(this.entityId);   // VarInt entity id
//       output.writeShort(this.xa);          // BE 2-byte signed short
//       output.writeShort(this.ya);          // BE 2-byte signed short
//       output.writeShort(this.za);          // BE 2-byte signed short
//       output.writeByte(this.yRot);         // 1 signed byte
//       output.writeByte(this.xRot);         // 1 signed byte
//       output.writeBoolean(this.onGround);  // 1 byte 0/1
//   }
//   read() pulls them back in the same order via readVarInt/readShort*3/readByte*2/readBoolean.
//
// Public ctor:
//   PosRot(int id, short xa, short ya, short za, byte yRot, byte xRot, boolean onGround)
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <id-dec> <xa-dec> <ya-dec> <za-dec> <yRot-dec> <xRot-dec> <onGround-0|1>
//       <readableBytes-dec> <hexBytes>
// All numeric fields decimal (shorts/bytes are SIGNED values as decoded by the Java codec);
// onGround as 0/1. <hexBytes> is the full packet payload, lowercase hex.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundMoveEntityPacket;
import net.minecraft.server.Bootstrap;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktMoveEntityPosRotParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Entity ids exercise VarInt 1->5 byte boundaries + negatives + extrema.
        int[] ids = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 42, 12345, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // short delta values: zero, sign, byte/short boundaries, extrema.
        short[] shorts = {
            0, 1, -1, 127, 128, -128, -129, 255, 256, 32767, -32768, 4096, -4096, 100, -100
        };

        // byte rotation values: zero, sign, extrema, mid.
        byte[] bytes = { 0, 1, -1, 127, -128, 64, -64, 42, -42, 100, -100 };

        boolean[] grounds = { false, true };

        int caseNo = 0;

        // 1) Sweep ids with a fixed nontrivial body.
        for (int id : ids) {
            emit("id" + (caseNo++), id, (short)321, (short)-654, (short)987, (byte)45, (byte)-90, true);
        }

        // 2) Sweep the three short deltas independently (others fixed).
        for (short s : shorts) {
            emit("xa" + (caseNo++), 7, s, (short)0, (short)0, (byte)0, (byte)0, false);
            emit("ya" + (caseNo++), 7, (short)0, s, (short)0, (byte)0, (byte)0, false);
            emit("za" + (caseNo++), 7, (short)0, (short)0, s, (byte)0, (byte)0, false);
        }

        // 3) Sweep yRot / xRot independently.
        for (byte b : bytes) {
            emit("yr" + (caseNo++), 13, (short)10, (short)20, (short)30, b, (byte)0, true);
            emit("xr" + (caseNo++), 13, (short)10, (short)20, (short)30, (byte)0, b, false);
        }

        // 4) onGround both states with a busy body.
        for (boolean g : grounds) {
            emit("og" + (caseNo++), 256, (short)-1, (short)1, (short)-32768, (byte)127, (byte)-128, g);
        }

        // 5) A couple of full-extrema combos.
        emit("max" + (caseNo++), Integer.MAX_VALUE, (short)32767, (short)32767, (short)32767, (byte)127, (byte)127, true);
        emit("min" + (caseNo++), Integer.MIN_VALUE, (short)-32768, (short)-32768, (short)-32768, (byte)-128, (byte)-128, false);
        emit("zero" + (caseNo++), 0, (short)0, (short)0, (short)0, (byte)0, (byte)0, false);
    }

    static void emit(String name, int id, short xa, short ya, short za, byte yRot, byte xRot, boolean onGround) {
        ClientboundMoveEntityPacket.PosRot pkt =
            new ClientboundMoveEntityPacket.PosRot(id, xa, ya, za, yRot, xRot, onGround);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundMoveEntityPacket.PosRot.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): all fields must match exactly.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundMoveEntityPacket back = ClientboundMoveEntityPacket.PosRot.STREAM_CODEC.decode(rb);
        if (back.getXa() != xa || back.getYa() != ya || back.getZa() != za
                || !back.hasRotation() || !back.hasPosition() || back.isOnGround() != onGround) {
            throw new IllegalStateException("round-trip field mismatch for " + name);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(id);
        O.print('\t');
        O.print((int)xa);
        O.print('\t');
        O.print((int)ya);
        O.print('\t');
        O.print((int)za);
        O.print('\t');
        O.print((int)yRot);
        O.print('\t');
        O.print((int)xRot);
        O.print('\t');
        O.print(onGround ? 1 : 0);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
