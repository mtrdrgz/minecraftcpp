// StructureStartsDump — ground truth for the createStructures gate.
//
// Reads the REAL server's structure-enabled world (generate-structures=true, written
// into world_structures/ by run_server_gen_structures.ps1) and dumps, per chunk, the
// `structures.starts` compound: every StructureStart that the server's
// ChunkGenerator.createStructures decided to BEGIN at that chunk (only VALID starts are
// stored — setStartForStructure is gated on start.isValid()). This is exactly the SET
// the C++ createStructures wiring must reproduce: which structure starts at which chunk,
// with which pieces (id + bounding box + orientation + genDepth), in builder order.
//
//   tools/run_groundtruth.ps1 -Tool StructureStartsDump -Out mcpp/build/structure_starts.tsv
//
// Output (TSV, ASCII):
//   S\t<structureId>\t<chunkX>\t<chunkZ>\t<references>\t<childCount>
//   C\t<pieceId>\t<minX>\t<minY>\t<minZ>\t<maxX>\t<maxY>\t<maxZ>\t<O>\t<GD>
// (each S is immediately followed by its <childCount> C rows, in stored order.)
//
// Region container parsing copied from ServerChunkDump.readChunkNbt (version-stable);
// only the per-chunk NBT payload is parsed with the real net.minecraft.nbt.NbtIo.
import java.io.ByteArrayInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.TreeMap;
import java.util.zip.GZIPInputStream;
import java.util.zip.InflaterInputStream;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtIo;

public class StructureStartsDump {
    static String REGION_DIR =
        "26.1.2/server_run/world_structures/dimensions/minecraft/overworld/region";

    static int beInt(byte[] b, int off) {
        return ((b[off] & 0xFF) << 24) | ((b[off + 1] & 0xFF) << 16)
             | ((b[off + 2] & 0xFF) << 8) | (b[off + 3] & 0xFF);
    }

    static CompoundTag readChunkNbt(int cx, int cz) throws IOException {
        int rx = cx >> 5, rz = cz >> 5;
        Path mca = Path.of(REGION_DIR, "r." + rx + "." + rz + ".mca");
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
            case 3: dec = raw; break;
            default: throw new IOException("unsupported region compression type " + compression);
        }
        try (DataInputStream dis = new DataInputStream(dec)) {
            return NbtIo.read(dis);
        }
    }

    public static void main(String[] args) throws IOException {
        if (args.length > 0) REGION_DIR = args[0];
        java.io.PrintStream out = System.out;
        TreeMap<String, Integer> tally = new TreeMap<>();
        int chunksWithStarts = 0, totalStarts = 0;

        Path dir = Path.of(REGION_DIR);
        if (!Files.isDirectory(dir)) {
            System.err.println("region dir not found: " + dir.toAbsolutePath());
            return;
        }
        try (var stream = Files.list(dir)) {
            for (Path mca : stream.filter(p -> p.getFileName().toString().endsWith(".mca")).sorted().toList()) {
                String[] parts = mca.getFileName().toString().split("\\.");
                int rx = Integer.parseInt(parts[1]), rz = Integer.parseInt(parts[2]);
                byte[] file = Files.readAllBytes(mca);
                for (int i = 0; i < 1024; i++) {
                    if (beInt(file, i * 4) == 0) continue;
                    int cx = rx * 32 + (i & 31), cz = rz * 32 + (i >> 5);
                    CompoundTag root = readChunkNbt(cx, cz);
                    if (root == null) continue;
                    CompoundTag structures = root.getCompoundOrEmpty("structures");
                    CompoundTag starts = structures.getCompoundOrEmpty("starts");
                    boolean any = false;
                    for (String key : starts.keySet()) {
                        CompoundTag start = starts.getCompoundOrEmpty(key);
                        String id = start.getStringOr("id", "");
                        if (id.isEmpty() || "INVALID".equals(id)) continue;
                        int sChunkX = start.getIntOr("ChunkX", 0);
                        int sChunkZ = start.getIntOr("ChunkZ", 0);
                        int references = start.getIntOr("references", 0);
                        ListTag children = start.getListOrEmpty("Children");
                        out.println("S\t" + id + "\t" + sChunkX + "\t" + sChunkZ + "\t"
                            + references + "\t" + children.size());
                        for (int ci = 0; ci < children.size(); ci++) {
                            CompoundTag piece = children.getCompoundOrEmpty(ci);
                            String pid = piece.getStringOr("id", "");
                            int[] bb = piece.getIntArray("BB").orElse(new int[6]);
                            int o = piece.getIntOr("O", 0);
                            int gd = piece.getIntOr("GD", 0);
                            StringBuilder sb = new StringBuilder("C\t").append(pid);
                            for (int k = 0; k < 6; k++) sb.append('\t').append(k < bb.length ? bb[k] : 0);
                            sb.append('\t').append(o).append('\t').append(gd);
                            out.println(sb);
                        }
                        tally.merge(id, 1, Integer::sum);
                        totalStarts++;
                        any = true;
                    }
                    if (any) chunksWithStarts++;
                }
            }
        }
        System.err.println("chunksWithStarts=" + chunksWithStarts + " totalStarts=" + totalStarts);
        for (var e : tally.entrySet()) System.err.println("  " + e.getKey() + " = " + e.getValue());
    }
}
