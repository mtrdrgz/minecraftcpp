// DumpRawNbt — reads one chunk's raw NBT from a region file and prints the
// structures.starts compound verbatim. Used to verify StructureStartsDump.java
// is reading the right fields.
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.zip.GZIPInputStream;
import java.util.zip.InflaterInputStream;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtIo;
import net.minecraft.nbt.Tag;

public class DumpRawNbt {
    static String regionDir = "26.1.2/server_run/world_structures/dimensions/minecraft/overworld/region";

    static int beInt(byte[] b, int off) {
        return ((b[off] & 0xFF) << 24) | ((b[off + 1] & 0xFF) << 16)
             | ((b[off + 2] & 0xFF) << 8) | (b[off + 3] & 0xFF);
    }

    static CompoundTag readChunkNbt(int cx, int cz) throws IOException {
        int rx = cx >> 5, rz = cz >> 5;
        Path mca = Path.of(regionDir, "r." + rx + "." + rz + ".mca");
        if (!Files.exists(mca)) return null;
        byte[] file = Files.readAllBytes(mca);
        int idx = (cx & 31) + (cz & 31) * 32;
        int loc = beInt(file, idx * 4);
        if (loc == 0) return null;
        int start = (loc >>> 8) * 4096;
        int length = beInt(file, start);
        int compression = file[start + 4] & 0xFF;
        byte[] payload = new byte[length - 1];
        System.arraycopy(file, start + 5, payload, 0, length - 1);
        java.io.InputStream raw = new ByteArrayInputStream(payload);
        java.io.InputStream dec;
        switch (compression) {
            case 1: dec = new GZIPInputStream(raw); break;
            case 2: dec = new InflaterInputStream(raw); break;
            default: throw new IOException("unsupported compression " + compression);
        }
        try (DataInputStream dis = new DataInputStream(dec)) {
            return NbtIo.read(dis);
        }
    }

    static void printTag(Tag tag, String indent) {
        if (tag instanceof CompoundTag ct) {
            for (String key : ct.keySet()) {
                Tag child = ct.get(key);
                System.out.println(indent + key + " (" + child.getType().getName() + ")");
                if (child instanceof CompoundTag || child instanceof ListTag) {
                    printTag(child, indent + "  ");
                } else {
                    System.out.println(indent + "  = " + child.toString().substring(0, Math.min(80, child.toString().length())));
                }
            }
        } else if (tag instanceof ListTag lt) {
            System.out.println(indent + "[list of " + lt.size() + "]");
            for (int i = 0; i < Math.min(lt.size(), 3); i++) {
                System.out.println(indent + "  [" + i + "]:");
                printTag(lt.get(i), indent + "    ");
            }
            if (lt.size() > 3) System.out.println(indent + "  ... (" + (lt.size() - 3) + " more)");
        }
    }

    public static void main(String[] args) throws IOException {
        int cx = Integer.parseInt(args[0]);
        int cz = Integer.parseInt(args[1]);
        CompoundTag root = readChunkNbt(cx, cz);
        if (root == null) { System.err.println("chunk not found"); return; }
        CompoundTag structures = root.getCompoundOrEmpty("structures");
        CompoundTag starts = structures.getCompoundOrEmpty("starts");
        System.out.println("=== Chunk (" + cx + "," + cz + ") structure starts ===");
        for (String key : starts.keySet()) {
            CompoundTag start = starts.getCompoundOrEmpty(key);
            System.out.println("\n--- " + key + " ---");
            printTag(start, "");
        }
    }
}
