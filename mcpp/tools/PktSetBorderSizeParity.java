// Ground truth for net.minecraft.network.protocol.game.ClientboundSetBorderSizePacket.
//
// Verified against the REAL 26.1.2 source. The STREAM_CODEC is built from
// Packet.codec(ClientboundSetBorderSizePacket::write, ClientboundSetBorderSizePacket::new),
// so the wire format is exactly ClientboundSetBorderSizePacket.write(FriendlyByteBuf):
//
//   output.writeDouble(this.size);   // FLOAT64, big-endian (8 bytes)
//
// and the private FriendlyByteBuf ctor reads it in exactly that order:
//   this.size = input.readDouble();
//
// The single field is a primitive double -- no registry/ItemStack/Component/Holder/NBT --
// so the C++ PacketBuffer (FriendlyByteBuf) port reproduces the bytes exactly.
//
// We construct each packet by reflectively invoking the private
// ClientboundSetBorderSizePacket(FriendlyByteBuf) constructor on a buffer that already
// holds our chosen field bytes (this is the packet's own READ path), then encode through
// the REAL STREAM_CODEC and dump the wire bytes. We also round-trip decode through the
// SAME codec as a sanity check.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <sizeBits-016x> <readableBytes-dec> <hexBytes>
// where <sizeBits> is Double.doubleToRawLongBits of the input double (lowercase hex) so
// the C++ side reconstructs the exact same double; <hexBytes> is the full packet payload,
// lowercase hex.
import io.netty.buffer.Unpooled;

import java.lang.reflect.Constructor;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSetBorderSizePacket;
import net.minecraft.server.Bootstrap;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktSetBorderSizeParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // double battery: zero/sign, identity, fractional, world-border physical sizes,
        // extrema, subnormal, irrational, typical lerp targets.
        double[] sizes = {
            0.0, -0.0, 1.0, -1.0,
            0.5, -0.5, 0.1, 3.141592653589793,
            60000000.0,                          // WorldBorder default size
            59999968.0,                          // 2 * 29999984 (absolute coordinate span)
            29999984.0, -29999984.0,
            1.7976931348623157E308,              // Double.MAX_VALUE
            -1.7976931348623157E308,             // -Double.MAX_VALUE
            4.9E-324,                            // smallest positive subnormal
            2.2250738585072014E-308,             // smallest positive normal
            123456.789, -987654.321, 2.5E7, 1000.0
        };

        int caseNo = 0;

        // (A) zero baseline.
        emit("base", 0.0);

        // (B) sweep every double in the battery into the single size field.
        for (int i = 0; i < sizes.length; i++) {
            emit("s" + (caseNo++), sizes[i]);
        }
    }

    // Build the packet via its private FriendlyByteBuf ctor (its own READ path) from a
    // buffer we populate with the chosen field bytes, then encode through the REAL
    // STREAM_CODEC and dump the wire bytes.
    static void emit(String name, double size) throws Exception {
        // Populate a FriendlyByteBuf in the exact codec READ order.
        FriendlyByteBuf in = new FriendlyByteBuf(Unpooled.buffer());
        in.writeDouble(size);

        Constructor<ClientboundSetBorderSizePacket> ctor =
            ClientboundSetBorderSizePacket.class.getDeclaredConstructor(FriendlyByteBuf.class);
        ctor.setAccessible(true);
        ClientboundSetBorderSizePacket pkt = ctor.newInstance(in);

        // Encode through the REAL STREAM_CODEC.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundSetBorderSizePacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): the field must match bit-exact.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundSetBorderSizePacket back = ClientboundSetBorderSizePacket.STREAM_CODEC.decode(rb);
        if (Double.doubleToRawLongBits(back.getSize()) != Double.doubleToRawLongBits(size)) {
            throw new IllegalStateException("round-trip field mismatch for " + name);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(size)));
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
