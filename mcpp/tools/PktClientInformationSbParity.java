// Ground truth for net.minecraft.network.protocol.common.ServerboundClientInformationPacket.
//
// The packet is a record wrapping a single ClientInformation record. Its
// STREAM_CODEC (Packet.codec(write, new)) delegates entirely to
// ClientInformation.write / new ClientInformation(FriendlyByteBuf). No
// packet-type id is part of the codec bytes (that framing lives outside the
// StreamCodec).
//
// Field order is VERBATIM from ClientInformation.write
// (26.1.2/src/net/minecraft/server/level/ClientInformation.java:35-45):
//     output.writeUtf(this.language);                 // VarInt(byteLen)+UTF-8 bytes, maxLen 16
//     output.writeByte(this.viewDistance);            // 1 byte (Netty writeByte: low 8 bits of int)
//     output.writeEnum(this.chatVisibility);          // VarInt(ordinal)
//     output.writeBoolean(this.chatColors);           // 1 byte
//     output.writeByte(this.modelCustomisation);      // 1 byte (low 8 bits)
//     output.writeEnum(this.mainHand);                // VarInt(ordinal)
//     output.writeBoolean(this.textFilteringEnabled); // 1 byte
//     output.writeBoolean(this.allowsListing);        // 1 byte
//     output.writeEnum(this.particleStatus);          // VarInt(ordinal)
//
// Read side (ClientInformation(FriendlyByteBuf) lines 21-33):
//     readUtf(16), readByte(), readEnum(ChatVisiblity), readBoolean(),
//     readUnsignedByte(), readEnum(HumanoidArm), readBoolean(), readBoolean(),
//     readEnum(ParticleStatus).
//   -> viewDistance round-trips through a SIGNED byte (readByte),
//      modelCustomisation round-trips through an UNSIGNED byte (readUnsignedByte).
//
// Enum declaration orders (== ordinal, the writeEnum/readEnum index):
//   ChatVisiblity: FULL=0, SYSTEM=1, HIDDEN=2     (ChatVisiblity.java:9-11)
//   HumanoidArm:   LEFT=0, RIGHT=1                (HumanoidArm.java:13-14)
//   ParticleStatus:ALL=0, DECREASED=1, MINIMAL=2  (ParticleStatus.java:9-11)
//
// We construct the REAL packet, encode through its STREAM_CODEC into a fresh
// FriendlyByteBuf, dump readableBytes() + raw hex, then decode back through the
// SAME codec (sanity round-trip on every field).
//
// Row: ENC \t name \t languageHex \t viewDistance \t chatVisOrd \t chatColors
//        \t modelCust \t mainHandOrd \t textFilter \t allowsListing \t particleOrd
//        \t readableBytes \t hex
//   languageHex: UTF-8 bytes of language as lowercase hex (ASCII-safe TSV transport).
//   viewDistance/modelCust: decimal ints (as constructed); the C++ side masks to
//     the byte the codec actually wrote and reads it back with the right signedness.
//   ordinals/bools: decimal.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ServerboundClientInformationPacket;
import net.minecraft.server.level.ClientInformation;
import net.minecraft.server.level.ParticleStatus;
import net.minecraft.world.entity.HumanoidArm;
import net.minecraft.world.entity.player.ChatVisiblity;

public class PktClientInformationSbParity {
    static final java.io.PrintStream O = System.out;

    static String utf8Hex(String s) {
        StringBuilder b = new StringBuilder();
        for (byte bb : s.getBytes(java.nio.charset.StandardCharsets.UTF_8))
            b.append(String.format("%02x", bb));
        return b.toString();
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundClientInformationPacket> codec =
            ServerboundClientInformationPacket.STREAM_CODEC;

        ChatVisiblity[] cv = ChatVisiblity.values();   // FULL,SYSTEM,HIDDEN
        HumanoidArm[] arm = HumanoidArm.values();       // LEFT,RIGHT
        ParticleStatus[] ps = ParticleStatus.values();  // ALL,DECREASED,MINIMAL

        // multi-byte UTF-8 language (still <= 16 UTF-16 chars; codec maxLen is 16):
        String uni = "niño";       // "niño" -> 5 UTF-8 bytes, 4 chars
        String cjk = "中文";    // "中文" -> 6 UTF-8 bytes, 2 chars

        // Each case: language, viewDistance, ChatVisiblity, chatColors,
        // modelCustomisation, HumanoidArm, textFilteringEnabled, allowsListing, ParticleStatus.
        // viewDistance/modelCustomisation kept within the byte the codec writes;
        // viewDistance exercises sign (readByte), modelCustomisation unsigned bits.
        Object[][] cases = {
            // default (vanilla createDefault)
            { "default",      "en_us", 2,   cv[0], true,  0,   arm[1], false, false, ps[0] },
            // empty language string (VarInt len 0)
            { "empty_lang",   "",      0,   cv[0], false, 0,   arm[0], false, false, ps[0] },
            // all max enum ordinals
            { "max_enums",    "en_us", 32,  cv[2], true,  0,   arm[1], true,  true,  ps[2] },
            // chat SYSTEM, particle DECREASED, left hand
            { "mid_enums",    "fr_fr", 16,  cv[1], false, 0,   arm[0], true,  false, ps[1] },
            // viewDistance signed-byte negative round-trip (readByte -> signed)
            { "vd_neg1",      "en_gb", -1,  cv[0], true,  0,   arm[1], false, true,  ps[0] },
            { "vd_min",       "en_gb", -128,cv[0], false, 0,   arm[0], true,  false, ps[2] },
            { "vd_max",       "en_gb", 127, cv[2], true,  0,   arm[1], false, false, ps[1] },
            // modelCustomisation high bits (unsigned byte, all 7 model-part flags set = 0x7f)
            { "model_7f",     "en_us", 10,  cv[0], true,  127, arm[1], true,  true,  ps[0] },
            { "model_ff",     "en_us", 10,  cv[1], false, 255, arm[0], false, false, ps[2] },
            { "model_80",     "en_us", 10,  cv[2], true,  128, arm[1], true,  false, ps[1] },
            // boolean coverage: every bool toggled
            { "bools_TFT",    "en_us", 4,   cv[0], true,  1,   arm[0], false, true,  ps[0] },
            { "bools_FTF",    "en_us", 4,   cv[1], false, 1,   arm[1], true,  false, ps[1] },
            // multi-byte UTF-8 languages
            { "lang_unicode", uni,     8,   cv[0], true,  0,   arm[1], false, false, ps[0] },
            { "lang_cjk",     cjk,     8,   cv[2], false, 0,   arm[0], true,  true,  ps[2] },
            // 16-char language (codec maxLen boundary, all ASCII -> 16 bytes, 1-byte VarInt len)
            { "lang_len16",   "abcdefghijklmnop", 12, cv[1], true, 0, arm[1], false, false, ps[1] },
        };

        for (Object[] c : cases) {
            String name   = (String) c[0];
            String lang   = (String) c[1];
            int viewDist  = (Integer) c[2];
            ChatVisiblity chatVis = (ChatVisiblity) c[3];
            boolean chatColors = (Boolean) c[4];
            int modelCust = (Integer) c[5];
            HumanoidArm mainHand = (HumanoidArm) c[6];
            boolean textFilter = (Boolean) c[7];
            boolean allowsListing = (Boolean) c[8];
            ParticleStatus particle = (ParticleStatus) c[9];

            ClientInformation info = new ClientInformation(
                lang, viewDist, chatVis, chatColors, modelCust,
                mainHand, textFilter, allowsListing, particle);
            ServerboundClientInformationPacket pkt = new ServerboundClientInformationPacket(info);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec; verify every field.
            ServerboundClientInformationPacket back = codec.decode(buf);
            ClientInformation bi = back.information();
            // viewDistance round-trips through a signed byte:
            int expViewDist = (byte) viewDist;
            // modelCustomisation round-trips through an unsigned byte:
            int expModelCust = modelCust & 0xff;
            if (!bi.language().equals(lang)
                || bi.viewDistance() != expViewDist
                || bi.chatVisibility() != chatVis
                || bi.chatColors() != chatColors
                || bi.modelCustomisation() != expModelCust
                || bi.mainHand() != mainHand
                || bi.textFilteringEnabled() != textFilter
                || bi.allowsListing() != allowsListing
                || bi.particleStatus() != particle) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            O.print("ENC\t");
            O.print(name);                 O.print('\t');
            O.print(utf8Hex(lang));        O.print('\t');
            O.print(viewDist);             O.print('\t');
            O.print(chatVis.ordinal());    O.print('\t');
            O.print(chatColors ? 1 : 0);   O.print('\t');
            O.print(modelCust);            O.print('\t');
            O.print(mainHand.ordinal());   O.print('\t');
            O.print(textFilter ? 1 : 0);   O.print('\t');
            O.print(allowsListing ? 1 : 0);O.print('\t');
            O.print(particle.ordinal());   O.print('\t');
            O.print(readable);             O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
