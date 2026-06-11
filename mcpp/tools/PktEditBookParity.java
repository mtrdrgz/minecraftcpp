// Ground truth for net.minecraft.network.protocol.game.ServerboundEditBookPacket.
//
// The packet is a record (int slot, List<String> pages, Optional<String> title).
// Its STREAM_CODEC is StreamCodec.composite over, IN ORDER:
//     ByteBufCodecs.VAR_INT                                      -> slot
//     ByteBufCodecs.stringUtf8(1024).apply(ByteBufCodecs.list(100)) -> pages
//     ByteBufCodecs.stringUtf8(32).apply(ByteBufCodecs::optional)    -> title
//
// Wire form (ByteBufCodecs.java):
//   slot  : VarInt.write(slot)                       (ByteBufCodecs.VAR_INT)
//   pages : VarInt.write(count) then, per element, Utf8String.write(page, 1024)
//           (collection/list codec: writeCount = VarInt(size); maxSize=100)
//   title : writeBoolean(present); if present Utf8String.write(title, 32)
//           (optional codec: bool flag then the value)
// Utf8String.write = VarInt(byteLength) + UTF-8 bytes; the char limit caps
// String.length() (UTF-16 units), the byte limit caps the encoded length to
// maxLength*3. No packet-type id is part of the codec bytes (framing is external).
//
// Source (26.1.2/src/net/minecraft/network/protocol/game/ServerboundEditBookPacket.java):
//   STREAM_CODEC = StreamCodec.composite(
//       ByteBufCodecs.VAR_INT, ::slot,
//       ByteBufCodecs.stringUtf8(1024).apply(ByteBufCodecs.list(100)), ::pages,
//       ByteBufCodecs.stringUtf8(32).apply(ByteBufCodecs::optional), ::title,
//       ServerboundEditBookPacket::new);
//   (canonical ctor does pages = List.copyOf(pages), so null elements are illegal.)
//
// We encode each case through the REAL StreamCodec into a fresh ByteBuf and dump
// readableBytes() + raw hex; we also decode back through the SAME codec and assert
// the record round-trips. The packet's codec target is plain ByteBuf, so no
// RegistryFriendlyByteBuf is required.
//
//   ENC <name>\t<slot>\t<pagesHex (CSV of per-page UTF-8 hex; "-" = empty list)>\t<titleHex or "-" = absent>\t<readableBytes>\t<hex>
//
// Strings are emitted as UTF-8 HEX so exact bytes survive the ASCII TSV transport
// (run_groundtruth.ps1 writes stdout as ASCII, which would mangle multi-byte UTF-8,
// and byte parity is this gate's whole point). The C++ side decodes the hex back to
// the exact byte string before writeUtf.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundEditBookPacket;

public class PktEditBookParity {
    static final java.io.PrintStream O = System.out;

    static String utf8hex(String s) {
        StringBuilder h = new StringBuilder();
        for (byte b : s.getBytes(StandardCharsets.UTF_8)) h.append(String.format("%02x", b));
        return h.toString();
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundEditBookPacket> codec =
            ServerboundEditBookPacket.STREAM_CODEC;

        // Finite/physical cases. slot crosses the 1->2 byte VarInt boundary; pages
        // exercise empty list, single, multiple, empty-string page, unicode page,
        // a 200-byte page (2-byte length prefix), and a 100-element max list; title
        // exercises absent, present-empty, present-ascii, present-unicode.
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 200; i++) big.append('x');   // 200 bytes -> 2-byte length prefix

        List<String> max100 = new ArrayList<>();
        for (int i = 0; i < 100; i++) max100.add("p" + i);

        List<String> pageWithSpacesAndNewline = List.of("line1\nline2", "page two");

        Object[][] cases = {
            // name, slot, pages(List<String>), title(Optional<String>)
            { "empty_all",      0,   List.of(),                         Optional.empty() },
            { "one_page",       0,   List.of("hello world"),            Optional.empty() },
            { "two_pages",      1,   List.of("first", "second"),        Optional.empty() },
            { "empty_page",     0,   List.of(""),                       Optional.empty() },
            { "title_only",     2,   List.of(),                         Optional.of("My Book") },
            { "title_empty",    3,   List.of("page"),                   Optional.of("") },
            { "pages_and_title",4,   List.of("p1", "p2", "p3"),         Optional.of("Diary") },
            { "unicode_page",   5,   List.of("niño 中文 😀"), Optional.of("título") },
            { "newline_page",   6,   pageWithSpacesAndNewline,          Optional.empty() },
            { "big_page",       7,   List.of(big.toString()),           Optional.empty() },
            { "slot127",        127, List.of("x"),                      Optional.empty() },
            { "slot128",        128, List.of("x"),                      Optional.empty() },   // 2-byte VarInt slot
            { "slot200",        200, List.of("x"),                      Optional.empty() },
            { "max_pages",      9,   max100,                            Optional.of("Anthology") },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            int slot = (Integer) c[1];
            List<String> pages = (List<String>) c[2];
            Optional<String> title = (Optional<String>) c[3];

            ServerboundEditBookPacket pkt = new ServerboundEditBookPacket(slot, pages, title);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ServerboundEditBookPacket back = codec.decode(buf);
            if (back.slot() != slot
                || !back.pages().equals(pages)
                || !back.title().equals(title)) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            // pages -> CSV of per-page UTF-8 hex; "-" denotes the empty list.
            String pagesHex;
            if (pages.isEmpty()) {
                pagesHex = "-";
            } else {
                StringBuilder pb = new StringBuilder();
                for (int i = 0; i < pages.size(); i++) {
                    if (i > 0) pb.append(',');
                    pb.append(utf8hex(pages.get(i)));
                }
                pagesHex = pb.toString();
            }
            // title -> "-" if absent, else UTF-8 hex (note: present-empty title => "" => hex is empty,
            // which would be ambiguous with absent; emit a leading '+' marker for present strings).
            String titleHex = title.isPresent() ? ("+" + utf8hex(title.get())) : "-";

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(slot);
            O.print('\t');
            O.print(pagesHex);
            O.print('\t');
            O.print(titleHex);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
