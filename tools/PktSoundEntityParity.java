// Ground truth for net.minecraft.network.protocol.game.ClientboundSoundEntityPacket.
//
// The packet's real STREAM_CODEC is Packet.codec(write, ::new) over a
// RegistryFriendlyByteBuf. The write(RegistryFriendlyByteBuf) body is, VERBATIM
// (ClientboundSoundEntityPacket.java:43-50):
//   SoundEvent.STREAM_CODEC.encode(output, this.sound); // Holder<SoundEvent>
//   output.writeEnum(this.source);                      // SoundSource
//   output.writeVarInt(this.id);                        // int entity id
//   output.writeFloat(this.volume);                     // float (BE)
//   output.writeFloat(this.pitch);                      // float (BE)
//   output.writeLong(this.seed);                        // long (BE)
//
// SoundEvent.STREAM_CODEC = ByteBufCodecs.holder(Registries.SOUND_EVENT, DIRECT_STREAM_CODEC)
//   (SoundEvent.java:26). ByteBufCodecs.holder().encode (ByteBufCodecs.java:603-613):
//     REFERENCE: VarInt.write(out, registry.getIdOrThrow(holder) + 1)
//     DIRECT   : VarInt.write(out, 0); directCodec.encode(...)
//   Every vanilla SoundEvents.* is a registered REFERENCE, so on the wire it is a single
//   VarInt = (BuiltInRegistries.SOUND_EVENT.getId(value) + 1). We only emit REFERENCE
//   sounds, so the C++ side reconstructs the holder from just (registry id + 1).
//
//   writeEnum(e) = writeVarInt(e.ordinal())  (FriendlyByteBuf.java:471-473). SoundSource
//   ordinals: MASTER0 MUSIC1 RECORDS2 WEATHER3 BLOCKS4 HOSTILE5 NEUTRAL6 PLAYERS7
//             AMBIENT8 VOICE9 UI10 (SoundSource.java:4-14).
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <soundId-dec> <soundKey ns:path> <sourceOrdinal-dec> <entityId-dec>
//       <volumeBits-08x> <pitchBits-08x> <seed-dec> <readableBytes-dec> <hex>
// where soundId is the SOUND_EVENT registry id (the on-wire VarInt is soundId+1), soundKey
// is the registry entry ns:path (so the C++ side resolves name->id via NetworkRegistries),
// volume/pitch bits are Float.floatToRawIntBits of the inputs, seed is a signed 64-bit
// decimal, readableBytes is the total payload length, hex is the full payload lowercase.
//
// The C++ pkt_sound_entity_parity resolves soundKey -> id via NetworkRegistries, writes
// VarInt(id+1) + VarInt(sourceOrdinal) + VarInt(entityId) + float + float + long via
// PacketBuffer and must match hex byte-for-byte AND readableBytes; it then decodes hex back
// and checks all fields (id-1 -> name round-trip via NetworkRegistries).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.server.Bootstrap;

public class PktSoundEntityParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // A frozen RegistryAccess that resolves Registries.SOUND_EVENT -> BuiltInRegistries
        // .SOUND_EVENT, exactly what RegistryFriendlyByteBuf needs for ByteBufCodecs.holder.
        RegistryAccess registryAccess = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        int soundCount = BuiltInRegistries.SOUND_EVENT.size();

        // Sound-id battery: id 0, VarInt(id+1) 1->2 byte boundary corners, and the last id
        // (a large multi-byte VarInt). byIdOrThrow(i) gives the sound whose getId == i.
        int[] soundIds = {
            0, 1, 2, 126, 127, 128, 254, 255, 256, soundCount / 2, soundCount - 1
        };

        // All 11 SoundSource ordinals.
        SoundSource[] sources = SoundSource.values();

        // Entity-id battery: VarInt 1->5 byte boundaries + negatives + extrema.
        int[] entityIds = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 42, 12345, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // Finite float battery (volume/pitch). Vanilla volume/pitch are typically [0,2] but
        // the codec writes raw floats, so we also pin signs / sub-unit / large / extremes.
        float[] vols = {
            0.0f, 1.0f, 0.5f, 2.0f, 0.25f, -1.0f, 3.4028235e38f, Float.MIN_VALUE
        };
        float[] pitches = {
            1.0f, 0.5f, 2.0f, 0.0f, 1.5f, -0.5f, 1.2345679e10f, 1.17549435e-38f
        };

        // Seed battery: 0, small, signs, byte boundaries, extrema.
        long[] seeds = {
            0L, 1L, -1L, 255L, 256L, 65535L, 4294967296L, 1234567890123456789L,
            Long.MAX_VALUE, Long.MIN_VALUE, -9223372036854775807L
        };

        int caseNo = 0;
        // (A) Sweep every sound id with a fixed source/entity/vol/pitch/seed.
        for (int sid : soundIds) {
            if (sid < 0 || sid >= soundCount) continue;
            emit(registryAccess, "sound" + (caseNo++), sid, SoundSource.BLOCKS, 1, 1.0f, 1.0f, 0L);
        }
        // (B) Sweep every SoundSource ordinal at a fixed (small) sound id.
        for (SoundSource s : sources) {
            emit(registryAccess, "src" + (caseNo++), 0, s, 7, 1.0f, 1.0f, 1L);
        }
        // (C) Sweep entity ids (VarInt boundaries).
        for (int eid : entityIds) {
            emit(registryAccess, "ent" + (caseNo++), 1, SoundSource.HOSTILE, eid, 1.0f, 1.0f, 0L);
        }
        // (D) Sweep volume/pitch pairs.
        for (int i = 0; i < vols.length; i++) {
            emit(registryAccess, "vp" + (caseNo++), 2, SoundSource.NEUTRAL, 99, vols[i], pitches[i], 42L);
        }
        // (E) Sweep seeds.
        for (long sd : seeds) {
            emit(registryAccess, "seed" + (caseNo++), 0, SoundSource.MASTER, 0, 1.0f, 1.0f, sd);
        }
        // (F) Fully-mixed cases.
        emit(registryAccess, "mix" + (caseNo++), soundCount - 1, SoundSource.UI, Integer.MAX_VALUE,
             -1.0f, 3.4028235e38f, Long.MAX_VALUE);
        emit(registryAccess, "mix" + (caseNo++), 0, SoundSource.AMBIENT, Integer.MIN_VALUE,
             0.0f, -0.0f, Long.MIN_VALUE);
        emit(registryAccess, "mix" + (caseNo++), soundCount / 3, SoundSource.VOICE, -12345,
             0.123456f, 9.87654f, -1234567890L);
    }

    @SuppressWarnings("deprecation")
    static void emit(RegistryAccess registryAccess, String name,
                     int soundId, SoundSource source, int entityId,
                     float volume, float pitch, long seed) {
        // The reference Holder<SoundEvent> for this registry id (kind REFERENCE).
        SoundEvent value = BuiltInRegistries.SOUND_EVENT.byIdOrThrow(soundId);
        int gotId = BuiltInRegistries.SOUND_EVENT.getId(value);
        if (gotId != soundId) {
            throw new IllegalStateException("sound id mismatch: wanted " + soundId + " got " + gotId);
        }
        Holder<SoundEvent> holder = BuiltInRegistries.SOUND_EVENT.wrapAsHolder(value);
        if (holder.kind() != Holder.Kind.REFERENCE) {
            throw new IllegalStateException("expected REFERENCE holder for sound id " + soundId);
        }
        String soundKey = BuiltInRegistries.SOUND_EVENT.getKey(value).toString(); // ns:path

        // Replicate ClientboundSoundEntityPacket.write VERBATIM into a RegistryFriendlyByteBuf
        // (the public ctor needs an Entity; the body is the source of truth and is reproduced
        // exactly here, field for field, including SoundEvent.STREAM_CODEC for the holder).
        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), registryAccess);
        SoundEvent.STREAM_CODEC.encode(buf, holder); // VarInt(getId(value)+1) for REFERENCE
        buf.writeEnum(source);                       // VarInt(ordinal)
        buf.writeVarInt(entityId);
        buf.writeFloat(volume);
        buf.writeFloat(pitch);
        buf.writeLong(seed);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode the exact bytes through the SAME per-field reads (sanity).
        RegistryFriendlyByteBuf rbuf = new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())), registryAccess);
        Holder<SoundEvent> backHolder = SoundEvent.STREAM_CODEC.decode(rbuf);
        SoundSource backSource = rbuf.readEnum(SoundSource.class);
        int backEntityId = rbuf.readVarInt();
        float backVolume = rbuf.readFloat();
        float backPitch = rbuf.readFloat();
        long backSeed = rbuf.readLong();
        int backSoundId = BuiltInRegistries.SOUND_EVENT.getId(backHolder.value());
        if (backSoundId != soundId || backSource != source || backEntityId != entityId
            || Float.floatToRawIntBits(backVolume) != Float.floatToRawIntBits(volume)
            || Float.floatToRawIntBits(backPitch) != Float.floatToRawIntBits(pitch)
            || backSeed != seed || rbuf.readableBytes() != 0) {
            throw new IllegalStateException("round-trip mismatch for " + name
                + " sound=" + soundId + " src=" + source + " ent=" + entityId
                + " -> sound=" + backSoundId + " src=" + backSource + " ent=" + backEntityId
                + " trailing=" + rbuf.readableBytes());
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(soundId);                                       // registry id (wire = id+1)
        O.print('\t');
        O.print(soundKey);                                      // ns:path
        O.print('\t');
        O.print(source.ordinal());                              // SoundSource ordinal
        O.print('\t');
        O.print(entityId);                                      // VarInt entity id, decimal
        O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(volume)));
        O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(pitch)));
        O.print('\t');
        O.print(seed);                                          // signed 64-bit decimal
        O.print('\t');
        O.print(n);                                             // readableBytes
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
