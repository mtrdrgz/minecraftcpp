// Ground truth for net.minecraft.network.protocol.game.ClientboundPlayerAbilitiesPacket.
//
// The packet body is exactly (real 26.1.2 source, ClientboundPlayerAbilitiesPacket
// lines 13-22, 33-64):
//   FLAG_INVULNERABLE = 1, FLAG_FLYING = 2, FLAG_CAN_FLY = 4, FLAG_INSTABUILD = 8
//   write :
//     byte bitfield = 0;
//     if (invulnerable) bitfield |= 1;
//     if (isFlying)     bitfield |= 2;
//     if (canFly)       bitfield |= 4;
//     if (instabuild)   bitfield |= 8;
//     FriendlyByteBuf.writeByte(bitfield)       // single raw byte
//     FriendlyByteBuf.writeFloat(this.flyingSpeed)   // raw int bits, big-endian
//     FriendlyByteBuf.writeFloat(this.walkingSpeed)  // raw int bits, big-endian
//   read :
//     byte bitfield = input.readByte();
//     invulnerable = (bitfield & 1) != 0;
//     isFlying     = (bitfield & 2) != 0;
//     canFly       = (bitfield & 4) != 0;
//     instabuild   = (bitfield & 8) != 0;
//     flyingSpeed  = input.readFloat();
//     walkingSpeed = input.readFloat();
// STREAM_CODEC = Packet.codec(write, new) -> StreamCodec.ofMember: NO packet-id prefix,
// just the body. So the wire payload is: 1B bitfield + 4B float + 4B float = 9 bytes.
//
// The ONLY public ctor takes net.minecraft.world.entity.player.Abilities; we build a
// real Abilities, set its public boolean fields + flyingSpeed/walkingSpeed (via the
// public setters), then construct the packet through that public ctor.
//
// Row format (tab separated), TAG = ENC:
//   ENC <name-hex> <invuln-dec> <flying-dec> <canfly-dec> <instabuild-dec>
//       <flySpeedBits-08x> <walkSpeedBits-08x> <readableBytes-dec> <hex>
// where name is a lowercase-hex label, the four booleans are 0/1 decimal, the float
// bits are Float.floatToRawIntBits(...) as lowercase 8-hex (the EXACT big-endian wire
// bytes), readableBytes is decimal, and <hex> is the full packet payload lowercase hex.
//
// Every case is round-trip-decoded through the SAME codec and asserted bit-exact before
// emit. The C++ pkt_player_abilities_parity rebuilds the bitfield + floats from these
// fields, re-encodes via PacketBuffer (writeByte/writeFloat/writeFloat), and must match
// <hex> byte-for-byte (+ readableBytes); it also decodes <hex> and checks the recovered
// flag bits + floats bit-exactly.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundPlayerAbilitiesPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.entity.player.Abilities;

public class PktPlayerAbilitiesParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundPlayerAbilitiesPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundPlayerAbilitiesPacket>)
                ClientboundPlayerAbilitiesPacket.STREAM_CODEC;

        // flyingSpeed / walkingSpeed are plain writeFloat so we pin signs, fractions,
        // zero, negative-zero and finite extremes to stress the raw-bits path (vanilla
        // defaults are 0.05f / 0.1f).
        float[] floats = {
            0.0f, -0.0f, 1.0f, -1.0f,
            0.05f, 0.1f,                      // vanilla DEFAULT_FLYING_SPEED / WALKING
            0.025f, 0.2f, 0.5f, 19.5f,
            3.4028235E38f, -3.4028235E38f,    // Float.MAX_VALUE / -MAX_VALUE
            1.4E-45f,                         // Float.MIN_VALUE (smallest +subnormal)
            0.123456f, -0.123456f, 123456.78f, -123456.78f,
            2.0f, 4.0f, 16.0f
        };

        // (A) Enumerate ALL 16 flag combinations (4 independent bits) with rotating
        //     finite float pairs, so every bitfield value 0..15 is exercised.
        int n = floats.length;
        for (int mask = 0; mask < 16; mask++) {
            boolean invuln = (mask & 1) != 0;
            boolean flying = (mask & 2) != 0;
            boolean canFly = (mask & 4) != 0;
            boolean insta  = (mask & 8) != 0;
            float fly  = floats[mask % n];
            float walk = floats[(mask + 5) % n];
            emit(CODEC, "mask" + mask, invuln, flying, canFly, insta, fly, walk);
        }

        // (B) Cross product of the four interesting boolean states (all-false / all-true)
        //     x float extremes to exercise the float-bit path independently of the flags.
        boolean[][] flagSets = {
            {false, false, false, false},   // all off
            {true,  true,  true,  true},    // all on
            {true,  false, true,  false},   // invuln + canFly (typical creative idle)
            {false, true,  true,  false}    // flying + canFly (creative in-flight)
        };
        float[] keyFloats = { 0.0f, -0.0f, 0.05f, 0.1f, -1.0f, 3.4028235E38f, 1.4E-45f };
        for (boolean[] fs : flagSets) {
            for (float fv : keyFloats) {
                emit(CODEC, "x", fs[0], fs[1], fs[2], fs[3], fv, fv);
            }
        }

        // (C) Hand-picked exact vanilla states.
        emit(CODEC, "survival",  false, false, false, false, 0.05f, 0.1f); // default
        emit(CODEC, "creative",  true,  false, true,  true,  0.05f, 0.1f); // creative
        emit(CODEC, "flying",    true,  true,  true,  true,  0.05f, 0.1f); // creative flying
    }

    static void emit(StreamCodec<FriendlyByteBuf, ClientboundPlayerAbilitiesPacket> CODEC,
                     String name, boolean invuln, boolean flying, boolean canFly,
                     boolean insta, float flySpeed, float walkSpeed) {
        // Build a real Abilities (public boolean fields + public speed setters) and
        // construct the packet through its only public ctor.
        Abilities ab = new Abilities();
        ab.invulnerable = invuln;
        ab.flying = flying;
        ab.mayfly = canFly;
        ab.instabuild = insta;
        ab.setFlyingSpeed(flySpeed);
        ab.setWalkingSpeed(walkSpeed);

        ClientboundPlayerAbilitiesPacket pkt = new ClientboundPlayerAbilitiesPacket(ab);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec; assert bit-exact equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundPlayerAbilitiesPacket dec = CODEC.decode(rbuf);
        if (dec.isInvulnerable() != invuln
            || dec.isFlying() != flying
            || dec.canFly() != canFly
            || dec.canInstabuild() != insta
            || Float.floatToRawIntBits(dec.getFlyingSpeed()) != Float.floatToRawIntBits(flySpeed)
            || Float.floatToRawIntBits(dec.getWalkingSpeed()) != Float.floatToRawIntBits(walkSpeed)) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + invuln + "," + flying + "," + canFly + ","
                + insta + "," + flySpeed + "," + walkSpeed + ") out=("
                + dec.isInvulnerable() + "," + dec.isFlying() + "," + dec.canFly() + ","
                + dec.canInstabuild() + "," + dec.getFlyingSpeed() + "," + dec.getWalkingSpeed() + ")");
        }

        O.print("ENC\t");
        O.print(toHexStr(name));               // name as lowercase hex (label column)
        O.print('\t');
        O.print(invuln ? 1 : 0);
        O.print('\t');
        O.print(flying ? 1 : 0);
        O.print('\t');
        O.print(canFly ? 1 : 0);
        O.print('\t');
        O.print(insta ? 1 : 0);
        O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(flySpeed)));
        O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(walkSpeed)));
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
    }

    static String toHex(FriendlyByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static String toHexStr(String s) {
        StringBuilder sb = new StringBuilder();
        for (byte by : s.getBytes(java.nio.charset.StandardCharsets.UTF_8))
            sb.append(String.format("%02x", by & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
