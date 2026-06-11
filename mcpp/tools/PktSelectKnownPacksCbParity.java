// Ground truth for net.minecraft.network.protocol.configuration.ClientboundSelectKnownPacks's
// StreamCodec.
//
// The packet is a record(List<KnownPack> knownPacks)
// (ClientboundSelectKnownPacks.java:11-14). Its STREAM_CODEC is StreamCodec.composite over,
// in this exact wire order:
//   KnownPack.STREAM_CODEC.apply(ByteBufCodecs.list()) -> knownPacks
// ByteBufCodecs.list() is ByteBufCodecs.collection(ArrayList::new, KnownPack.STREAM_CODEC)
// (ByteBufCodecs.java:406-412 -> 413-435): it writes writeCount(size) = VarInt(size)
// (ByteBufCodecs.java:399-405), then encodes each element in order. (The clientbound list()
// has no max-size cap, unlike the serverbound list(64); the *encoded bytes are identical*.)
// Each KnownPack.STREAM_CODEC (KnownPack.java:9-11) is StreamCodec.composite over:
//   ByteBufCodecs.STRING_UTF8 -> namespace
//   ByteBufCodecs.STRING_UTF8 -> id
//   ByteBufCodecs.STRING_UTF8 -> version
// STRING_UTF8 = stringUtf8(32767) (ByteBufCodecs.java:168,267-277) -> Utf8String.write
// (Utf8String.java:35-55): VarInt(utf8ByteLen) then the UTF-8 bytes.
//
// So the body is exactly: VarInt(numPacks) then per pack three Utf8 strings
// (VarInt(byteLen)+UTF-8) in namespace,id,version order. No registry/ItemStack/
// Component/Holder/NBT, so the body is fully representable by the certified
// PacketBuffer (FriendlyByteBuf) port. Packet codec -> no packet-id prefix, just the body.
//
// Row format (tab separated). Strings are carried as hex of their raw UTF-8 bytes so any
// byte content survives without text/whitespace ambiguity. hex columns are lowercase hex:
//   ENC <numPacks> [<nsHex> <idHex> <verHex>]... <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
// The decode round-trip is a sanity check (abort on any drift so no bad row is emitted).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.configuration.ClientboundSelectKnownPacks;
import net.minecraft.server.packs.repository.KnownPack;

public class PktSelectKnownPacksCbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (ByteBuf, packet).
        StreamCodec<ByteBuf, ClientboundSelectKnownPacks> CODEC =
            (StreamCodec<ByteBuf, ClientboundSelectKnownPacks>)
                ClientboundSelectKnownPacks.STREAM_CODEC;

        // Empty list (just a VarInt 0).
        emit(CODEC, new ArrayList<>());

        // Single pack with empty strings (three VarInt 0 length prefixes).
        emit(CODEC, List.of(new KnownPack("", "", "")));

        // Single vanilla-style pack.
        emit(CODEC, List.of(new KnownPack("minecraft", "core", "26.1.2")));

        // Mixed-length ASCII strings to exercise multiple distinct VarInt length prefixes.
        emit(CODEC, List.of(new KnownPack("a", "bb", "ccc")));

        // Several packs to exercise the list VarInt(size) and ordering.
        emit(CODEC, List.of(
            new KnownPack("minecraft", "core", "26.1.2"),
            new KnownPack("minecraft", "data", "26.1.2"),
            new KnownPack("examplemod", "main", "1.0.0")));

        // Non-ASCII (multi-byte UTF-8) to verify VarInt(byteLen) is byte count, not char
        // count: "é" is 2 bytes, "你好" is 6 bytes, "🧊" is 4 bytes.
        emit(CODEC, List.of(new KnownPack("é", "你好", "🧊")));

        // A pack with a long-ish string crossing the 1-byte VarInt boundary (>=128 bytes
        // -> 2-byte VarInt length). 200 'x' chars = 200 bytes.
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 200; i++) sb.append('x');
        emit(CODEC, List.of(new KnownPack("ns", sb.toString(), "v")));

        // A larger list (128 packs) to push the list-size VarInt across the 1->2 byte
        // boundary (the clientbound list() has no 64 cap).
        List<KnownPack> big = new ArrayList<>();
        for (int i = 0; i < 128; i++) big.add(new KnownPack("mod" + i, "id" + i, "v" + i));
        emit(CODEC, big);
    }

    static void emit(StreamCodec<ByteBuf, ClientboundSelectKnownPacks> CODEC, List<KnownPack> packs) {
        ClientboundSelectKnownPacks pkt = new ClientboundSelectKnownPacks(packs);

        // ENC: encode through the REAL codec, dump readableBytes + body bytes.
        ByteBuf buf = Unpooled.buffer();
        CODEC.encode(buf, pkt);
        int n = buf.readableBytes();
        String hex = toHex(buf);

        StringBuilder line = new StringBuilder();
        line.append("ENC\t").append(packs.size());
        for (KnownPack p : packs) {
            line.append('\t').append(strHex(p.namespace()));
            line.append('\t').append(strHex(p.id()));
            line.append('\t').append(strHex(p.version()));
        }
        line.append('\t').append(n).append('\t').append(hex);
        O.print(line);
        O.print('\n');

        // DEC: decode the same bytes through the REAL codec; round-trip sanity (abort on
        // any drift so no bad row is ever emitted).
        ByteBuf rbuf = Unpooled.wrappedBuffer(unhex(hex));
        ClientboundSelectKnownPacks dec = CODEC.decode(rbuf);
        List<KnownPack> dp = dec.knownPacks();
        if (dp.size() != packs.size()) {
            throw new IllegalStateException("round-trip size mismatch: in=" + packs.size() + " out=" + dp.size());
        }
        for (int i = 0; i < packs.size(); i++) {
            KnownPack a = packs.get(i);
            KnownPack b = dp.get(i);
            if (!a.namespace().equals(b.namespace()) || !a.id().equals(b.id()) || !a.version().equals(b.version())) {
                throw new IllegalStateException("round-trip element mismatch at " + i + ": in=" + a + " out=" + b);
            }
        }
    }

    static String strHex(String s) {
        byte[] b = s.getBytes(StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder();
        for (byte x : b) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
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
