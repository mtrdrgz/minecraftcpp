// Ground truth for net.minecraft.network.protocol.game.ClientboundSetBorderLerpSizePacket.
//
// Verified against the REAL 26.1.2 source. The STREAM_CODEC is built from
// Packet.codec(ClientboundSetBorderLerpSizePacket::write, ClientboundSetBorderLerpSizePacket::new),
// so the wire format is exactly ClientboundSetBorderLerpSizePacket.write(FriendlyByteBuf):
//
//   output.writeDouble(oldSize);    // FLOAT64, big-endian (8 bytes)
//   output.writeDouble(newSize);    // FLOAT64, big-endian (8 bytes)
//   output.writeVarLong(lerpTime);  // VAR_LONG (LEB128)
//
// and the private FriendlyByteBuf ctor reads them in exactly that order:
//   readDouble, readDouble, readVarLong.
//
// All fields are primitives -- no registry/ItemStack/Component/Holder/NBT -- so the
// PacketBuffer (FriendlyByteBuf) port reproduces the bytes exactly.
//
// We construct each packet by reflectively invoking the private
// ClientboundSetBorderLerpSizePacket(FriendlyByteBuf) constructor on a buffer that
// already holds our chosen field bytes (this is the packet's own READ path), then
// encode through the REAL STREAM_CODEC and dump the wire bytes. We also round-trip
// decode through the SAME codec as a sanity check.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <oldSizeBits-016x> <newSizeBits-016x> <lerpTime-dec> <readableBytes-dec> <hexBytes>
// where the two *Bits cols are Double.doubleToRawLongBits of the input doubles
// (lowercase hex) so the C++ side reconstructs the exact same doubles; lerpTime is a
// decimal long (VarLong input). <hexBytes> is the full packet payload, lowercase hex.
import io.netty.buffer.Unpooled;

import java.lang.reflect.Constructor;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSetBorderLerpSizePacket;
import net.minecraft.server.Bootstrap;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktSetBorderLerpSizeParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // double battery: 0.0/-0.0/1.0/-1.0, small, large, typical world coords, extrema, subnormal.
        double[] doubles = {
            0.0, -0.0, 1.0, -1.0,
            0.5, -0.5, 0.1, 3.141592653589793,
            29999984.0, -29999984.0,            // world border absolute coordinate range
            60000000.0, 1.7976931348623157E308, // big / Double.MAX_VALUE
            4.9E-324,                           // smallest subnormal
            123456.789, -987654.321, 2.5E7
        };

        // lerpTime: VarLong LEB128 boundaries + negatives + extrema.
        long[] lerps = {
            0L, 1L, 127L, 128L, 16383L, 16384L, 2097151L, 2097152L,
            268435455L, 268435456L, 34359738367L, 34359738368L,
            -1L, 1000L, 9223372036854775807L, -9223372036854775808L
        };

        int caseNo = 0;

        // (A) zero/identity baseline.
        emit("base", 0.0, 0.0, 0L);

        // (B) sweep doubles into the two double fields (rotate through the battery),
        //     holding lerpTime at a typical physical value.
        for (int i = 0; i < doubles.length; i++) {
            double os = doubles[i];
            double ns = doubles[(i + 1) % doubles.length];
            emit("d" + (caseNo++), os, ns, lerps[i % lerps.length]);
        }

        // (C) sweep lerpTime (VarLong) across every boundary, doubles at typical coords.
        for (int i = 0; i < lerps.length; i++) {
            emit("l" + (caseNo++), 1000.0, 2000.0, lerps[i]);
        }

        // (D) a couple of fully-mixed extreme rows.
        emit("max", 1.7976931348623157E308, 1.7976931348623157E308, 9223372036854775807L);
        emit("min", -1.7976931348623157E308, -1.7976931348623157E308, -9223372036854775808L);
    }

    // Build the packet via its private FriendlyByteBuf ctor (its own READ path) from a
    // buffer we populate with the chosen field bytes, then encode through the REAL
    // STREAM_CODEC and dump the wire bytes.
    static void emit(String name, double oldSize, double newSize, long lerpTime)
            throws Exception {
        // Populate a FriendlyByteBuf in the exact codec READ order.
        FriendlyByteBuf in = new FriendlyByteBuf(Unpooled.buffer());
        in.writeDouble(oldSize);
        in.writeDouble(newSize);
        in.writeVarLong(lerpTime);

        Constructor<ClientboundSetBorderLerpSizePacket> ctor =
            ClientboundSetBorderLerpSizePacket.class.getDeclaredConstructor(FriendlyByteBuf.class);
        ctor.setAccessible(true);
        ClientboundSetBorderLerpSizePacket pkt = ctor.newInstance(in);

        // Encode through the REAL STREAM_CODEC.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundSetBorderLerpSizePacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): every field must match.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundSetBorderLerpSizePacket back = ClientboundSetBorderLerpSizePacket.STREAM_CODEC.decode(rb);
        if (Double.doubleToRawLongBits(back.getOldSize()) != Double.doubleToRawLongBits(oldSize)
            || Double.doubleToRawLongBits(back.getNewSize()) != Double.doubleToRawLongBits(newSize)
            || back.getLerpTime() != lerpTime) {
            throw new IllegalStateException("round-trip field mismatch for " + name);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(oldSize)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(newSize)));
        O.print('\t');
        O.print(lerpTime);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
