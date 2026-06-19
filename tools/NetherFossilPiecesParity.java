// Reference case generator for the deterministic selection inside
// net.minecraft.world.level.levelgen.structure.structures.NetherFossilPieces.
// Runs the REAL decompiled classes from client.jar and emits the fossil template
// and Rotation chosen by NetherFossilPieces.addPieces' RNG consumption order:
//   Rotation.getRandom(random) first, Util.getRandom(FOSSILS, random) second.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/NetherFossilPiecesParity.java
//   java  -cp <out>:26.1.2/client.jar NetherFossilPiecesParity > nether_fossil_pieces_cases.tsv

import java.lang.reflect.Field;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.util.Util;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.structure.structures.NetherFossilPieces;

public class NetherFossilPiecesParity {
    static Identifier[] fossils() throws Exception {
        Field f = NetherFossilPieces.class.getDeclaredField("FOSSILS");
        f.setAccessible(true);
        return (Identifier[])f.get(null);
    }

    public static void main(String[] args) throws Exception {
        Identifier[] fossils = fossils();
        long[] seeds = {
            0L,
            1L,
            -1L,
            12345L,
            987654321L,
            -1234567890123456789L,
            9223372036854775807L,
            -9223372036854775808L
        };

        for (long seed : seeds) {
            RandomSource random = RandomSource.create(seed);
            Rotation rotation = Rotation.getRandom(random);
            Identifier template = Util.getRandom(fossils, random);
            System.out.println("CASE\t" + seed + "\t" + rotation.name() + "\t" + template.toString());
        }
    }
}
