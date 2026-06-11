// Ground truth for net.minecraft.network.protocol.common.ServerboundCustomClickActionPacket.
//
// Real class (26.1.2):
//   record ServerboundCustomClickActionPacket(Identifier id, Optional<Tag> payload)
//   STREAM_CODEC = StreamCodec.composite(
//        Identifier.STREAM_CODEC, ::id,
//        UNTRUSTED_TAG_CODEC,      ::payload, ::new)
//   where UNTRUSTED_TAG_CODEC =
//        ByteBufCodecs.optionalTagCodec(...).apply(ByteBufCodecs.lengthPrefixed(65536)).
//
// Wire (in STREAM_CODEC order — Packet.codec => no packet-id prefix, body only):
//   1) Identifier.STREAM_CODEC = STRING_UTF8.map(toString) -> Utf8String.write:
//          VarInt(utf8 byteLen) + UTF-8 bytes of "namespace:path".
//   2) payload via lengthPrefixed(65536) wrapping optionalTagCodec:
//          inner = FriendlyByteBuf.writeNbt(tag-or-null):
//                    null  -> single byte 0x00 (EndTag, == Java's null encoding)
//                    tag   -> type byte + UNNAMED network-NBT payload (NbtIo.writeAnyTag)
//          outer = VarInt(inner.length) + inner bytes.
//
// So the whole payload is exactly: Utf8(id) , VarInt(nbtLen) , nbtBytes.
//
// We do NOT hand-encode bytes Java-side: we build the REAL packet and let
// STREAM_CODEC.encode write everything, then dump the bytes as hex.
//
// To let the C++ side INDEPENDENTLY re-encode (not just echo our NBT bytes), each row
// carries the identifier string and a compact, deterministic TAG SPEC that BOTH sides
// rebuild into a structurally identical Tag, then encode through their own NBT writer:
//   none            -> Optional.empty()              (null tag -> 0x00)
//   str:<utf8hex>   -> StringTag.valueOf(...)
//   i8:<dec>        -> ByteTag.valueOf((byte))
//   i16:<dec>       -> ShortTag.valueOf((short))
//   i32:<dec>       -> IntTag.valueOf(int)
//   i64:<dec>       -> LongTag.valueOf(long)
//   f32:<rawbits>   -> FloatTag.valueOf(Float.intBitsToFloat(rawbits))
//   f64:<rawbits>   -> DoubleTag.valueOf(Double.longBitsToDouble(rawbits))
//   cmp1:<key-utf8hex>:<i32dec>  -> CompoundTag{ key -> IntTag } (single key: order trivial;
//                                   Java CompoundTag is a HashMap, so multi-key order is
//                                   NOT portable — we only ever use ONE key)
//   lstI:<n>        -> ListTag of n IntTags valueOf(0..n-1) (ListTag is an ArrayList: ordered)
//
// Row format (tab separated):
//   ENC <id-string> <tagSpec> <readableBytes-dec> <hex>
//
// Floats/doubles are carried as raw bits (never decimal) so the value is exact.
//
// The C++ pkt_custom_click_action_sb_parity rebuilds id+tag from <id-string,tagSpec>,
// re-encodes through PacketBuffer (writeString + lengthPrefixed NBT) and must match
// <hex> byte-for-byte (+ readableBytes).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.nio.charset.StandardCharsets;
import java.util.Optional;
import net.minecraft.SharedConstants;
import net.minecraft.nbt.ByteTag;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.DoubleTag;
import net.minecraft.nbt.FloatTag;
import net.minecraft.nbt.IntTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.LongTag;
import net.minecraft.nbt.ShortTag;
import net.minecraft.nbt.StringTag;
import net.minecraft.nbt.Tag;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ServerboundCustomClickActionPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;

public class PktCustomClickActionSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        StreamCodec<ByteBuf, ServerboundCustomClickActionPacket> CODEC =
            (StreamCodec<ByteBuf, ServerboundCustomClickActionPacket>)
                ServerboundCustomClickActionPacket.STREAM_CODEC;

        // Identifiers: default namespace omitted form ("foo") parses to minecraft:foo; we
        // also pass explicit namespaces. The wire string is always Identifier.toString()
        // == "namespace:path", so we emit THAT (canonical) string in the row.
        String[] ids = {
            "minecraft:foo",
            "minecraft:custom_action",
            "mymod:do_thing",
            "a:b",
            "minecraft:a_b/c.d-e",
        };

        // Tag specs covering: empty, every scalar tag type (incl. extreme/boundary values
        // carried as raw bits for floats), a single-key compound, and ordered int lists.
        String[] specs = {
            "none",
            "str:" + hex("".getBytes(StandardCharsets.UTF_8)),       // empty string
            "str:" + hex("hello".getBytes(StandardCharsets.UTF_8)),
            "str:" + hex("é中".getBytes(StandardCharsets.UTF_8)), // multibyte UTF-8
            "i8:0", "i8:127", "i8:-128", "i8:-1",
            "i16:0", "i16:32767", "i16:-32768", "i16:1234",
            "i32:0", "i32:1", "i32:-1", "i32:2147483647", "i32:-2147483648", "i32:305419896",
            "i64:0", "i64:1", "i64:-1",
            "i64:9223372036854775807", "i64:-9223372036854775808", "i64:1311768467463790320",
            "f32:" + Integer.toString(Float.floatToRawIntBits(0.0f)),
            "f32:" + Integer.toString(Float.floatToRawIntBits(-0.0f)),
            "f32:" + Integer.toString(Float.floatToRawIntBits(1.0f)),
            "f32:" + Integer.toString(Float.floatToRawIntBits(3.5f)),
            "f32:" + Integer.toString(Float.floatToRawIntBits(Float.NaN)),
            "f32:" + Integer.toString(Float.floatToRawIntBits(Float.POSITIVE_INFINITY)),
            "f64:" + Long.toString(Double.doubleToRawLongBits(0.0)),
            "f64:" + Long.toString(Double.doubleToRawLongBits(-0.0)),
            "f64:" + Long.toString(Double.doubleToRawLongBits(1.0)),
            "f64:" + Long.toString(Double.doubleToRawLongBits(2.718281828459045)),
            "f64:" + Long.toString(Double.doubleToRawLongBits(Double.NaN)),
            "f64:" + Long.toString(Double.doubleToRawLongBits(Double.NEGATIVE_INFINITY)),
            "cmp1:" + hex("count".getBytes(StandardCharsets.UTF_8)) + ":42",
            "cmp1:" + hex("x".getBytes(StandardCharsets.UTF_8)) + ":-7",
            "lstI:0", "lstI:1", "lstI:3", "lstI:8",
        };

        // Each id paired with a rotating spec, AND each spec paired with a rotating id, so
        // both the Utf8 and the NBT path exercise every variant in several combinations.
        for (int i = 0; i < ids.length; i++) {
            emit(CODEC, ids[i], specs[i % specs.length]);
        }
        for (int j = 0; j < specs.length; j++) {
            emit(CODEC, ids[j % ids.length], specs[j]);
        }
        // A few explicit corners.
        emit(CODEC, "minecraft:foo", "none");
        emit(CODEC, "a:b", "lstI:0");
        emit(CODEC, "mymod:do_thing", "cmp1:" + hex("k".getBytes(StandardCharsets.UTF_8)) + ":0");
    }

    static void emit(StreamCodec<ByteBuf, ServerboundCustomClickActionPacket> CODEC,
                     String idString, String spec) throws Exception {
        Identifier id = Identifier.parse(idString);
        // Re-derive the canonical wire string from the parsed Identifier so the row carries
        // exactly what Identifier.toString() will serialize.
        String canonical = id.toString();
        Optional<Tag> payload = buildTag(spec);

        ServerboundCustomClickActionPacket pkt = new ServerboundCustomClickActionPacket(id, payload);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundCustomClickActionPacket dec = CODEC.decode(rbuf);
        if (!dec.id().toString().equals(canonical) || !dec.payload().equals(payload)) {
            throw new IllegalStateException("round-trip mismatch for id=" + canonical + " spec=" + spec);
        }

        O.print("ENC\t");
        O.print(canonical);
        O.print('\t');
        O.print(spec);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
    }

    static Optional<Tag> buildTag(String spec) {
        if (spec.equals("none")) return Optional.empty();
        int c = spec.indexOf(':');
        String kind = c < 0 ? spec : spec.substring(0, c);
        String rest = c < 0 ? "" : spec.substring(c + 1);
        switch (kind) {
            case "str":  return Optional.of(StringTag.valueOf(new String(unhex(rest), StandardCharsets.UTF_8)));
            case "i8":   return Optional.of(ByteTag.valueOf((byte) Integer.parseInt(rest)));
            case "i16":  return Optional.of(ShortTag.valueOf((short) Integer.parseInt(rest)));
            case "i32":  return Optional.of(IntTag.valueOf(Integer.parseInt(rest)));
            case "i64":  return Optional.of(LongTag.valueOf(Long.parseLong(rest)));
            case "f32":  return Optional.of(FloatTag.valueOf(Float.intBitsToFloat(Integer.parseInt(rest))));
            case "f64":  return Optional.of(DoubleTag.valueOf(Double.longBitsToDouble(Long.parseLong(rest))));
            case "cmp1": {
                int c2 = rest.indexOf(':');
                String keyHex = rest.substring(0, c2);
                int v = Integer.parseInt(rest.substring(c2 + 1));
                CompoundTag ct = new CompoundTag();
                ct.put(new String(unhex(keyHex), StandardCharsets.UTF_8), IntTag.valueOf(v));
                return Optional.of(ct);
            }
            case "lstI": {
                int n = Integer.parseInt(rest);
                ListTag lt = new ListTag();
                for (int i = 0; i < n; i++) lt.add(IntTag.valueOf(i));
                return Optional.of(lt);
            }
            default: throw new IllegalArgumentException("bad spec: " + spec);
        }
    }

    static String hex(byte[] b) {
        StringBuilder sb = new StringBuilder();
        for (byte x : b) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
    }

    static String toHex(FriendlyByteBuf b) {
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
