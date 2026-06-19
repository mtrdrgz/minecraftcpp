// Ground truth for net.minecraft.network.protocol.game.ClientboundCustomChatCompletionsPacket's
// StreamCodec. Verbatim from 26.1.2/src ClientboundCustomChatCompletionsPacket.java:
//
//   public record ClientboundCustomChatCompletionsPacket(Action action, List<String> entries)
//   STREAM_CODEC = Packet.codec(write, ctor)   -> no packet-id prefix, just the body.
//
//   write(FriendlyByteBuf out):                          (lines 19-22)
//     out.writeEnum(this.action);
//     out.writeCollection(this.entries, FriendlyByteBuf::writeUtf);
//
//   read(FriendlyByteBuf in):                            (lines 15-17)
//     this(in.readEnum(Action.class), in.readList(FriendlyByteBuf::readUtf));
//
//   writeEnum(value)       = writeVarInt(value.ordinal())            (FriendlyByteBuf:471-473)
//   writeCollection(c,enc) = writeVarInt(c.size()); for e: enc(out,e) (FriendlyByteBuf:134-140)
//   writeUtf(s)            = Utf8String.write -> VarInt(byteLen)+UTF-8 bytes
//                                                                  (FriendlyByteBuf:572-579, Utf8String:35-55)
//
//   Action ordinals (declaration order, ClientboundCustomChatCompletionsPacket.java 33-37):
//     ADD=0  REMOVE=1  SET=2
//
// Rows (tab separated):
//   ENUM  <ordinal> <name>                        per Action constant (enum gate)
//   ENC   <name> <actionOrdinal> <count> <entriesHex...> <readableBytes> <hexBytes>
//         actionOrdinal/count are decimal; entriesHex is a comma-separated list of the
//         lowercase-hex of each entry's UTF-8 bytes ("-" when the list is empty).
//
// The C++ side reconstructs the same packet from these columns, writes the SAME fields
// via PacketBuffer in the SAME codec order, and requires bytes-hex == expected AND
// readableBytes match, then round-trips the bytes back through PacketBuffer.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundCustomChatCompletionsPacket;
import net.minecraft.network.protocol.game.ClientboundCustomChatCompletionsPacket.Action;

public class PktCustomChatCompletionsParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        @SuppressWarnings("unchecked")
        StreamCodec<FriendlyByteBuf, ClientboundCustomChatCompletionsPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundCustomChatCompletionsPacket>)
                ClientboundCustomChatCompletionsPacket.STREAM_CODEC;

        // Enum gate: dump ordinal()+name() for every Action constant.
        for (Action a : Action.values()) {
            O.print("ENUM\t");
            O.print(a.ordinal());
            O.print('\t');
            O.print(a.name());
            O.print('\n');
        }

        // Battery of finite/physical cases. Entries are arbitrary strings (chat
        // completions are free-form player/command names), so we exercise:
        //   - empty list (count VarInt 0)
        //   - single ASCII entry
        //   - several entries (count VarInt boundary 127->128 not reached; small counts)
        //   - multibyte UTF-8 entries (2/3/4-byte code points) -> writeUtf byte length
        //   - an entry > 127 UTF-8 bytes -> writeUtf VarInt length boundary 1->2 bytes
        //   - a count that crosses the VarInt 1->2-byte boundary (128 entries)
        //   - empty-string entry (writeUtf VarInt(0) + no bytes)
        // Crossed with each Action.

        String longEntry = repeat("a", 200);              // > 127 UTF-8 bytes -> 2-byte VarInt len
        String multi2 = "éüñ";             // Latin-1 supplement, 2 bytes each
        String multi3 = "中文あ";             // CJK / Hiragana, 3 bytes each
        String multi4 = new String(Character.toChars(0x1F600)) + new String(Character.toChars(0x1F4A9)); // emoji, 4 bytes each

        // For each Action, the same set of list shapes.
        for (Action a : Action.values()) {
            String p = a.name();
            emit(CODEC, p + "_empty",        a, new ArrayList<String>());
            emit(CODEC, p + "_one_ascii",    a, listOf("Steve"));
            emit(CODEC, p + "_empty_string", a, listOf(""));
            emit(CODEC, p + "_three",        a, listOf("alpha", "Bravo_99", "x"));
            emit(CODEC, p + "_multi2",       a, listOf(multi2));
            emit(CODEC, p + "_multi3",       a, listOf(multi3));
            emit(CODEC, p + "_multi4",       a, listOf(multi4));
            emit(CODEC, p + "_mixed",        a, listOf("ascii", multi2, multi3, multi4, ""));
            emit(CODEC, p + "_long",         a, listOf(longEntry));
            emit(CODEC, p + "_long_plus",    a, listOf("a", longEntry, "b"));

            // count VarInt boundary: 127 entries (1-byte VarInt count) and 128 (2-byte).
            emit(CODEC, p + "_count127",     a, repeatList("e", 127));
            emit(CODEC, p + "_count128",     a, repeatList("e", 128));
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ClientboundCustomChatCompletionsPacket> CODEC,
                     String caseName, Action action, List<String> entries) throws Exception {
        ClientboundCustomChatCompletionsPacket pkt =
            new ClientboundCustomChatCompletionsPacket(action, entries);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hexBytes = toHex(buf);

        int actionOrdinal = action.ordinal();
        int count = entries.size();
        // Trailing comma after EVERY entry (incl. empties) so the field is unambiguous:
        // 0 entries -> "" ; 1 empty entry -> "," ; ["a",""] -> "<hexA>,," . A between-only
        // join + "-" sentinel cannot distinguish an empty list from a single empty string.
        StringBuilder entriesHex = new StringBuilder();
        for (int i = 0; i < entries.size(); i++) {
            entriesHex.append(strHex(entries.get(i)));
            entriesHex.append(',');
        }

        // Round-trip decode sanity: rebuild from the wire and confirm fields match.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hexBytes)));
        ClientboundCustomChatCompletionsPacket dec = CODEC.decode(rbuf);
        boolean ok = dec.action() == action && dec.entries().size() == entries.size();
        if (ok) {
            for (int i = 0; i < entries.size(); i++) {
                if (!dec.entries().get(i).equals(entries.get(i))) { ok = false; break; }
            }
        }
        if (!ok) throw new IllegalStateException("round-trip mismatch for " + caseName);

        O.print("ENC\t");
        O.print(caseName);
        O.print('\t');
        O.print(actionOrdinal);
        O.print('\t');
        O.print(count);
        O.print('\t');
        O.print(entriesHex.toString());  // may be empty (count==0); trailing-comma terminated otherwise
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hexBytes.isEmpty() ? "-" : hexBytes);
        O.print('\n');
    }

    static List<String> listOf(String... xs) {
        List<String> l = new ArrayList<String>();
        for (String x : xs) l.add(x);
        return l;
    }

    static List<String> repeatList(String unit, int n) {
        List<String> l = new ArrayList<String>();
        for (int i = 0; i < n; i++) l.add(unit);
        return l;
    }

    static String repeat(String unit, int n) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < n; i++) sb.append(unit);
        return sb.toString();
    }

    static String toHex(FriendlyByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static String strHex(String s) {
        byte[] u8 = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder();
        for (byte x : u8) sb.append(String.format("%02x", x & 0xff));
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
