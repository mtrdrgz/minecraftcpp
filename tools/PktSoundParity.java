// Ground truth for net.minecraft.network.protocol.game.ClientboundSoundPacket's
// StreamCodec. Verbatim from 26.1.2/src ClientboundSoundPacket.java:
//
//   STREAM_CODEC = Packet.codec(write, ctor)   -> no packet-id prefix, just the body.
//
//   write(RegistryFriendlyByteBuf out):                  (ClientboundSoundPacket.java 56-65)
//     SoundEvent.STREAM_CODEC.encode(out, this.sound);   // Holder<SoundEvent>
//     out.writeEnum(this.source);                        // SoundSource
//     out.writeInt(this.x);                              // (int)(x*8.0)  BE int
//     out.writeInt(this.y);                              // (int)(y*8.0)  BE int
//     out.writeInt(this.z);                              // (int)(z*8.0)  BE int
//     out.writeFloat(this.volume);                       // BE float
//     out.writeFloat(this.pitch);                        // BE float
//     out.writeLong(this.seed);                          // BE long
//
//   The ctor stores x = (int)(x*8.0), y = (int)(y*8.0), z = (int)(z*8.0)
//   (ClientboundSoundPacket.java 37-39; LOCATION_ACCURACY = 8.0F).
//
//   SoundEvent.STREAM_CODEC (SoundEvent.java:26):
//     ByteBufCodecs.holder(Registries.SOUND_EVENT, DIRECT_STREAM_CODEC)
//   holder(...) encode (ByteBufCodecs.java 603-613):
//     case REFERENCE: id = registry.getIdOrThrow(holder); VarInt.write(out, id + 1);
//     case DIRECT:    VarInt.write(out, 0); directCodec.encode(out, value);
//   Every SoundEvents.* / BuiltInRegistries.SOUND_EVENT.get(id) is a *registered
//   REFERENCE* holder, so the wire form is VarInt(getId + 1). getId is exactly
//   BuiltInRegistries.SOUND_EVENT.getId(value) — the network_registries.tsv id.
//
//   writeEnum(value) = writeVarInt(value.ordinal())   (FriendlyByteBuf:471-473)
//   SoundSource ordinals (declaration order, SoundSource.java 4-14):
//     MASTER=0 MUSIC=1 RECORDS=2 WEATHER=3 BLOCKS=4 HOSTILE=5 NEUTRAL=6
//     PLAYERS=7 AMBIENT=8 VOICE=9 UI=10
//
// Rows (tab separated):
//   ENUM  <ordinal> <name>                                   per SoundSource constant
//   ENC   <case> <soundName> <soundWireId> <srcOrdinal> <xi> <yi> <zi>
//         <volHex> <pitchHex> <seedDec> <readableBytes> <hexBytes>
//     soundName    = the SoundEvent registry entry ns:path (so C++ can resolve it)
//     soundWireId  = BuiltInRegistries.SOUND_EVENT.getId(value) (the +1 is NOT applied
//                    here; the codec adds it; emitted for cross-check only)
//     srcOrdinal   = SoundSource.ordinal()
//     xi/yi/zi     = the stored (int)(coord*8.0)  (decimal, signed)
//     volHex/pitchHex = float bits, %08x big-endian
//     seedDec      = signed long decimal
//
// The C++ side resolves soundName -> id via NetworkRegistries, writes VarInt(id+1)
// then enum/ints/floats/long in the SAME order via PacketBuffer, requires bytes-hex ==
// expected AND readableBytes match, then round-trips the bytes back.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSoundPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;

public class PktSoundParity {
    static final java.io.PrintStream O = System.out;
    static RegistryAccess REG;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The STREAM_CODEC is typed RegistryFriendlyByteBuf; the holder codec needs
        // registryAccess().lookupOrThrow(SOUND_EVENT).asHolderIdMap(). Build a real
        // RegistryAccess-backed buffer (same pattern as PktBlockUpdateParity) so the
        // typed codec is exercised faithfully — its ids ARE BuiltInRegistries ids.
        REG = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // Enum gate: dump ordinal()+name() for every SoundSource constant.
        for (SoundSource s : SoundSource.values()) {
            O.print("ENUM\t");
            O.print(s.ordinal());
            O.print('\t');
            O.print(s.name());
            O.print('\n');
        }

        // Battery of REAL registered sound events spanning the id space:
        //   id 0 (smallest), a few mid ids, and the LAST id (largest VarInt).
        // Pull the first and last by id to be exact about the boundaries.
        Holder.Reference<SoundEvent> firstHolder =
            BuiltInRegistries.SOUND_EVENT.get(0).orElseThrow();        // id 0
        int lastId = BuiltInRegistries.SOUND_EVENT.size() - 1;
        Holder.Reference<SoundEvent> lastHolder =
            BuiltInRegistries.SOUND_EVENT.get(lastId).orElseThrow();   // largest id

        String[] midNames = new String[] {
            "minecraft:block.anvil.land",
            "minecraft:entity.experience_orb.pickup",
            "minecraft:block.note_block.harp",
            "minecraft:ui.button.click",
            "minecraft:entity.lightning_bolt.thunder",
            "minecraft:entity.generic.explode",
        };

        // Source variety covering several SoundSource ordinals incl 0 and UI(10).
        SoundSource[] srcs = new SoundSource[] {
            SoundSource.MASTER, SoundSource.MUSIC, SoundSource.BLOCKS,
            SoundSource.HOSTILE, SoundSource.PLAYERS, SoundSource.UI,
        };

        // Coordinate / volume / pitch / seed battery: signs, boundaries, fixed-point.
        // x is a double; the packet stores (int)(x*8.0). Use values that exercise
        // negative truncation-toward-zero, fractional-eighths, and large magnitudes.
        double[] xs = { 0.0,   12.5,  -7.25,  100.125,  -0.001,  1000000.5 };
        double[] ys = { 64.0,  -3.5,  0.0,    255.875,  -2048.0, 0.0625 };
        double[] zs = { 0.0,  -64.5,  31.125, -0.0,      9999.999, -1.0 };
        float[]  vols   = { 1.0f, 0.0f, 0.5f,  2.0f,  100.0f, 0.001f };
        float[]  pitches= { 1.0f, 0.5f, 2.0f,  0.001f, 63.0f, 1.0f };
        long[]   seeds  = { 0L, 1L, -1L, Long.MIN_VALUE, Long.MAX_VALUE,
                            1234567890123456789L };

        // Case 0: smallest id (0), MASTER, zeros.
        emit("first_id0", firstHolder, SoundSource.MASTER,
             0.0, 64.0, 0.0, 1.0f, 1.0f, 0L);

        // Case 1: largest id, UI, extreme seed.
        emit("last_idmax", lastHolder, SoundSource.UI,
             100.125, 255.875, -0.0, 0.5f, 2.0f, Long.MIN_VALUE);

        // Mid sounds crossed with the coordinate/source/primitive battery.
        for (int i = 0; i < midNames.length; i++) {
            Identifier loc = Identifier.parse(midNames[i]);
            Holder.Reference<SoundEvent> h =
                BuiltInRegistries.SOUND_EVENT.get(loc).orElseThrow();
            SoundSource s = srcs[i % srcs.length];
            emit("mid_" + sanitize(midNames[i]), h, s,
                 xs[i], ys[i], zs[i], vols[i], pitches[i], seeds[i]);
        }

        // A few extra boundary rows reusing first/last holders to vary primitives.
        emit("neg_trunc", firstHolder, SoundSource.HOSTILE,
             -7.25, -3.5, -64.5, 0.001f, 0.001f, -1L);
        emit("big_coord", lastHolder, SoundSource.PLAYERS,
             1000000.5, -2048.0, 9999.999, 100.0f, 63.0f, Long.MAX_VALUE);
        emit("seed_max_long", firstHolder, SoundSource.NEUTRAL,
             1.0, 1.0, 1.0, 1.0f, 1.0f, 1234567890123456789L);
    }

    static void emit(String caseName, Holder<SoundEvent> sound, SoundSource source,
                     double x, double y, double z, float volume, float pitch, long seed)
            throws Exception {
        ClientboundSoundPacket pkt =
            new ClientboundSoundPacket(sound, source, x, y, z, volume, pitch, seed);

        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), REG);
        ClientboundSoundPacket.STREAM_CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hexBytes = toHex(buf);

        // Round-trip decode sanity from the wire bytes.
        RegistryFriendlyByteBuf rbuf =
            new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hexBytes)), REG);
        ClientboundSoundPacket dec = ClientboundSoundPacket.STREAM_CODEC.decode(rbuf);
        boolean ok =
            dec.getSource() == source &&
            (int)(x * 8.0) == (int)(dec.getX() * 8.0F) &&
            (int)(y * 8.0) == (int)(dec.getY() * 8.0F) &&
            (int)(z * 8.0) == (int)(dec.getZ() * 8.0F) &&
            Float.floatToIntBits(dec.getVolume()) == Float.floatToIntBits(volume) &&
            Float.floatToIntBits(dec.getPitch())  == Float.floatToIntBits(pitch) &&
            dec.getSeed() == seed;
        if (ok) {
            // The decoded holder must resolve to the same registry id.
            int wantId = BuiltInRegistries.SOUND_EVENT.getId(sound.value());
            int gotId  = BuiltInRegistries.SOUND_EVENT.getId(dec.getSound().value());
            ok = wantId == gotId;
        }
        if (!ok) throw new IllegalStateException("round-trip mismatch for " + caseName);

        String soundName = sound.unwrapKey().orElseThrow().identifier().toString();  // ResourceKey.identifier() (renamed from .location() in 26.1.2)
        int soundWireId  = BuiltInRegistries.SOUND_EVENT.getId(sound.value());
        int xi = (int)(x * 8.0);
        int yi = (int)(y * 8.0);
        int zi = (int)(z * 8.0);

        O.print("ENC\t");
        O.print(caseName);                 O.print('\t');
        O.print(soundName);                O.print('\t');
        O.print(soundWireId);              O.print('\t');
        O.print(source.ordinal());         O.print('\t');
        O.print(xi);                       O.print('\t');
        O.print(yi);                       O.print('\t');
        O.print(zi);                       O.print('\t');
        O.print(String.format("%08x", Float.floatToIntBits(volume)));  O.print('\t');
        O.print(String.format("%08x", Float.floatToIntBits(pitch)));   O.print('\t');
        O.print(seed);                     O.print('\t');
        O.print(readable);                 O.print('\t');
        O.print(hexBytes.isEmpty() ? "-" : hexBytes);
        O.print('\n');
    }

    static String sanitize(String s) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                sb.append(c);
            else sb.append('_');
        }
        return sb.toString();
    }

    static String toHex(ByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        if (s.equals("-")) return new byte[0];
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
