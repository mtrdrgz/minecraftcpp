// Ground truth for net.minecraft.world.entity.ai.memory.MemoryStatus
// (Minecraft 26.1.2).
//
// MemoryStatus is a plain Java enum with no fields/constructor:
//
//   public enum MemoryStatus { VALUE_PRESENT, VALUE_ABSENT, REGISTERED; }
//
// The only observable, ported state is each constant's ordinal() (declaration
// order) and name(). We iterate the REAL
// net.minecraft.world.entity.ai.memory.MemoryStatus.values() array and emit one
// row per constant so the C++ side can diff both.
//
// Output is captured at class-load via O = System.out BEFORE bootstrap, so any
// engine bootstrap chatter never pollutes the TSV.
//
// Row tags (tab-separated):
//   STATUS <ordinal:int>  <name:string>
//   COUNT  <values.length:int>
import net.minecraft.world.entity.ai.memory.MemoryStatus;

public class MemoryStatusParity {
    static final java.io.PrintStream O = System.out;
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) throws Exception {
        // Bootstrap is not strictly required for a fieldless enum, but harmless
        // and keeps us robust if class init ever touches registries.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
        }

        MemoryStatus[] vals = MemoryStatus.values();
        for (MemoryStatus s : vals) {
            row("STATUS", Integer.toString(s.ordinal()), s.name());
        }
        row("COUNT", Integer.toString(vals.length));

        O.print(OUT);
        O.flush();
    }

    static void row(String tag, String... cols) {
        OUT.append(tag);
        for (String c : cols) OUT.append('\t').append(c);
        OUT.append('\n');
    }
}
