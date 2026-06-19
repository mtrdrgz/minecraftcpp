// Ground truth for net.minecraft.network.protocol.game.ClientboundSetPassengersPacket.
//
// The packet body is exactly (ClientboundSetPassengersPacket lines 27-35 in 26.1.2/src):
//
//     private ClientboundSetPassengersPacket(final FriendlyByteBuf input) {
//        this.vehicle = input.readVarInt();
//        this.passengers = input.readVarIntArray();
//     }
//     private void write(final FriendlyByteBuf output) {
//        output.writeVarInt(this.vehicle);
//        output.writeVarIntArray(this.passengers);
//     }
//
// FriendlyByteBuf.writeVarIntArray(int[] ints) (FriendlyByteBuf lines 309-317):
//     writeVarInt(ints.length); for (int i : ints) writeVarInt(i);
// readVarIntArray() (lines 319-335): int size = readVarInt(); loop size * readVarInt().
//
// So the full wire body is:
//     VarInt vehicle | VarInt count | count * VarInt passengerId
//
// STREAM_CODEC = Packet.codec(write, ctor) -> StreamCodec.ofMember: body only, no
// packet-id prefix. vehicle and the passenger ids are raw entity ids (plain ints):
// writeVarInt is plain LEB128 (NOT zig-zag), so negative ids encode to five bytes.
//
// The packet's only ctor that sets these from raw ints is the private FriendlyByteBuf
// decode ctor (the public ctor takes a live Entity); both fields are private. We
// therefore build each physical case by encoding a seed buffer
//     VarInt(vehicle) + VarInt(count) + count * VarInt(passengerId)
// and decoding it through the REAL STREAM_CODEC.decode, then re-encode through the
// REAL STREAM_CODEC.encode and dump the exact wire bytes. We assert the decode->encode
// round-trip is stable, and read vehicle + passengers[] back via reflection to
// sanity-check they equal the inputs.
//
// Row format (tab separated):
//   ENC <name> <vehicle-dec> <count-dec> <id0,id1,...|-> <readableBytes> <hexBytes>
//     encode the real packet -> body bytes; the id list column is a comma-joined
//     decimal list of passenger ids, or "-" when empty.
//
// The C++ pkt_set_passengers_parity rebuilds the body via PacketBuffer.writeVarInt
// (vehicle, then count, then each passenger id) and must match <hexBytes> byte-for-byte
// AND on readableBytes; it then decodes the expected bytes back and must round-trip the
// vehicle, count and every passenger id.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Field;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetPassengersPacket;

public class PktSetPassengersParity {
    static final java.io.PrintStream O = System.out;

    // VarInt LEB128 byte-boundary values + sign + extremes. Entity ids are plain ints
    // passed verbatim through writeVarInt, so negatives are legal and encode to 5 bytes.
    static final int[] V = {
        0, 1, 2, 127, 128, 129, 255, 256, 16383, 16384, 16385,
        2097151, 2097152, 2097153, 268435455, 268435456, 268435457,
        -1, -2, -128, -2097152, Integer.MAX_VALUE, Integer.MIN_VALUE
    };

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundSetPassengersPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetPassengersPacket>)
                ClientboundSetPassengersPacket.STREAM_CODEC;

        // vehicle + passengers are private; read them back for round-trip sanity asserts.
        Field vehicleField = ClientboundSetPassengersPacket.class.getDeclaredField("vehicle");
        vehicleField.setAccessible(true);
        Field passengersField = ClientboundSetPassengersPacket.class.getDeclaredField("passengers");
        passengersField.setAccessible(true);

        // --- empty passenger list (count = 0), vehicle pinned across VarInt boundaries ---
        for (int veh : V) {
            emit("empty_veh_" + veh, veh, new int[] {}, CODEC, vehicleField, passengersField);
        }

        // --- single passenger: each boundary value alone, with a typical small vehicle ---
        for (int p : V) {
            emit("one_" + p, 1, new int[] { p }, CODEC, vehicleField, passengersField);
        }

        // --- vehicle ALSO crossing VarInt boundaries (paired with a small passenger) ---
        for (int veh : V) {
            emit("vehbnd_" + veh, veh, new int[] { 7 }, CODEC, vehicleField, passengersField);
        }

        // --- typical realistic passenger runs (small non-negative entity ids) ---
        emit("run_small", 1, new int[] { 2, 3, 4, 5 }, CODEC, vehicleField, passengersField);
        emit("run_ids", 42, new int[] { 100, 1000, 32767, 65536 }, CODEC, vehicleField, passengersField);
        emit("run_seq", 100, new int[] { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 },
             CODEC, vehicleField, passengersField);

        // --- mixed boundary + sign + extremes in one passenger list (count multi-byte too) ---
        emit("mixed_all", 16384, V, CODEC, vehicleField, passengersField);

        // --- passenger-list SIZE itself crosses the count VarInt byte boundary: 127 -> 1
        //     byte, 128 -> 2 byte count prefix. ---
        int[] c127 = new int[127];
        for (int i = 0; i < c127.length; i++) c127[i] = i;
        emit("count127", 5, c127, CODEC, vehicleField, passengersField);

        int[] c128 = new int[128];
        for (int i = 0; i < c128.length; i++) c128[i] = i;
        emit("count128", 5, c128, CODEC, vehicleField, passengersField);

        int[] c200 = new int[200];
        for (int i = 0; i < c200.length; i++) c200[i] = i * 7 - 13; // spread of signs/sizes
        emit("count200", -1, c200, CODEC, vehicleField, passengersField);
    }

    static void emit(String name, int vehicle, int[] passengers,
                     StreamCodec<FriendlyByteBuf, ClientboundSetPassengersPacket> codec,
                     Field vehicleField, Field passengersField) throws Exception {
        // Build the REAL packet by decoding the seed body through STREAM_CODEC.decode.
        // Seed = VarInt(vehicle) + VarInt(count) + count * VarInt(passengerId), exactly
        // the order the private FriendlyByteBuf ctor reads (readVarInt + readVarIntArray).
        FriendlyByteBuf seed = new FriendlyByteBuf(Unpooled.buffer());
        seed.writeVarInt(vehicle);
        seed.writeVarInt(passengers.length);
        for (int p : passengers) seed.writeVarInt(p);
        ClientboundSetPassengersPacket pkt = codec.decode(seed);

        // Sanity: the packet's private fields equal the inputs.
        int recVehicle = vehicleField.getInt(pkt);
        if (recVehicle != vehicle)
            throw new IllegalStateException(name + ": vehicle " + recVehicle + " != " + vehicle);
        int[] recPass = (int[]) passengersField.get(pkt);
        if (recPass.length != passengers.length)
            throw new IllegalStateException(name + ": passenger count " + recPass.length
                + " != " + passengers.length);
        for (int i = 0; i < passengers.length; i++)
            if (recPass[i] != passengers[i])
                throw new IllegalStateException(name + ": passenger[" + i + "] "
                    + recPass[i] + " != " + passengers[i]);

        // ENC: encode through the real codec, dump readableBytes + body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Round-trip decode through the SAME codec (sanity on the encoded bytes).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundSetPassengersPacket dec = codec.decode(rbuf);
        if (vehicleField.getInt(dec) != vehicle)
            throw new IllegalStateException(name + ": round-trip vehicle mismatch");
        int[] backPass = (int[]) passengersField.get(dec);
        if (backPass.length != passengers.length)
            throw new IllegalStateException(name + ": round-trip count mismatch");
        for (int i = 0; i < passengers.length; i++)
            if (backPass[i] != passengers[i])
                throw new IllegalStateException(name + ": round-trip passenger[" + i + "] mismatch");

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(vehicle);
        O.print('\t');
        O.print(passengers.length);
        O.print('\t');
        O.print(joinIds(passengers));
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
    }

    static String joinIds(int[] ids) {
        if (ids.length == 0) return "-";
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < ids.length; i++) {
            if (i > 0) sb.append(',');
            sb.append(ids[i]);
        }
        return sb.toString();
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
