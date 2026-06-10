// Ground truth for net.minecraft.network.protocol.game.ClientboundRemoveMobEffectPacket.
//
// The packet's real STREAM_CODEC is, VERBATIM
// (ClientboundRemoveMobEffectPacket.java:14-21):
//   public record ClientboundRemoveMobEffectPacket(int entityId, Holder<MobEffect> effect) ...
//   STREAM_CODEC = StreamCodec.composite(
//      ByteBufCodecs.VAR_INT,        ::entityId,
//      MobEffect.STREAM_CODEC,       ::effect,
//      ::new)
//
// So the wire body, field-by-field in codec order, is:
//   (1) ByteBufCodecs.VAR_INT.encode  -> VarInt.write(out, entityId)
//       (ByteBufCodecs.java: VAR_INT writes VarInt(value)) -- a plain LEB128 VarInt.
//   (2) MobEffect.STREAM_CODEC.encode (MobEffect.java:41):
//       MobEffect.STREAM_CODEC = ByteBufCodecs.holderRegistry(Registries.MOB_EFFECT)
//       holderRegistry == registry(key, Registry::asHolderIdMap)  (ByteBufCodecs.java:584-586)
//       registry(...).encode (ByteBufCodecs.java:573-576):
//          int id = getRegistryOrThrow(out).getIdOrThrow(value); VarInt.write(out, id);
//       asHolderIdMap().getId(holder) = registry.getId(holder.value())  (Registry.java:145-149)
//       => a PLAIN VarInt of BuiltInRegistries.MOB_EFFECT.getId(effect) -- NO +1.
//          (holderRegistry uses registry()'s plain-id form, NOT holder()'s id+1 form.)
//   read (ByteBufCodecs.java:568-571): entityId = VarInt.read; effect = byIdOrThrow(VarInt.read).
//
// CRUCIALLY the registry-held field is captured both as the raw VarInt id AND as the
// registry entry name (ns:path), so the C++ side resolves the name -> id via the
// network_registries.tsv table (NOT a hard-coded id) and confirms == the GT id.
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <entityId-dec> <effectName ns:path> <effectId-dec> <readableBytes> <hex>
// where entityId is the signed-32 VarInt arg (decimal), effectName is the MobEffect
// registry key (Identifier "ns:path"), effectId is BuiltInRegistries.MOB_EFFECT.getId
// (the VarInt payload, decimal), readableBytes is the total payload length, and hex is
// the full payload as lowercase bytes.
//
// The C++ pkt_remove_mob_effect_parity resolves effectName -> id via NetworkRegistries
// (FAIL loudly if absent or != effectId), rebuilds the body via PacketBuffer
// (writeVarInt(entityId) + writeVarInt(effectId)) and must match hex byte-for-byte and
// readableBytes; it then decodes hex back (readVarInt id, reg.name back) and checks fields.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundRemoveMobEffectPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.effect.MobEffect;

public class PktRemoveMobEffectParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The STREAM_CODEC is typed RegistryFriendlyByteBuf; the holderRegistry codec
        // resolves Registries.MOB_EFFECT from the buffer's RegistryAccess, so build a
        // real RegistryAccess-backed buffer to exercise the typed codec faithfully.
        RegistryAccess registryAccess = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        int effectCount = BuiltInRegistries.MOB_EFFECT.size();
        if (effectCount <= 0) throw new IllegalStateException("MOB_EFFECT registry empty");

        // entityId battery: 0, small, signs and the VarInt 1->2->3->4->5 byte boundaries.
        // ByteBufCodecs.VAR_INT is a SIGNED 32-bit VarInt (negatives -> full 5 bytes).
        int[] entityIds = {
            0, 1, 2, 126, 127, 128, 129, 254, 255, 256,
            16383, 16384, 16385, 2097151, 2097152,
            Integer.MAX_VALUE, -1, -128, Integer.MIN_VALUE,
        };

        // Effect-id battery: id 0 (first registered), a few named ones spread across the
        // registry, and the last id (a "large id"). byIdOrThrow == the registry id we ask.
        int[] effectIds = {
            0,                       // minecraft:speed
            1,                       // minecraft:slowness
            18,                      // minecraft:poison (named, mid)
            31,                      // minecraft:hero_of_the_village
            effectCount / 2,
            effectCount - 1,         // largest id
        };

        // (A) Sweep entityIds at a fixed effect (id 0) and a large effect (last id).
        for (int e : entityIds) {
            emit(registryAccess, "ent", e, 0);
            emit(registryAccess, "ent", e, effectCount - 1);
        }

        // (B) Sweep effect ids at a fixed entityId.
        for (int eid : effectIds) {
            if (eid < 0 || eid >= effectCount) continue;
            emit(registryAccess, "eff", 1, eid);
        }

        // (C) A few fully-mixed cases (sign / boundary x named effect).
        emit(registryAccess, "mix", Integer.MAX_VALUE, effectCount - 1);
        emit(registryAccess, "mix", -1, 18);
        emit(registryAccess, "mix", Integer.MIN_VALUE, 0);
        emit(registryAccess, "mix", 12345, Math.min(9, effectCount - 1)); // regeneration
    }

    @SuppressWarnings("deprecation")
    static void emit(RegistryAccess registryAccess, String name,
                     int entityId, int effectId) throws Exception {
        Holder.Reference<MobEffect> effect = BuiltInRegistries.MOB_EFFECT.get(effectId)
            .orElseThrow(() -> new IllegalStateException("no MobEffect at id " + effectId));

        // sanity: the registry id of this effect's value is exactly the id we asked for
        // (this is what asHolderIdMap().getId(holder) returns on the wire).
        int gotId = BuiltInRegistries.MOB_EFFECT.getId(effect.value());
        if (gotId != effectId) {
            throw new IllegalStateException("effect id mismatch: wanted " + effectId + " got " + gotId);
        }
        Identifier effectKey = BuiltInRegistries.MOB_EFFECT.getKey(effect.value());
        if (effectKey == null) throw new IllegalStateException("no key for effect id " + effectId);

        ClientboundRemoveMobEffectPacket pkt =
            new ClientboundRemoveMobEffectPacket(entityId, (Holder<MobEffect>) effect);

        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), registryAccess);
        ClientboundRemoveMobEffectPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        RegistryFriendlyByteBuf rbuf =
            new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())), registryAccess);
        ClientboundRemoveMobEffectPacket back =
            ClientboundRemoveMobEffectPacket.STREAM_CODEC.decode(rbuf);
        int backId = BuiltInRegistries.MOB_EFFECT.getId(back.effect().value());
        if (back.entityId() != entityId || backId != effectId) {
            throw new IllegalStateException("round-trip mismatch for " + name + " ent=" + entityId
                + " eff=" + effectId + " -> ent=" + back.entityId() + " eff=" + backId);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(entityId);              // signed-32 VarInt arg, decimal
        O.print('\t');
        O.print(effectKey.toString());  // ns:path
        O.print('\t');
        O.print(effectId);              // VarInt registry-id payload, decimal
        O.print('\t');
        O.print(n);                     // readableBytes
        O.print('\t');
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
