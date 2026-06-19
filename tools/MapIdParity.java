import net.minecraft.world.level.saveddata.maps.MapId;

// Ground truth for mcpp/src/world/level/saveddata/maps/MapId.h. Emits tab-separated rows
// from the REAL net.minecraft.world.level.saveddata.maps.MapId record. MapId is a public
// record (public canonical ctor, public id() accessor, public record-generated
// equals/hashCode, public key()), so NO reflection is needed.
//
// Run:  mcpp/tools/run_groundtruth.ps1 -Tool MapIdParity -Out mcpp/build/map_id.tsv
//
// Row formats (TAG \t inputs... \t outputs...):
//   ID        id            | id()  (decimal, should round-trip the input)
//   HASH      id            | hashCode()  (decimal, signed int -- proves == id)
//   KEY       id            | key()  (raw string "maps/" + id)
//   EQUALS    idA idB       | equals(0/1)
public class MapIdParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Finite/physical ints spanning zeros, small magnitudes, byte/short/word
        // boundaries, negatives, and the 32-bit extremes. (Map ids are non-negative in
        // practice, but the record wrapper imposes no constraint, so the full signed
        // range exercises the int-concat / hashCode paths.)
        int[] vals = {
            Integer.MIN_VALUE, -2147483647, -1000000000, -1000000, -65536, -65535,
            -256, -255, -128, -100, -16, -10, -2, -1,
            0, 1, 2, 9, 10, 16, 99, 100, 127, 128, 255, 256, 999, 1000,
            65535, 65536, 1000000, 1000000000, 2147483646, Integer.MAX_VALUE
        };

        for (int id : vals) {
            MapId m = new MapId(id);
            O.println("ID\t" + id + "\t" + m.id());
            O.println("HASH\t" + id + "\t" + m.hashCode());
            O.println("KEY\t" + id + "\t" + m.key());
        }

        // EQUALS over all ordered pairs (record equals compares the single int component).
        for (int a : vals) {
            MapId ma = new MapId(a);
            for (int b : vals) {
                MapId mb = new MapId(b);
                int eq = ma.equals(mb) ? 1 : 0;
                O.println("EQUALS\t" + a + "\t" + b + "\t" + eq);
            }
        }
    }
}
