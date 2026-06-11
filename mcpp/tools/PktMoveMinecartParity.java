// Ground truth for net.minecraft.network.protocol.game.ClientboundMoveMinecartPacket.
//
// 26.1.2 wire format (verified against the REAL source):
//   public record ClientboundMoveMinecartPacket(int entityId, List<NewMinecartBehavior.MinecartStep> lerpSteps)
//   STREAM_CODEC = StreamCodec.composite(
//       ByteBufCodecs.VAR_INT,                                 ::entityId,   -> VarInt entityId
//       NewMinecartBehavior.MinecartStep.STREAM_CODEC.apply(ByteBufCodecs.list()),
//                                                              ::lerpSteps,  -> list of MinecartStep
//       ::new)
//
// ByteBufCodecs.list() == collection(ArrayList::new): encode writes VarInt(size) then each element.
//
// NewMinecartBehavior.MinecartStep(Vec3 position, Vec3 movement, float yRot, float xRot, float weight):
//   MinecartStep.STREAM_CODEC = StreamCodec.composite(
//       Vec3.STREAM_CODEC,        ::position,  -> 3 doubles (x,y,z) big-endian
//       Vec3.STREAM_CODEC,        ::movement,  -> 3 doubles (x,y,z) big-endian
//       ByteBufCodecs.ROTATION_BYTE, ::yRot,   -> 1 byte = Mth.packDegrees(yRot)
//       ByteBufCodecs.ROTATION_BYTE, ::xRot,   -> 1 byte = Mth.packDegrees(xRot)
//       ByteBufCodecs.FLOAT,      ::weight,    -> 4-byte big-endian float
//       ::new)
//   Vec3.STREAM_CODEC: encode = writeDouble(x); writeDouble(y); writeDouble(z).
//   ByteBufCodecs.ROTATION_BYTE = BYTE.map(Mth::unpackDegrees, Mth::packDegrees), so encode
//     writes (byte)Mth.packDegrees(angle) where packDegrees(a) = (byte)floor(a * 256.0F / 360.0F).
//     NOTE the multiply is FLOAT precision, the floor is Math.floor(double), cast to byte (low 8 bits).
//   ByteBufCodecs.FLOAT: encode = output.writeFloat(weight) (IEEE-754 32-bit big-endian).
//
// There is NO packet-id prefix (the type id is framed elsewhere). The codec is declared over
// FriendlyByteBuf/RegistryFriendlyByteBuf; we encode through a real RegistryAccess-backed
// RegistryFriendlyByteBuf to mirror the real send path (no registry lookups actually occur).
//
// Row format (tab-separated), TAG = ENC. The position/movement doubles and the yRot/xRot/weight
// floats are emitted as RAW IEEE-754 bit patterns (Double.doubleToRawLongBits / Float
// .floatToRawIntBits, lowercase hex) so the C++ side reconstructs the EXACT same numbers and
// replays the exact quantization byte-for-byte:
//   ENC <name> <entityId-dec> <stepCount-dec> [ per step: <px16> <py16> <pz16> <mx16> <my16> <mz16>
//                                               <yRot8> <xRot8> <weight8> ] <readableBytes-dec> <hexBytes>
// where px16.. are %016x of doubleToRawLongBits, yRot8/xRot8/weight8 are %08x of floatToRawIntBits.
//
// We round-trip-decode every case through the SAME codec and assert entityId + step count match,
// and the raw position/movement doubles round-trip exactly (lossless), as a sanity check.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import java.util.ArrayList;
import java.util.List;

import net.minecraft.SharedConstants;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundMoveMinecartPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.entity.vehicle.minecart.NewMinecartBehavior;
import net.minecraft.world.phys.Vec3;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktMoveMinecartParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Real RegistryAccess (the codec is RegistryFriendlyByteBuf-typed even though this packet
        // performs no registry lookups; mirror the real send path exactly).
        RegistryAccess registryAccess = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // entityId pins every LEB128 byte boundary (1->5 bytes) and signed extremes.
        int[] ids = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 42, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // A bank of MinecartStep values chosen to exercise:
        //  - Vec3 doubles: zero, +-, sub-unit, large, NaN/Inf/-0 (Vec3.STREAM_CODEC is raw writeDouble, lossless)
        //  - ROTATION_BYTE: angles hitting every packDegrees wrap (0,90,180,270,360, negatives,
        //    non-multiples-of-360/256 so floor matters, +-720 wrap, and tricky fractional ones)
        //  - FLOAT weight: 0, +-, subnormal, NaN, Inf, -0
        Step[] steps = {
            new Step(0,0,0,  0,0,0,            0f,    0f,    0f),
            new Step(1.5,-2.25,3.125,  -0.5,0.0625,-0.125,  90f,   -90f,  1.0f),
            new Step(100.0,-200.0,300.0,  0.001,-0.002,0.003,  180f, 270f, 0.5f),
            new Step(-9.81,0.0,12.5,  1e-9,-1e-9,5e-10,  45.0f, 135.0f, 2.0f),
            new Step(1.0e9,-1.0e9,1.0e9,  -3.4,7.7,-1.1,  359.9f, 0.1f, 0.25f),
            new Step(0.1,0.2,0.3,  -0.3,-0.2,-0.1,  -1.0f, 360.0f, -1.5f),
            new Step(-0.0,0.0,-0.0,  0.0,-0.0,0.0,  720.0f, -720.0f, 3.0f),
            new Step(Double.NaN,1.0,-1.0,  Double.POSITIVE_INFINITY,Double.NEGATIVE_INFINITY,0.0,
                     30.0f, 210.0f, Float.NaN),
            new Step(Double.MAX_VALUE,-Double.MAX_VALUE,Double.MIN_VALUE,  0,0,0,
                     1.234f, -5.678f, Float.POSITIVE_INFINITY),
            new Step(2.2250738585072014e-308,-1.0,1.0,  0,0,0,
                     256.0f, -256.0f, Float.MIN_VALUE), // packDegrees(256)= floor(256*256/360)
        };

        // Step counts chosen to cross the list-count VarInt boundary and exercise empty list.
        int caseNo = 0;
        // empty list
        emit(registryAccess, "empty" + (caseNo++), 7, new ArrayList<>());
        // single step, every step variant
        for (Step s : steps) {
            List<NewMinecartBehavior.MinecartStep> one = new ArrayList<>();
            one.add(s.toMc());
            emit(registryAccess, "one" + (caseNo++), 42, one);
        }
        // multi-step list (all steps), across several ids to vary the entityId VarInt
        List<NewMinecartBehavior.MinecartStep> all = new ArrayList<>();
        for (Step s : steps) all.add(s.toMc());
        for (int id : ids) {
            emit(registryAccess, "multi" + (caseNo++), id, all);
        }
        // a list whose size needs a 2-byte VarInt count (>=128 elements): repeat a simple step.
        List<NewMinecartBehavior.MinecartStep> big = new ArrayList<>();
        for (int i = 0; i < 130; i++) {
            big.add(new Step(i, -i, i*0.5, 0.1, -0.1, 0.2, (float)(i*3), (float)(-i*2), (float)i).toMc());
        }
        emit(registryAccess, "big" + (caseNo++), 12345, big);
    }

    static final class Step {
        final double px, py, pz, mx, my, mz;
        final float yRot, xRot, weight;
        Step(double px,double py,double pz,double mx,double my,double mz,float y,float x,float w){
            this.px=px;this.py=py;this.pz=pz;this.mx=mx;this.my=my;this.mz=mz;
            this.yRot=y;this.xRot=x;this.weight=w;
        }
        NewMinecartBehavior.MinecartStep toMc(){
            return new NewMinecartBehavior.MinecartStep(
                new Vec3(px,py,pz), new Vec3(mx,my,mz), yRot, xRot, weight);
        }
    }

    static void emit(RegistryAccess registryAccess, String name, int entityId,
                     List<NewMinecartBehavior.MinecartStep> steps) {
        ClientboundMoveMinecartPacket pkt = new ClientboundMoveMinecartPacket(entityId, steps);

        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), registryAccess);
        ClientboundMoveMinecartPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity).
        RegistryFriendlyByteBuf rb = new RegistryFriendlyByteBuf(Unpooled.copiedBuffer(buf), registryAccess);
        ClientboundMoveMinecartPacket back = ClientboundMoveMinecartPacket.STREAM_CODEC.decode(rb);
        if (back.entityId() != entityId)
            throw new IllegalStateException("round-trip entityId mismatch " + back.entityId() + " != " + entityId);
        if (back.lerpSteps().size() != steps.size())
            throw new IllegalStateException("round-trip step count mismatch");
        for (int i = 0; i < steps.size(); i++) {
            NewMinecartBehavior.MinecartStep a = steps.get(i);
            NewMinecartBehavior.MinecartStep b = back.lerpSteps().get(i);
            // position/movement are raw doubles: must round-trip with identical bits.
            if (Double.doubleToRawLongBits(a.position().x()) != Double.doubleToRawLongBits(b.position().x())
             || Double.doubleToRawLongBits(a.position().y()) != Double.doubleToRawLongBits(b.position().y())
             || Double.doubleToRawLongBits(a.position().z()) != Double.doubleToRawLongBits(b.position().z())
             || Double.doubleToRawLongBits(a.movement().x()) != Double.doubleToRawLongBits(b.movement().x())
             || Double.doubleToRawLongBits(a.movement().y()) != Double.doubleToRawLongBits(b.movement().y())
             || Double.doubleToRawLongBits(a.movement().z()) != Double.doubleToRawLongBits(b.movement().z()))
                throw new IllegalStateException("round-trip Vec3 bits mismatch at step " + i);
            // weight is raw float: lossless.
            if (Float.floatToRawIntBits(a.weight()) != Float.floatToRawIntBits(b.weight()))
                throw new IllegalStateException("round-trip weight bits mismatch at step " + i);
        }
        if (rb.readableBytes() != 0)
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(entityId);
        O.print('\t');
        O.print(steps.size());
        for (NewMinecartBehavior.MinecartStep s : steps) {
            O.print('\t'); O.print(String.format("%016x", Double.doubleToRawLongBits(s.position().x())));
            O.print('\t'); O.print(String.format("%016x", Double.doubleToRawLongBits(s.position().y())));
            O.print('\t'); O.print(String.format("%016x", Double.doubleToRawLongBits(s.position().z())));
            O.print('\t'); O.print(String.format("%016x", Double.doubleToRawLongBits(s.movement().x())));
            O.print('\t'); O.print(String.format("%016x", Double.doubleToRawLongBits(s.movement().y())));
            O.print('\t'); O.print(String.format("%016x", Double.doubleToRawLongBits(s.movement().z())));
            O.print('\t'); O.print(String.format("%08x", Float.floatToRawIntBits(s.yRot())));
            O.print('\t'); O.print(String.format("%08x", Float.floatToRawIntBits(s.xRot())));
            O.print('\t'); O.print(String.format("%08x", Float.floatToRawIntBits(s.weight())));
        }
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
