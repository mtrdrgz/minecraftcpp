// Ground truth for net.minecraft.network.protocol.game.ClientboundUpdateMobEffectPacket.
//
// STREAM_CODEC = Packet.codec(ClientboundUpdateMobEffectPacket::write,
//                             ClientboundUpdateMobEffectPacket::new)  [class line 12-14]
// -> StreamCodec.ofMember: NO packet-id prefix, body only. The body is exactly
// (ClientboundUpdateMobEffectPacket.write, lines 58-64):
//   output.writeVarInt(this.entityId)
//   MobEffect.STREAM_CODEC.encode(output, this.effect)   // Holder<MobEffect>
//   output.writeVarInt(this.effectAmplifier)
//   output.writeVarInt(this.effectDurationTicks)
//   output.writeByte(this.flags)
//
// MobEffect.STREAM_CODEC (net.minecraft.world.effect.MobEffect line 41):
//   ByteBufCodecs.holderRegistry(Registries.MOB_EFFECT)
// holderRegistry -> registry(key, Registry::asHolderIdMap) whose encode (ByteBufCodecs
// lines 573-576) is EXACTLY:
//   int id = getRegistryOrThrow(output).getIdOrThrow(value); VarInt.write(output, id);
// i.e. a PLAIN VarInt of getId (NO +1 — that +1 belongs to the holder()/optional
// reference form, holderRegistry is the bare registry id). asHolderIdMap().getId(holder)
// == BuiltInRegistries.MOB_EFFECT.getId(value), the 0-based registration index, which is
// exactly what assets/network_registries.tsv stores for minecraft:mob_effect.
//
// flags byte (ClientboundUpdateMobEffectPacket lines 15-18, 30-47):
//   FLAG_AMBIENT=1 if effect.isAmbient(); FLAG_VISIBLE=2 if effect.isVisible();
//   FLAG_SHOW_ICON=4 if effect.showIcon(); FLAG_BLEND=8 if blend.
//
// We encode each case through the REAL STREAM_CODEC into a RegistryFriendlyByteBuf wrapping
// a bootstrapped RegistryAccess (BuiltInRegistries), so the effect VarInt is produced by the
// genuine codec path. We round-trip-decode through the same codec for sanity.
//
// Row format (tab separated):
//   ENC <entityId-dec> <effectName ns:path> <amplifier-dec> <durationTicks-dec>
//       <flags-dec> <readableBytes-dec> <hex>
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundUpdateMobEffectPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;

public class PktUpdateMobEffectParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Real registry access (built-in registries) so holderRegistry(MOB_EFFECT) resolves.
        RegistryAccess registries =
            RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        StreamCodec<RegistryFriendlyByteBuf, ClientboundUpdateMobEffectPacket> CODEC =
            (StreamCodec<RegistryFriendlyByteBuf, ClientboundUpdateMobEffectPacket>)
                ClientboundUpdateMobEffectPacket.STREAM_CODEC;

        // Battery: spread of real mob effects (incl id 0 = speed and the largest id
        // = infested, currently 38 of 40 registered) crossed with finite/boundary
        // amplifier+duration values and every flag-bit combination.
        // Each row: { effectName, entityId, amplifier, duration, ambient, visible, showIcon, blend }
        Object[][] cases = {
            // effect id 0 (speed) — flag sweep
            {"minecraft:speed",          0,   0,   0, false, false, false, false},
            {"minecraft:speed",          1,   0,   0, true,  false, false, false}, // flags=1
            {"minecraft:speed",          2,   0,   0, false, true,  false, false}, // flags=2
            {"minecraft:speed",          3,   0,   0, false, false, true,  false}, // flags=4
            {"minecraft:speed",          4,   0,   0, false, false, false, true},  // flags=8
            {"minecraft:speed",          5,   0,   0, true,  true,  true,  true},   // flags=15
            // varied effects across the id range
            {"minecraft:slowness",       10,  1,   200, false, true,  true,  false},
            {"minecraft:regeneration",   42,  4,   900, true,  false, true,  true},
            {"minecraft:fire_resistance",100, 0,   3600, false, true,  false, false},
            {"minecraft:night_vision",   255, 2,   12000, true, true,  false, true},
            {"minecraft:poison",         1024, 3,  -1, false, false, true,  false}, // -1 dur (infinite) -> 5-byte varint
            {"minecraft:wither",         16384, 5, 16383, true, true,  true,  false},
            {"minecraft:absorption",     2097152, 7, 2097151, false, true, true, true},
            {"minecraft:luck",           268435456, 9, 268435455, true, false, false, true}, // 4->5 byte boundary
            // largest currently-registered effect id (infested = 38)
            {"minecraft:infested",       7,   127, 1000000, true, true,  true,  true},
            // entityId boundaries (VarInt is non-zig-zag: negatives -> 5 bytes)
            {"minecraft:darkness",       Integer.MAX_VALUE, 0, 0, false, false, false, false},
            {"minecraft:hunger",         -1,  0,   0, false, false, false, false},
            {"minecraft:weakness",       Integer.MIN_VALUE, 255, Integer.MAX_VALUE, true, true, true, true},
            // amplifier boundary (writeVarInt of amplifier; amplifier is a plain int here)
            {"minecraft:strength",       9,   Integer.MAX_VALUE, 5, false, true, false, false},
            {"minecraft:haste",          9,   0,   Integer.MIN_VALUE, true, false, true, false},
        };

        for (Object[] c : cases) {
            String effectName = (String) c[0];
            int entityId   = (Integer) c[1];
            int amplifier  = (Integer) c[2];
            int duration   = (Integer) c[3];
            boolean ambient = (Boolean) c[4];
            boolean visible = (Boolean) c[5];
            boolean showIcon = (Boolean) c[6];
            boolean blend   = (Boolean) c[7];

            Holder<MobEffect> effect =
                BuiltInRegistries.MOB_EFFECT.get(Identifier.parse(effectName)).orElseThrow();

            // Build the real MobEffectInstance with the exact flag controls, then the packet.
            // NOTE: MobEffectInstance clamps amplifier to [0,255] (MobEffectInstance:81) — the
            // wire carries the CLAMPED value, so we emit the packet's actual getter values
            // (getEffectAmplifier/getEffectDurationTicks/getEntityId), never our raw inputs.
            MobEffectInstance inst =
                new MobEffectInstance(effect, duration, amplifier, ambient, visible, showIcon);
            ClientboundUpdateMobEffectPacket pkt =
                new ClientboundUpdateMobEffectPacket(entityId, inst, blend);

            int outEntityId = pkt.getEntityId();
            int outAmplifier = pkt.getEffectAmplifier();
            int outDuration = pkt.getEffectDurationTicks();
            int outFlags = (pkt.isEffectAmbient() ? 1 : 0) | (pkt.isEffectVisible() ? 2 : 0)
                         | (pkt.effectShowsIcon() ? 4 : 0) | (pkt.shouldBlend() ? 8 : 0);
            String outEffectName = pkt.getEffect().getRegisteredName(); // ns:path

            // ENC through the REAL codec into a RegistryFriendlyByteBuf.
            RegistryFriendlyByteBuf buf =
                new RegistryFriendlyByteBuf(Unpooled.buffer(), registries);
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec; assert all fields recover.
            RegistryFriendlyByteBuf rbuf =
                new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)), registries);
            ClientboundUpdateMobEffectPacket dec = CODEC.decode(rbuf);
            int decFlags = (dec.isEffectAmbient() ? 1 : 0) | (dec.isEffectVisible() ? 2 : 0)
                         | (dec.effectShowsIcon() ? 4 : 0) | (dec.shouldBlend() ? 8 : 0);
            String decEffectName = dec.getEffect().getRegisteredName(); // ns:path
            if (dec.getEntityId() != outEntityId
                || !decEffectName.equals(outEffectName)
                || dec.getEffectAmplifier() != outAmplifier
                || dec.getEffectDurationTicks() != outDuration
                || decFlags != outFlags) {
                throw new IllegalStateException(
                    "round-trip mismatch for " + effectName
                    + " in=(eid=" + outEntityId + ",amp=" + outAmplifier + ",dur=" + outDuration
                    + ",flags=" + outFlags + ")"
                    + " out=(eid=" + dec.getEntityId() + ",eff=" + decEffectName
                    + ",amp=" + dec.getEffectAmplifier() + ",dur=" + dec.getEffectDurationTicks()
                    + ",flags=" + decFlags + ")");
            }

            O.print("ENC\t");
            O.print(outEntityId);
            O.print('\t');
            O.print(outEffectName);
            O.print('\t');
            O.print(outAmplifier);
            O.print('\t');
            O.print(outDuration);
            O.print('\t');
            O.print(outFlags);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }

    static String toHex(ByteBuf b) {
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
