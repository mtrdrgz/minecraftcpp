// Ground truth for net.minecraft.network.protocol.game.ClientboundMoveEntityPacket.Pos.
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src ClientboundMoveEntityPacket.java:
// the nested static class Pos, its private write(FriendlyByteBuf) and read(FriendlyByteBuf),
// reached through Pos.STREAM_CODEC = Packet.codec(Pos::write, Pos::read)):
//
//   Pos.write(output):
//       output.writeVarInt(this.entityId);   // VarInt(entityId)
//       output.writeShort(this.xa);          // BE 2-byte short
//       output.writeShort(this.ya);          // BE 2-byte short
//       output.writeShort(this.za);          // BE 2-byte short
//       output.writeBoolean(this.onGround);  // 1 byte 0/1
//   Pos.read(input):
//       int   entityId = input.readVarInt();
//       short xa       = input.readShort();
//       short ya       = input.readShort();
//       short za       = input.readShort();
//       boolean onGround = input.readBoolean();
//       return new Pos(entityId, xa, ya, za, onGround);
//
// The public ctor is  Pos(int id, short xa, short ya, short za, boolean onGround).
// Packet.codec -> StreamCodec.ofMember (body only; NO packet-id / length prefix).
//
// All fields are plain primitives PacketBuffer supports (writeVarInt/writeShort/writeBool),
// so no registry/ItemStack/Component/Holder/NBT is involved.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <id-dec> <xa-dec> <ya-dec> <za-dec> <onGround-0/1> <readableBytes-dec> <hexBytes>
// id/xa/ya/za are SIGNED decimals (xa/ya/za are Java shorts -32768..32767). <hexBytes> is the
// full packet payload, lowercase hex. The C++ side replays writeVarInt/writeShort/writeBool
// through PacketBuffer and must match <hexBytes> + <readableBytes> byte-for-byte, then decodes
// the bytes back and requires every field round-trips.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundMoveEntityPacket;
import net.minecraft.server.Bootstrap;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktMoveEntityPosParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Entity ids exercise VarInt 1->5 byte boundaries + negatives + extrema.
        int[] ids = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 42, 12345, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // Shorts: zero, sign, extrema, and small/typical delta values. (Java short range.)
        short[] shorts = {
            (short) 0, (short) 1, (short) -1, (short) 127, (short) -128,
            (short) 255, (short) 256, (short) 4096, (short) -4096,
            (short) 32767, (short) -32768, (short) 12345, (short) -12345
        };

        boolean[] grounds = { false, true };

        int caseNo = 0;
        // Diagonal-ish coverage: pair each id with a rotating (xa,ya,za) triple and both onGround.
        for (int id : ids) {
            for (int si = 0; si < shorts.length; si++) {
                short xa = shorts[si];
                short ya = shorts[(si + 1) % shorts.length];
                short za = shorts[(si + 2) % shorts.length];
                for (boolean og : grounds) {
                    emit("c" + (caseNo++), id, xa, ya, za, og);
                }
            }
        }
        // A few explicit all-extrema corners.
        emit("c" + (caseNo++), Integer.MAX_VALUE, (short) 32767, (short) 32767, (short) 32767, true);
        emit("c" + (caseNo++), Integer.MIN_VALUE, (short) -32768, (short) -32768, (short) -32768, false);
        emit("c" + (caseNo++), 0, (short) 0, (short) 0, (short) 0, false);
        emit("c" + (caseNo++), 0, (short) 0, (short) 0, (short) 0, true);
    }

    static void emit(String name, int id, short xa, short ya, short za, boolean onGround) throws Exception {
        ClientboundMoveEntityPacket.Pos pkt =
            new ClientboundMoveEntityPacket.Pos(id, xa, ya, za, onGround);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundMoveEntityPacket.Pos.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): all fields must match exactly.
        // entityId has no public getter (protected field) -> read it via reflection.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundMoveEntityPacket.Pos back = ClientboundMoveEntityPacket.Pos.STREAM_CODEC.decode(rb);
        if (entityIdOf(back) != id || back.getXa() != xa || back.getYa() != ya
            || back.getZa() != za || back.isOnGround() != onGround) {
            throw new IllegalStateException("round-trip mismatch for " + name);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(id);
        O.print('\t');
        O.print((int) xa);
        O.print('\t');
        O.print((int) ya);
        O.print('\t');
        O.print((int) za);
        O.print('\t');
        O.print(onGround ? 1 : 0);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }

    // The 'entityId' field is declared 'protected final' on the abstract base with no public
    // getter; read it reflectively (setAccessible) for the round-trip sanity check.
    static int entityIdOf(ClientboundMoveEntityPacket p) throws Exception {
        java.lang.reflect.Field f = ClientboundMoveEntityPacket.class.getDeclaredField("entityId");
        f.setAccessible(true);
        return f.getInt(p);
    }
}
