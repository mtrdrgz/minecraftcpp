// Ground truth for net.minecraft.network.protocol.game.ClientboundInitializeBorderPacket.
//
// Verified against the REAL 26.1.2 source. The STREAM_CODEC is built from
// Packet.codec(ClientboundInitializeBorderPacket::write, ClientboundInitializeBorderPacket::new),
// so the wire format is exactly ClientboundInitializeBorderPacket.write(FriendlyByteBuf):
//
//   output.writeDouble(newCenterX);          // FLOAT64, big-endian (8 bytes)
//   output.writeDouble(newCenterZ);          // FLOAT64, big-endian (8 bytes)
//   output.writeDouble(oldSize);             // FLOAT64, big-endian (8 bytes)
//   output.writeDouble(newSize);             // FLOAT64, big-endian (8 bytes)
//   output.writeVarLong(lerpTime);           // VAR_LONG (LEB128)
//   output.writeVarInt(newAbsoluteMaxSize);  // VAR_INT  (LEB128)
//   output.writeVarInt(warningBlocks);       // VAR_INT  (LEB128)
//   output.writeVarInt(warningTime);         // VAR_INT  (LEB128)
//
// and the private FriendlyByteBuf ctor reads them in exactly that order:
//   readDouble, readDouble, readDouble, readDouble, readVarLong, readVarInt, readVarInt, readVarInt.
//
// All fields are primitives — no registry/ItemStack/Component/Holder/NBT — so the
// PacketBuffer (FriendlyByteBuf) port reproduces the bytes exactly.
//
// We construct each packet by reflectively invoking the private
// ClientboundInitializeBorderPacket(FriendlyByteBuf) constructor on a buffer that
// already holds our chosen field bytes (this is the packet's own READ path), then
// encode through the REAL STREAM_CODEC and dump the wire bytes. We also round-trip
// decode through the SAME codec as a sanity check.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <centerXBits-016x> <centerZBits-016x> <oldSizeBits-016x> <newSizeBits-016x>
//       <lerpTime-dec> <newAbsoluteMaxSize-dec> <warningBlocks-dec> <warningTime-dec>
//       <readableBytes-dec> <hexBytes>
// where the four *Bits cols are Double.doubleToRawLongBits of the input doubles
// (lowercase hex) so the C++ side reconstructs the exact same doubles; lerpTime is a
// decimal long (VarLong input); the three int cols are decimal (VarInt inputs).
// <hexBytes> is the full packet payload, lowercase hex.
import io.netty.buffer.Unpooled;

import java.lang.reflect.Constructor;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundInitializeBorderPacket;
import net.minecraft.server.Bootstrap;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktInitializeBorderParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // double battery: 0.0/-0.0/1.0/-1.0, small, large, typical world coords, extrema, subnormal.
        double[] doublesA = {
            0.0, -0.0, 1.0, -1.0,
            0.5, -0.5, 0.1, 3.141592653589793,
            29999984.0, -29999984.0,           // world border absolute coordinate range
            60000000.0, 1.7976931348623157E308, // big / Double.MAX_VALUE
            4.9E-324,                            // smallest subnormal
            123456.789, -987654.321, 2.5E7
        };

        // lerpTime: VarLong LEB128 boundaries + negatives + extrema.
        long[] lerps = {
            0L, 1L, 127L, 128L, 16383L, 16384L, 2097151L, 2097152L,
            268435455L, 268435456L, 34359738367L, 34359738368L,
            -1L, 1000L, 9223372036854775807L, -9223372036854775808L
        };

        // VarInt battery: LEB128 1->5 byte boundaries + negatives + extrema + physical defaults.
        int[] ints = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 5, 15, 29999984, 2147483647, -2147483648
        };

        int caseNo = 0;

        // (A) zero/identity baseline.
        emit("base", 0.0, 0.0, 0.0, 0.0, 0L, 0, 0, 0);

        // (B) sweep doubles into the four double fields (rotate through the battery),
        //     holding the integral fields at typical physical values.
        for (int i = 0; i < doublesA.length; i++) {
            double cx = doublesA[i];
            double cz = doublesA[(i + 1) % doublesA.length];
            double os = doublesA[(i + 2) % doublesA.length];
            double ns = doublesA[(i + 3) % doublesA.length];
            emit("d" + (caseNo++), cx, cz, os, ns,
                 lerps[i % lerps.length],
                 ints[i % ints.length],
                 ints[(i + 1) % ints.length],
                 ints[(i + 2) % ints.length]);
        }

        // (C) sweep lerpTime (VarLong) across every boundary, doubles at typical coords.
        for (int i = 0; i < lerps.length; i++) {
            emit("l" + (caseNo++), 12.5, -12.5, 1000.0, 2000.0,
                 lerps[i], 29999984, 5, 15);
        }

        // (D) sweep each VarInt field independently across the boundary battery.
        for (int i = 0; i < ints.length; i++) {
            emit("amax" + (caseNo++), 0.0, 0.0, 100.0, 100.0, 0L, ints[i], 5, 15);
        }
        for (int i = 0; i < ints.length; i++) {
            emit("wb" + (caseNo++), 0.0, 0.0, 100.0, 100.0, 0L, 29999984, ints[i], 15);
        }
        for (int i = 0; i < ints.length; i++) {
            emit("wt" + (caseNo++), 0.0, 0.0, 100.0, 100.0, 0L, 29999984, 5, ints[i]);
        }

        // (E) a couple of fully-mixed extreme rows.
        emit("max", 1.7976931348623157E308, 1.7976931348623157E308,
             1.7976931348623157E308, 1.7976931348623157E308,
             9223372036854775807L, 2147483647, 2147483647, 2147483647);
        emit("min", -1.7976931348623157E308, -1.7976931348623157E308,
             -1.7976931348623157E308, -1.7976931348623157E308,
             -9223372036854775808L, -2147483648, -2147483648, -2147483648);
    }

    // Build the packet via its private FriendlyByteBuf ctor (its own READ path) from a
    // buffer we populate with the chosen field bytes, then encode through the REAL
    // STREAM_CODEC and dump the wire bytes.
    static void emit(String name, double centerX, double centerZ, double oldSize, double newSize,
                     long lerpTime, int newAbsoluteMaxSize, int warningBlocks, int warningTime)
            throws Exception {
        // Populate a FriendlyByteBuf in the exact codec READ order.
        FriendlyByteBuf in = new FriendlyByteBuf(Unpooled.buffer());
        in.writeDouble(centerX);
        in.writeDouble(centerZ);
        in.writeDouble(oldSize);
        in.writeDouble(newSize);
        in.writeVarLong(lerpTime);
        in.writeVarInt(newAbsoluteMaxSize);
        in.writeVarInt(warningBlocks);
        in.writeVarInt(warningTime);

        Constructor<ClientboundInitializeBorderPacket> ctor =
            ClientboundInitializeBorderPacket.class.getDeclaredConstructor(FriendlyByteBuf.class);
        ctor.setAccessible(true);
        ClientboundInitializeBorderPacket pkt = ctor.newInstance(in);

        // Encode through the REAL STREAM_CODEC.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundInitializeBorderPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): every field must match.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundInitializeBorderPacket back = ClientboundInitializeBorderPacket.STREAM_CODEC.decode(rb);
        if (Double.doubleToRawLongBits(back.getNewCenterX()) != Double.doubleToRawLongBits(centerX)
            || Double.doubleToRawLongBits(back.getNewCenterZ()) != Double.doubleToRawLongBits(centerZ)
            || Double.doubleToRawLongBits(back.getOldSize())    != Double.doubleToRawLongBits(oldSize)
            || Double.doubleToRawLongBits(back.getNewSize())    != Double.doubleToRawLongBits(newSize)
            || back.getLerpTime()           != lerpTime
            || back.getNewAbsoluteMaxSize() != newAbsoluteMaxSize
            || back.getWarningBlocks()      != warningBlocks
            || back.getWarningTime()        != warningTime) {
            throw new IllegalStateException("round-trip field mismatch for " + name);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(centerX)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(centerZ)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(oldSize)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(newSize)));
        O.print('\t');
        O.print(lerpTime);
        O.print('\t');
        O.print(newAbsoluteMaxSize);
        O.print('\t');
        O.print(warningBlocks);
        O.print('\t');
        O.print(warningTime);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
