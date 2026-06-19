// Ground truth for net.minecraft.network.protocol.game.ClientboundAddEntityPacket (26.1.2).
//
// The packet's STREAM_CODEC is, VERBATIM (ClientboundAddEntityPacket.java:18-20, 109-121):
//   STREAM_CODEC = Packet.codec(ClientboundAddEntityPacket::write, ClientboundAddEntityPacket::new)
//   private void write(RegistryFriendlyByteBuf output) {
//      output.writeVarInt(this.id);                                  // VarInt id
//      output.writeUUID(this.uuid);                                  // 2 longs BE (MSB, LSB)
//      ByteBufCodecs.registry(Registries.ENTITY_TYPE).encode(output, this.type);
//                                                                    // -> VarInt(getIdOrThrow(type))  PLAIN id (NOT holder, no +1)
//      output.writeDouble(this.x);                                   // double BE
//      output.writeDouble(this.y);                                   // double BE
//      output.writeDouble(this.z);                                   // double BE
//      Vec3.LP_STREAM_CODEC.encode(output, this.movement);          // net.minecraft.network.LpVec3.write (quantized velocity)
//      output.writeByte(this.xRot);                                  // byte  = Mth.packDegrees(xRotDeg)
//      output.writeByte(this.yRot);                                  // byte  = Mth.packDegrees(yRotDeg)
//      output.writeByte(this.yHeadRot);                              // byte  = Mth.packDegrees((float)yHeadRotDeg)
//      output.writeVarInt(this.data);                                // VarInt data
//   }
//
// The angle bytes come from the CONSTRUCTOR (ClientboundAddEntityPacket.java:88-90):
//   this.xRot     = Mth.packDegrees(xRot);
//   this.yRot     = Mth.packDegrees(yRot);
//   this.yHeadRot = Mth.packDegrees((float)yHeadRot);
//   Mth.packDegrees(float angle) = (byte)Mth.floor(angle * 256.0F / 360.0F)
//                                = (byte)(int)Math.floor(angle * 256.0F / 360.0F)   (Mth.java:181-183, 61-63)
//
// ByteBufCodecs.registry(ResourceKey) -> VarInt.write(out, registry.getIdOrThrow(value))  (ByteBufCodecs.java:560-582)
//   == PLAIN VarInt of the EntityType registry id. NO holder()/+1.  (holder(...) would VarInt(id+1).)
//
// Vec3.LP_STREAM_CODEC = StreamCodec.of(LpVec3::write, LpVec3::read)  (Vec3.java:36; net.minecraft.network.LpVec3).
//   LpVec3.write(out, v): x=sanitize(v.x),y=...,z=...; cl=absMax(x,absMax(y,z));
//     if cl < 3.051944088384301E-5: out.writeByte(0)
//     else: scale=ceilLong(cl); partial=(scale&3)!=scale; markers=partial?(scale&3|4):scale;
//           buffer = markers | pack(x/scale)<<3 | pack(y/scale)<<18 | pack(z/scale)<<33;
//           out.writeByte((byte)buffer); out.writeByte((byte)(buffer>>8)); out.writeInt((int)(buffer>>16));
//           if partial: VarInt.write(out, (int)(scale>>2));
//     pack(v)=Math.round((v*0.5+0.5)*32766.0).  (net.minecraft.network.LpVec3 — certified by lp_vec3_parity)
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <id-dec> <uuidHi-016x> <uuidLo-016x> <typeName ns:path> <typeId-dec>
//       <xBits-016x> <yBits-016x> <zBits-016x>
//       <vxBits-016x> <vyBits-016x> <vzBits-016x>
//       <xRotBits-08x> <yRotBits-08x> <yHeadRotBits-08x>
//       <data-dec> <readableBytes-dec> <hexBytes>
// Position/velocity components are Double.doubleToRawLongBits of the INPUT doubles; the three
// rotations are Float.floatToRawIntBits of the INPUT float degrees (the C++ side replays
// Mth.packDegrees from these). typeName lets the C++ side resolve the EntityType id via
// NetworkRegistries; typeId is the expected resolved id (cross-check).
//
// The packet's STREAM_CODEC needs a RegistryFriendlyByteBuf (the registry(ENTITY_TYPE) codec
// reads input.registryAccess()), so we encode through a real RegistryAccess-backed buffer.
import io.netty.buffer.Unpooled;

import java.util.UUID;

import net.minecraft.SharedConstants;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.resources.Identifier;  // 26.1.2: ResourceLocation was renamed to Identifier
import net.minecraft.server.Bootstrap;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.phys.Vec3;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktAddEntityParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Real RegistryAccess (the registry(ENTITY_TYPE) codec needs input.registryAccess()).
        RegistryAccess registryAccess = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        int typeCount = BuiltInRegistries.ENTITY_TYPE.size();
        if (typeCount <= 0) throw new IllegalStateException("ENTITY_TYPE registry empty");

        // Spread of EntityTypes incl id 0 and a large id (resolved by registry id).
        int[] typeIds = {
            0,                  // first registered (smallest id)
            1,
            6,                  // arrow
            32,                 // creeper
            71,                 // item
            100,                // pig
            132,                // tnt
            150,                // zombie
            typeCount - 1,      // last registered (largest id)
        };

        // Entity-network-id battery: VarInt 1->5 byte boundaries + negatives + extrema.
        int[] ids = { 0, 1, 127, 128, 16383, 16384, 2097151, 268435455, -1, 42, Integer.MAX_VALUE, Integer.MIN_VALUE };

        // Positions (finite doubles incl signs + extremes used by entities).
        double[][] positions = {
            {0.0, 0.0, 0.0},
            {0.5, 64.0, -0.5},
            {-1234.5, 319.0625, 5678.25},
            {30000000.0, -64.0, -30000000.0},
            {1.5, -2.25, 3.75},
        };

        // Velocities — hit every LpVec3 branch (zero, sub-unit, scale>=4 partial, clamp).
        double[][] vels = {
            {0.0, 0.0, 0.0},                       // -> single 0 byte
            {0.1, -0.2, 0.3},                       // scale 1
            {0.5, -0.5, 0.25},                      // scale 1, near edge
            {1.0, 0.0, 0.0},                        // scale 1 axis
            {3.5, 0.0, 0.0},                        // scale 4 -> partial/continuation
            {7.0, -6.5, 3.2},                       // scale 7 -> partial varint
            {100.0, -50.0, 25.0},                   // larger varint scale
            {1.7179869183E10, -1.7179869183E10, 1.0E5}, // ABS_MAX (no clamp)
        };

        // Rotations in degrees (float). Mth.packDegrees maps these to signed bytes.
        float[][] rots = {
            {0.0f, 0.0f, 0.0f},
            {90.0f, 180.0f, 270.0f},
            {-90.0f, 45.0f, 359.5f},
            {12.34f, -56.78f, 123.45f},
            {360.0f, -360.0f, 720.0f},
        };

        // data values (VarInt; for most entities 0, but exercise the boundaries).
        int[] datas = { 0, 1, 127, 128, 16384, -1, Integer.MAX_VALUE };

        // Fixed UUIDs (deterministic, incl all-zero and a high-bit one).
        long[][] uuids = {
            {0L, 0L},
            {0x0123456789abcdefL, 0xfedcba9876543210L},
            {0x8000000000000000L, 0x00000000000000ffL},
            {-1L, -1L},
        };

        int caseNo = 0;
        // (A) Sweep entity TYPE ids (the registry-held field) at fixed everything else.
        for (int t : typeIds) {
            emit(registryAccess, "type" + (caseNo++), 7, uuids[0][0], uuids[0][1], t,
                 positions[1], vels[1], rots[1], 0);
        }
        // (B) Sweep network ids (VarInt boundaries).
        for (int id : ids) {
            emit(registryAccess, "id" + (caseNo++), id, uuids[1][0], uuids[1][1], 32,
                 positions[2], vels[2], rots[2], 0);
        }
        // (C) Sweep positions.
        for (double[] p : positions) {
            emit(registryAccess, "pos" + (caseNo++), 42, uuids[1][0], uuids[1][1], 100,
                 p, vels[3], rots[0], 0);
        }
        // (D) Sweep velocities (LpVec3 branches).
        for (double[] v : vels) {
            emit(registryAccess, "vel" + (caseNo++), 42, uuids[1][0], uuids[1][1], 6,
                 positions[1], v, rots[0], 0);
        }
        // (E) Sweep rotations.
        for (float[] r : rots) {
            emit(registryAccess, "rot" + (caseNo++), 42, uuids[1][0], uuids[1][1], 150,
                 positions[1], vels[1], r, 0);
        }
        // (F) Sweep data VarInt.
        for (int d : datas) {
            emit(registryAccess, "data" + (caseNo++), 42, uuids[1][0], uuids[1][1], 132,
                 positions[1], vels[1], rots[1], d);
        }
        // (G) Sweep UUIDs.
        for (long[] u : uuids) {
            emit(registryAccess, "uuid" + (caseNo++), 42, u[0], u[1], 71,
                 positions[1], vels[1], rots[1], 0);
        }
    }

    static void emit(RegistryAccess registryAccess, String name, int id, long uuidHi, long uuidLo,
                     int typeId, double[] pos, double[] vel, float[] rot, int data) throws Exception {
        UUID uuid = new UUID(uuidHi, uuidLo);
        EntityType<?> type = BuiltInRegistries.ENTITY_TYPE.byIdOrThrow(typeId);
        Identifier key = BuiltInRegistries.ENTITY_TYPE.getKey(type);
        String typeName = key.toString();

        // Sanity: the registry id of this type is exactly the id we asked for.
        int gotTypeId = BuiltInRegistries.ENTITY_TYPE.getId(type);
        if (gotTypeId != typeId) {
            throw new IllegalStateException("type id mismatch: wanted " + typeId + " got " + gotTypeId);
        }

        // Public constructor (ClientboundAddEntityPacket.java:69-93):
        //   (int id, UUID uuid, double x, double y, double z, float xRot, float yRot,
        //    EntityType<?> type, int data, Vec3 movement, double yHeadRot)
        ClientboundAddEntityPacket pkt = new ClientboundAddEntityPacket(
            id, uuid, pos[0], pos[1], pos[2],
            rot[0], rot[1],          // xRot, yRot (float degrees)
            type, data,
            new Vec3(vel[0], vel[1], vel[2]),
            (double) rot[2]          // yHeadRot (degrees, taken as double then (float))
        );

        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), registryAccess);
        ClientboundAddEntityPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec (sanity).
        RegistryFriendlyByteBuf rb = new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())), registryAccess);
        ClientboundAddEntityPacket back = ClientboundAddEntityPacket.STREAM_CODEC.decode(rb);
        if (back.getId() != id) throw new IllegalStateException("rt id " + back.getId() + " != " + id);
        if (!back.getUUID().equals(uuid)) throw new IllegalStateException("rt uuid mismatch " + name);
        if (BuiltInRegistries.ENTITY_TYPE.getId(back.getType()) != typeId)
            throw new IllegalStateException("rt type mismatch " + name);
        if (back.getData() != data) throw new IllegalStateException("rt data " + back.getData() + " != " + data);
        if (rb.readableBytes() != 0) throw new IllegalStateException("rt trailing bytes " + name);

        O.print("ENC\t");
        O.print(name);                 O.print('\t');
        O.print(id);                   O.print('\t');
        O.print(String.format("%016x", uuidHi)); O.print('\t');
        O.print(String.format("%016x", uuidLo)); O.print('\t');
        O.print(typeName);             O.print('\t');
        O.print(typeId);               O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(pos[0]))); O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(pos[1]))); O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(pos[2]))); O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(vel[0]))); O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(vel[1]))); O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(vel[2]))); O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(rot[0]))); O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(rot[1]))); O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(rot[2]))); O.print('\t');
        O.print(data);                 O.print('\t');
        O.print(n);                    O.print('\t');
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
