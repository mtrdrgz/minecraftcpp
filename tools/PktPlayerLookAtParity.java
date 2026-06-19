// Ground truth for net.minecraft.network.protocol.game.ClientboundPlayerLookAtPacket.
//
// 26.1.2 wire format — verified VERBATIM against the REAL packet write/read (FriendlyByteBuf):
//   ClientboundPlayerLookAtPacket.STREAM_CODEC = Packet.codec(::write, ::new)
//   private void write(FriendlyByteBuf output) {
//       output.writeEnum(this.fromAnchor);   -> VarInt(ordinal)      EntityAnchorArgument.Anchor
//       output.writeDouble(this.x);          -> double x   (8B, BE)
//       output.writeDouble(this.y);          -> double y   (8B, BE)
//       output.writeDouble(this.z);          -> double z   (8B, BE)
//       output.writeBoolean(this.atEntity);  -> byte 0/1
//       if (this.atEntity) {
//           output.writeVarInt(this.entity);   -> VarInt entityId
//           output.writeEnum(this.toAnchor);   -> VarInt(ordinal)    EntityAnchorArgument.Anchor
//       }
//   }
//   FriendlyByteBuf.writeEnum(Enum) == writeVarInt(value.ordinal())  (verified line 471-473).
//   FriendlyByteBuf.readEnum(clazz) == clazz.getEnumConstants()[readVarInt()] (line 467-469).
//   EntityAnchorArgument.Anchor enum order: FEET(0), EYES(1).
//
// Full payload order:
//   VarInt(fromAnchor.ordinal()) | double x | double y | double z | byte atEntity
//   [ if atEntity: VarInt(entity) | VarInt(toAnchor.ordinal()) ]
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <fromOrd-dec> <xBits-016x> <yBits-016x> <zBits-016x>
//       <atEntity-dec> <entity-dec> <toOrd-dec> <readableBytes-dec> <hexBytes>
// where x/y/z bits are Double.doubleToRawLongBits (lowercase 16-hex); fromOrd/toOrd are enum
// ordinals (decimal); atEntity is 0/1; entity is decimal. When atEntity==0, entity/toOrd are
// emitted as 0 but the C++ side must NOT write them (they are absent from the wire).
//
// The atEntity=true public constructor requires a live Entity (registry-held); rather than build
// one we set the private final fields via reflection (setAccessible) on an instance created with
// the public no-entity ctor. NEVER sun.misc.Unsafe / ReflectionFactory. All inputs finite/physical.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.commands.arguments.EntityAnchorArgument;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundPlayerLookAtPacket;
import net.minecraft.server.Bootstrap;

import java.lang.reflect.Field;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktPlayerLookAtParity {
    static final java.io.PrintStream O = System.out;

    static Field F_X, F_Y, F_Z, F_ENTITY, F_FROM, F_TO, F_AT;

    static void initReflection() throws Exception {
        Class<?> c = ClientboundPlayerLookAtPacket.class;
        F_X = c.getDeclaredField("x");           F_X.setAccessible(true);
        F_Y = c.getDeclaredField("y");           F_Y.setAccessible(true);
        F_Z = c.getDeclaredField("z");           F_Z.setAccessible(true);
        F_ENTITY = c.getDeclaredField("entity"); F_ENTITY.setAccessible(true);
        F_FROM = c.getDeclaredField("fromAnchor"); F_FROM.setAccessible(true);
        F_TO = c.getDeclaredField("toAnchor");   F_TO.setAccessible(true);
        F_AT = c.getDeclaredField("atEntity");   F_AT.setAccessible(true);
    }

    // Build a packet with arbitrary field values by constructing via the public no-entity ctor
    // (sets fromAnchor/x/y/z, atEntity=false, entity=0, toAnchor=null) then overwriting the
    // private final fields via reflection. This avoids needing a live Entity.
    static ClientboundPlayerLookAtPacket build(EntityAnchorArgument.Anchor fromAnchor,
                                               double x, double y, double z,
                                               boolean atEntity, int entity,
                                               EntityAnchorArgument.Anchor toAnchor) throws Exception {
        ClientboundPlayerLookAtPacket pkt =
            new ClientboundPlayerLookAtPacket(fromAnchor, x, y, z);
        // fromAnchor/x/y/z already set by ctor, but set again for clarity/robustness.
        F_FROM.set(pkt, fromAnchor);
        F_X.setDouble(pkt, x);
        F_Y.setDouble(pkt, y);
        F_Z.setDouble(pkt, z);
        F_AT.setBoolean(pkt, atEntity);
        F_ENTITY.setInt(pkt, entity);
        F_TO.set(pkt, toAnchor);
        return pkt;
    }

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();
        initReflection();

        EntityAnchorArgument.Anchor[] anchors = EntityAnchorArgument.Anchor.values(); // FEET, EYES

        // Double coordinate samples: 0.0/-0.0/1.0/small/large/typical-coordinate/extrema/sign.
        double[] ds = {
            0.0, -0.0, 1.0, -1.0, 0.5, -0.5,
            3.0517578125E-5, -2.5E-7,
            64.5, -123.0, 1234.5, -678.25,
            30000000.0, -29999999.5,                 // near world border
            1.0E10, -1.0E10,
            Double.MAX_VALUE, -Double.MAX_VALUE,
            Double.MIN_VALUE, Double.MIN_NORMAL,
            123.456789, -987.654321
        };

        // Entity ids: exercise VarInt 1->5 byte boundaries + negatives + extrema.
        int[] ids = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 42, 12345, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        int caseNo = 0;
        int nd = ds.length, ni = ids.length, na = anchors.length;

        // atEntity=false cases: walk doubles + anchors; entity/toAnchor absent from the wire.
        for (int k = 0; k < nd * 2; k++) {
            EntityAnchorArgument.Anchor from = anchors[k % na];
            double x = ds[k % nd];
            double y = ds[(k + 5) % nd];
            double z = ds[(k + 11) % nd];
            emit("pos" + (caseNo++), from, x, y, z, false, 0, null);
        }

        // atEntity=true cases: walk ids (VarInt boundaries) x anchors x doubles.
        int totalAt = Math.max(ni, nd) * 4;
        for (int k = 0; k < totalAt; k++) {
            EntityAnchorArgument.Anchor from = anchors[k % na];
            EntityAnchorArgument.Anchor to = anchors[(k + 1) % na];
            int entity = ids[k % ni];
            double x = ds[k % nd];
            double y = ds[(k + 3) % nd];
            double z = ds[(k + 7) % nd];
            emit("ent" + (caseNo++), from, x, y, z, true, entity, to);
        }

        // Explicit extreme/edge rows.
        emit("zero",   anchors[0], 0.0, 0.0, 0.0, false, 0, null);
        emit("negzero",anchors[1], -0.0, -0.0, -0.0, false, 0, null);
        emit("eyes_at_zero", EntityAnchorArgument.Anchor.EYES, 0.0, 0.0, 0.0, true, 0,
             EntityAnchorArgument.Anchor.FEET);
        emit("feet_at_maxid", EntityAnchorArgument.Anchor.FEET, 30000000.0, 320.0, -30000000.0,
             true, Integer.MAX_VALUE, EntityAnchorArgument.Anchor.EYES);
        emit("eyes_at_minid", EntityAnchorArgument.Anchor.EYES, -30000000.0, -64.0, 30000000.0,
             true, Integer.MIN_VALUE, EntityAnchorArgument.Anchor.FEET);
        emit("dblext_at", EntityAnchorArgument.Anchor.FEET, Double.MAX_VALUE, -Double.MAX_VALUE,
             Double.MIN_VALUE, true, 268435455, EntityAnchorArgument.Anchor.EYES);
        emit("border_at", EntityAnchorArgument.Anchor.EYES, 1.0E10, -1.0E10, 123.456789,
             true, 268435456, EntityAnchorArgument.Anchor.EYES);
    }

    static void emit(String name, EntityAnchorArgument.Anchor from,
                     double x, double y, double z,
                     boolean atEntity, int entity, EntityAnchorArgument.Anchor to) throws Exception {
        ClientboundPlayerLookAtPacket pkt = build(from, x, y, z, atEntity, entity, to);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundPlayerLookAtPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): every field must match exactly.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundPlayerLookAtPacket back = ClientboundPlayerLookAtPacket.STREAM_CODEC.decode(rb);
        double bx = F_X.getDouble(back), by = F_Y.getDouble(back), bz = F_Z.getDouble(back);
        boolean bat = F_AT.getBoolean(back);
        int bent = F_ENTITY.getInt(back);
        EntityAnchorArgument.Anchor bfrom = (EntityAnchorArgument.Anchor) F_FROM.get(back);
        EntityAnchorArgument.Anchor bto = (EntityAnchorArgument.Anchor) F_TO.get(back);
        if (bfrom != from
            || Double.doubleToRawLongBits(bx) != Double.doubleToRawLongBits(x)
            || Double.doubleToRawLongBits(by) != Double.doubleToRawLongBits(y)
            || Double.doubleToRawLongBits(bz) != Double.doubleToRawLongBits(z)
            || bat != atEntity) {
            throw new IllegalStateException("round-trip field mismatch for " + name);
        }
        if (atEntity) {
            if (bent != entity || bto != to) {
                throw new IllegalStateException("round-trip entity/toAnchor mismatch for " + name);
            }
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(from.ordinal());
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(x)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(y)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(z)));
        O.print('\t');
        O.print(atEntity ? 1 : 0);
        O.print('\t');
        O.print(entity);
        O.print('\t');
        O.print(to == null ? 0 : to.ordinal());
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
