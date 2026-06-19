// Ground truth for the NBT tag codec (read + write), from REAL world data.
//
// Extracts uncompressed NBT payloads from the generated server world — level.dat
// (gzip root) and several chunk NBTs out of a region .mca (zlib sectors) — writes
// each payload to mcpp/build/nbt_cases/<name>.nbt (raw, uncompressed), and emits
// per case to stdout:
//
//   CANON   <name> <canonical dump>   read-equivalence oracle (spec below)
//   REWRITE <name> <hex>              NbtIo.write bytes after a Java read->write
//                                     round trip (uncompressed, named root)
//
// Canonical dump spec (both sides implement identically; whitespace-free):
//   compound -> {k1=v1,k2=v2,...}   keys sorted by UTF-8 byte order, each key as
//                                   s"<hex of bytes>"
//   list     -> [<elemTypeId>;v1,v2,...]
//   byte/short/int/long -> b/s/i/l + decimal
//   float/double        -> f/d + 8/16 hex digits of the raw IEEE bits
//   string              -> s"<hex of the string's bytes>"  (hex avoids escaping)
//   byte/int/long array -> B/I/L[<hex of each element, fixed width 2/8/16>...]
//   end (in lists)      -> e
//
// The REWRITE oracle is byte-exact because Java's read->write round trip is
// deterministic: reading inserts keys into CompoundTag's HashMap in file order,
// and an identical insertion sequence reproduces the identical iteration order,
// so the rewrite equals the on-disk payload ordering. The C++ side must therefore
// preserve compound INSERTION order to match (java.util.HashMap quirk-emulation is
// NOT needed for round trips).
//
//   run_groundtruth.ps1 -Tool NbtParity -Out mcpp/build/nbt_cases.tsv
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.TreeMap;
import java.util.zip.GZIPInputStream;
import java.util.zip.InflaterInputStream;
import net.minecraft.nbt.ByteArrayTag;
import net.minecraft.nbt.ByteTag;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.DoubleTag;
import net.minecraft.nbt.FloatTag;
import net.minecraft.nbt.IntArrayTag;
import net.minecraft.nbt.IntTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.LongArrayTag;
import net.minecraft.nbt.LongTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.nbt.ShortTag;
import net.minecraft.nbt.StringTag;
import net.minecraft.nbt.Tag;

public class NbtParity {
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) throws Exception {
        Path world = Path.of("26.1.2", "server_run", "world");
        Path casesDir = Path.of("mcpp", "build", "nbt_cases");
        Files.createDirectories(casesDir);

        // level.dat: gzip-compressed named root compound
        byte[] levelPayload;
        try (InputStream in = new GZIPInputStream(Files.newInputStream(world.resolve("level.dat")))) {
            levelPayload = in.readAllBytes();
        }
        emitCase(casesDir, "level_dat", levelPayload);

        // a few chunk NBTs from the first region file (zlib sectors, manual container parse)
        Path regionDir = world.resolve("dimensions/minecraft/overworld/region");
        Path mca = Files.list(regionDir).filter(p -> p.toString().endsWith(".mca")).sorted().findFirst().orElseThrow();
        byte[] region = Files.readAllBytes(mca);
        int emitted = 0;
        for (int idx = 0; idx < 1024 && emitted < 4; idx++) {
            int off = ((region[idx * 4] & 0xFF) << 16) | ((region[idx * 4 + 1] & 0xFF) << 8) | (region[idx * 4 + 2] & 0xFF);
            if (off == 0) continue;
            int base = off * 4096;
            int len = ((region[base] & 0xFF) << 24) | ((region[base + 1] & 0xFF) << 16)
                    | ((region[base + 2] & 0xFF) << 8) | (region[base + 3] & 0xFF);
            int comp = region[base + 4] & 0xFF;
            if (comp != 2) continue; // zlib only
            byte[] payload;
            try (InputStream in = new InflaterInputStream(new ByteArrayInputStream(region, base + 5, len - 1))) {
                payload = in.readAllBytes();
            }
            emitCase(casesDir, "chunk_" + idx, payload);
            emitted++;
        }

        // synthetic case: every tag type + modified-UTF-8 stressors (non-ASCII,
        // embedded U+0000, surrogate pair) — written by the REAL NbtIo so the
        // payload bytes are authoritative for the C++ reader.
        CompoundTag syn = new CompoundTag();
        syn.putByte("b", (byte) -7);
        syn.putShort("s", (short) -30000);
        syn.putInt("i", 123456789);
        syn.putLong("l", -1234567890123456789L);
        syn.putFloat("f", 3.14159f);
        syn.putDouble("d", -2.718281828459045);
        syn.putString("ascii", "hello");
        syn.putString("nul", "a" + (char) 0 + "b");   // MUTF-8 encodes U+0000 as 0xC0 0x80
        syn.putString("eñe-ü-中", "niño 中文 😀");   // key AND value non-ASCII + emoji surrogate pair
        syn.putByteArray("ba", new byte[]{ 0, 1, -1, 127, -128 });
        syn.putIntArray("ia", new int[]{ Integer.MIN_VALUE, -1, 0, Integer.MAX_VALUE });
        syn.putLongArray("la", new long[]{ Long.MIN_VALUE, 0, Long.MAX_VALUE });
        ListTag list = new ListTag();
        list.add(StringTag.valueOf("x"));
        list.add(StringTag.valueOf("ü"));
        syn.put("list", list);
        ListTag nested = new ListTag();
        CompoundTag inner = new CompoundTag();
        inner.putInt("k", 42);
        nested.add(inner);
        syn.put("nested", nested);
        ListTag empty = new ListTag();
        syn.put("emptyList", empty);
        syn.put("emptyCompound", new CompoundTag());
        ByteArrayOutputStream synBytes = new ByteArrayOutputStream();
        NbtIo.write(syn, new DataOutputStream(synBytes));
        emitCase(casesDir, "synthetic", synBytes.toByteArray());

        System.out.print(OUT);
    }

    static void emitCase(Path dir, String name, byte[] payload) throws Exception {
        Files.write(dir.resolve(name + ".nbt"), payload);
        // read with the REAL NbtIo (named root: readAnyTag? level/chunk roots are named "" compounds)
        DataInputStream din = new DataInputStream(new ByteArrayInputStream(payload));
        CompoundTag root = NbtIo.read(din, NbtAccounter.unlimitedHeap());
        OUT.append("CANON\t").append(name).append('\t');
        canon(root, OUT);
        OUT.append('\n');
        // rewrite via the real writer (uncompressed, named "" root — same framing as the payload)
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        NbtIo.write(root, new DataOutputStream(bos));
        OUT.append("REWRITE\t").append(name).append('\t').append(hex(bos.toByteArray())).append('\n');
    }

    static void canon(Tag tag, StringBuilder sb) {
        if (tag instanceof CompoundTag c) {
            TreeMap<byte[], Tag> sorted = new TreeMap<>((a, b) -> {
                int n = Math.min(a.length, b.length);
                for (int i = 0; i < n; i++) { int d = (a[i] & 0xFF) - (b[i] & 0xFF); if (d != 0) return d; }
                return a.length - b.length;
            });
            for (String k : c.keySet()) sorted.put(k.getBytes(StandardCharsets.UTF_8), c.get(k));
            sb.append('{');
            boolean first = true;
            for (var e : sorted.entrySet()) {
                if (!first) sb.append(',');
                first = false;
                sb.append("s\"").append(hex(e.getKey())).append("\"=");
                canon(e.getValue(), sb);
            }
            sb.append('}');
        } else if (tag instanceof ListTag l) {
            // no element-type id in the canon form: per-element prefixes carry the
            // type, and the wire-level list element-type byte is covered by REWRITE
            sb.append('[');
            for (int i = 0; i < l.size(); i++) { if (i > 0) sb.append(','); canon(l.get(i), sb); }
            sb.append(']');
        } else if (tag instanceof ByteTag t)   sb.append('b').append(t.byteValue());
        else if (tag instanceof ShortTag t)    sb.append('s').append(t.shortValue());
        else if (tag instanceof IntTag t)      sb.append('i').append(t.intValue());
        else if (tag instanceof LongTag t)     sb.append('l').append(t.longValue());
        else if (tag instanceof FloatTag t)    sb.append('f').append(String.format("%08x", Float.floatToRawIntBits(t.floatValue())));
        else if (tag instanceof DoubleTag t)   sb.append('d').append(String.format("%016x", Double.doubleToRawLongBits(t.doubleValue())));
        else if (tag instanceof StringTag t)   sb.append("s\"").append(hex(t.value().getBytes(StandardCharsets.UTF_8))).append('"');
        else if (tag instanceof ByteArrayTag t) {
            sb.append("B[");
            for (byte b : t.getAsByteArray()) sb.append(String.format("%02x", b));
            sb.append(']');
        } else if (tag instanceof IntArrayTag t) {
            sb.append("I[");
            for (int v : t.getAsIntArray()) sb.append(String.format("%08x", v));
            sb.append(']');
        } else if (tag instanceof LongArrayTag t) {
            sb.append("L[");
            for (long v : t.getAsLongArray()) sb.append(String.format("%016x", v));
            sb.append(']');
        } else if (tag.getId() == Tag.TAG_END) sb.append('e');
        else throw new IllegalStateException("unhandled tag id " + tag.getId());
    }

    static String hex(byte[] bytes) {
        StringBuilder sb = new StringBuilder(bytes.length * 2);
        for (byte b : bytes) sb.append(String.format("%02x", b));
        return sb.toString();
    }
}
