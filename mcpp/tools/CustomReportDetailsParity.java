// Ground truth for net.minecraft.network.protocol.common.ClientboundCustomReportDetailsPacket.
//
// 26.1.2 wire format — verified VERBATIM against the REAL source:
//   record ClientboundCustomReportDetailsPacket(Map<String,String> details)
//   STREAM_CODEC = StreamCodec.composite(DETAILS_STREAM_CODEC, ::details, ::new)
//        -> body only, NO packet-id prefix.
//   DETAILS_STREAM_CODEC = ByteBufCodecs.map(HashMap::new,
//                              ByteBufCodecs.stringUtf8(128),     // key,   maxLength 128
//                              ByteBufCodecs.stringUtf8(4096),    // value, maxLength 4096
//                              32)                                // MAX_DETAIL_COUNT
//   ByteBufCodecs.map.encode (ByteBufCodecs.java:460-466):
//        writeCount(output, map.size(), maxSize);   // == VarInt.write(size)   (plain LEB128)
//        map.forEach((k, v) -> { keyCodec.encode(k); valueCodec.encode(v); });
//   stringUtf8(n).encode -> Utf8String.write(output, value, n) (Utf8String.java:35-55):
//        VarInt.write(output, utf8ByteLen);  output.writeBytes(utf8Bytes);
//
//   So the wire is exactly:
//        VarInt(size)
//        for each entry in MAP ITERATION ORDER:
//            VarInt(utf8ByteLen(key))   key UTF-8 bytes
//            VarInt(utf8ByteLen(value)) value UTF-8 bytes
//
//   We construct the packet with a LinkedHashMap so map.forEach() (and our TSV dump)
//   both follow INSERTION ORDER deterministically; the encoded byte order then equals
//   the TSV entry order, which the C++ side replays through mc::net::PacketBuffer.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <size> <k0HexUtf8> <v0HexUtf8> ... <k{n-1}HexUtf8> <v{n-1}HexUtf8> <bytesN> <hexBytes>
// Keys/values are carried as raw-UTF-8 hex (lowercase, "" -> empty field is impossible in
// TSV so empty strings are emitted as the literal token "_" ; the C++ side maps "_" -> "").
// The C++ side INDEPENDENTLY re-encodes writeVarInt(size) + per-entry writeUtf(k)+writeUtf(v).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import java.util.LinkedHashMap;
import java.util.Map;

import net.minecraft.SharedConstants;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.common.ClientboundCustomReportDetailsPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.core.RegistryAccess;

@SuppressWarnings({"unchecked", "deprecation"})
public class CustomReportDetailsParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        int caseNo = 0;

        // empty map -> just VarInt(0)
        emit("empty" + (caseNo++), ordered());

        // single entry, plain ASCII
        emit("single" + (caseNo++), ordered("key", "value"));

        // empty key and/or empty value (legal: VarInt(0) length prefix, no bytes)
        emit("emptyKey" + (caseNo++), ordered("", "v"));
        emit("emptyVal" + (caseNo++), ordered("k", ""));
        emit("emptyBoth" + (caseNo++), ordered("", ""));

        // a few entries, insertion order preserved by LinkedHashMap
        emit("trio" + (caseNo++), ordered("a", "1", "bb", "22", "ccc", "333"));

        // value crossing the 127-byte VarInt boundary (128 'x' -> VarInt 2-byte length 0x80 0x01)
        emit("len127" + (caseNo++), ordered("k", repeat('x', 127)));
        emit("len128" + (caseNo++), ordered("k", repeat('x', 128)));
        emit("len300" + (caseNo++), ordered("k", repeat('x', 300)));

        // multi-byte UTF-8: byteLen != charLen.
        //   "é"     U+00E9 -> 2 bytes (c3 a9)
        //   "€"     U+20AC -> 3 bytes (e2 82 ac)
        //   "𝄞"     U+1D11E -> 4 bytes (f0 9d 84 9e), 2 UTF-16 units
        emit("utf8" + (caseNo++), ordered("é", "€", "abc𝄞", "naïve €café"));

        // all 0x00..0x7f ASCII bytes as a value (exercises every single-byte code unit)
        StringBuilder ascii = new StringBuilder();
        for (int i = 0; i < 0x80; i++) ascii.append((char) i);
        emit("asciiAll" + (caseNo++), ordered("ascii", ascii.toString()));

        // MAX_DETAIL_COUNT = 32 entries (size VarInt still 1 byte; exercises the count loop)
        LinkedHashMap<String, String> big = new LinkedHashMap<>();
        for (int i = 0; i < 32; i++) big.put("k" + i, "val-" + (i * 7));
        emit("max32" + (caseNo++), big);
    }

    static String repeat(char c, int n) {
        StringBuilder sb = new StringBuilder(n);
        for (int i = 0; i < n; i++) sb.append(c);
        return sb.toString();
    }

    // Build a LinkedHashMap preserving the given k,v,k,v,... insertion order.
    static LinkedHashMap<String, String> ordered(String... kv) {
        LinkedHashMap<String, String> m = new LinkedHashMap<>();
        for (int i = 0; i + 1 < kv.length; i += 2) m.put(kv[i], kv[i + 1]);
        return m;
    }

    static String hexUtf8(String s) {
        if (s.isEmpty()) return "_";
        byte[] b = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (byte by : b) sb.append(String.format("%02x", by));
        return sb.toString();
    }

    static void emit(String name, Map<String, String> details) {
        ClientboundCustomReportDetailsPacket pkt = new ClientboundCustomReportDetailsPacket(details);

        // STREAM_CODEC is StreamCodec<ByteBuf, ...>; a RegistryFriendlyByteBuf is a ByteBuf and
        // needs no registry access for this packet (pure string map). Use it for generality.
        RegistryFriendlyByteBuf buf =
            new RegistryFriendlyByteBuf(Unpooled.buffer(), RegistryAccess.EMPTY);
        ClientboundCustomReportDetailsPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): the map must survive byte-for-byte.
        RegistryFriendlyByteBuf rb =
            new RegistryFriendlyByteBuf(Unpooled.copiedBuffer(buf), RegistryAccess.EMPTY);
        ClientboundCustomReportDetailsPacket back =
            ClientboundCustomReportDetailsPacket.STREAM_CODEC.decode(rb);
        if (!back.details().equals(details)) {
            throw new IllegalStateException("round-trip map mismatch for " + name + ": "
                + back.details() + " != " + details);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(details.size());
        // entries in MAP ITERATION ORDER (== insertion order for our LinkedHashMaps).
        for (Map.Entry<String, String> e : details.entrySet()) {
            O.print('\t');
            O.print(hexUtf8(e.getKey()));
            O.print('\t');
            O.print(hexUtf8(e.getValue()));
        }
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
