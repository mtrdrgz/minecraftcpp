// Ground truth for net.minecraft.network.protocol.game.ClientboundDeleteChatPacket.
//
// 26.1.2 wire format — verified VERBATIM against the REAL source:
//   ClientboundDeleteChatPacket(MessageSignature.Packed messageSignature)
//   STREAM_CODEC = Packet.codec(::write, ::new)  -> body only, NO packet-id prefix.
//   write(FriendlyByteBuf output):
//       MessageSignature.Packed.write(output, this.messageSignature);
//   MessageSignature.Packed.write(out, packed) (MessageSignature.java:85-90):
//       out.writeVarInt(packed.id() + 1);                 // VarInt (plain LEB128, no zig-zag)
//       if (packed.fullSignature() != null)               // == iff id == -1 (id+1 == 0)
//           MessageSignature.write(out, packed.fullSignature());  // out.writeBytes(256 raw bytes)
//   MessageSignature record enforces bytes.length == 256.
//
//   Two encodings:
//     (A) cached id >= 0:  packed = new Packed(id)            -> VarInt(id+1)            [no blob]
//     (B) full signature:  packed = new Packed(MessageSignature(256 bytes))  // id == -1
//                                                             -> VarInt(0) + 256 raw bytes
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <id-dec-signed> <full 0|1> <sigHex-or-"-"> <readableBytes-dec> <hexBytes>
// where <id-dec-signed> is packed.id() (so -1 for the full-signature form, the value we feed
// back into the VarInt), <full> is 1 iff a 256-byte signature follows, and <sigHex> is the
// 256-byte signature (512 hex chars) or "-" when absent. The C++ side INDEPENDENTLY re-encodes
// (writeVarInt(id+1) [+ writeBytes(sig)]) through mc::net::PacketBuffer.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.chat.MessageSignature;
import net.minecraft.network.protocol.game.ClientboundDeleteChatPacket;
import net.minecraft.server.Bootstrap;

@SuppressWarnings({"unchecked", "deprecation"})
public class DeleteChatParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        int caseNo = 0;

        // (A) cached-id form: VarInt(id+1). Exercise the 1->5 byte VarInt boundaries
        //     (id+1 crossing 0,128,16384,...) plus negatives (id+1 < 0 -> 5 bytes).
        // id() is the value written as id+1; choose ids so id+1 hits the boundaries and the
        // negative path. id must NOT be -1 here (id==-1 means full-signature form), so the
        // smallest cached id is 0 -> VarInt(1).
        int[] cachedIds = {
            0,          // id+1 = 1
            1,          // id+1 = 2
            126,        // id+1 = 127  (last 1-byte VarInt)
            127,        // id+1 = 128  (first 2-byte VarInt)
            16382,      // id+1 = 16383 (last 2-byte)
            16383,      // id+1 = 16384 (first 3-byte)
            2097150,    // id+1 = 2097151 (last 3-byte)
            2097151,    // id+1 = 2097152 (first 4-byte)
            268435454,  // id+1 = 268435455 (last 4-byte)
            268435455,  // id+1 = 268435456 (first 5-byte)
            42,
            1000,
            Integer.MAX_VALUE - 1, // id+1 = Integer.MAX_VALUE
            Integer.MAX_VALUE      // id+1 overflows to Integer.MIN_VALUE -> negative -> 5 bytes
        };
        for (int id : cachedIds) {
            emitCached("cached" + (caseNo++), id);
        }

        // (B) full-signature form: id == -1, VarInt(0) followed by 256 signature bytes.
        emitFull("sigZero" + (caseNo++), zeros());
        emitFull("sigOnes" + (caseNo++), ones());
        emitFull("sigRamp" + (caseNo++), ramp());
        emitFull("sigPattern" + (caseNo++), pattern());
    }

    static byte[] zeros() {
        return new byte[256];
    }

    static byte[] ones() {
        byte[] b = new byte[256];
        java.util.Arrays.fill(b, (byte) 0xFF);
        return b;
    }

    static byte[] ramp() {
        byte[] b = new byte[256];
        for (int i = 0; i < 256; i++) b[i] = (byte) i; // 0x00..0xFF, exercises every byte value
        return b;
    }

    static byte[] pattern() {
        byte[] b = new byte[256];
        for (int i = 0; i < 256; i++) b[i] = (byte) ((i * 31 + 7) & 0xFF);
        return b;
    }

    static void emitCached(String name, int id) {
        emit(name, new MessageSignature.Packed(id));
    }

    static void emitFull(String name, byte[] sig) {
        emit(name, new MessageSignature.Packed(new MessageSignature(sig)));
    }

    static void emit(String name, MessageSignature.Packed packed) {
        ClientboundDeleteChatPacket pkt = new ClientboundDeleteChatPacket(packed);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundDeleteChatPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): id + signature must survive.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundDeleteChatPacket back = ClientboundDeleteChatPacket.STREAM_CODEC.decode(rb);
        MessageSignature.Packed bp = back.messageSignature();
        if (bp.id() != packed.id()) {
            throw new IllegalStateException("round-trip id mismatch for " + name + ": " + bp.id() + " != " + packed.id());
        }
        boolean fullA = packed.fullSignature() != null;
        boolean fullB = bp.fullSignature() != null;
        if (fullA != fullB) {
            throw new IllegalStateException("round-trip full-flag mismatch for " + name);
        }
        if (fullA && !java.util.Arrays.equals(packed.fullSignature().bytes(), bp.fullSignature().bytes())) {
            throw new IllegalStateException("round-trip signature mismatch for " + name);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        int full = fullA ? 1 : 0;
        String sigHex = "-";
        if (fullA) {
            byte[] s = packed.fullSignature().bytes();
            StringBuilder sb = new StringBuilder();
            for (byte by : s) sb.append(String.format("%02x", by));
            sigHex = sb.toString();
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(packed.id());
        O.print('\t');
        O.print(full);
        O.print('\t');
        O.print(sigHex);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
