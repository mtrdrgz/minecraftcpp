// Ground truth for net.minecraft.world.level.levelgen.GenerationStep
// (Minecraft 26.1.2).
//
// In 26.1.2 GenerationStep declares ONE nested enum, Decoration, which implements
// StringRepresentable. Each constant carries a single string field exposed by two
// public accessors that return the SAME value:
//   public String getName()           -> this.name
//   public String getSerializedName() -> this.name
// (Codec uses getSerializedName via StringRepresentable.fromEnum(values).)
//
// The legacy GenerationStep.Carving enum (AIR / LIQUID) was REMOVED in this
// version — BiomeGenerationSettings now keeps a single un-split carver HolderSet —
// so there is nothing of that kind to emit; we do NOT fabricate it.
//
// We iterate the REAL GenerationStep.Decoration.values() and emit, per constant,
// ordinal() + name() + getName() + getSerializedName(); plus a COUNT row.
//
// Row tags (tab-separated; captured on STDOUT):
//   DEC   <ordinal:int>  <name:string>  <getName:string>  <getSerializedName:string>
//   COUNT <values.length:int>
import net.minecraft.world.level.levelgen.GenerationStep;

public class GenerationStepParity {
    static final java.io.PrintStream O = System.out;
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) throws Exception {
        // Harmless for a self-contained enum, but keeps us robust if class init
        // ever reaches into the registries ("Not bootstrapped" guard).
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
        }

        GenerationStep.Decoration[] vals = GenerationStep.Decoration.values();
        for (GenerationStep.Decoration d : vals) {
            row("DEC",
                Integer.toString(d.ordinal()),
                d.name(),
                d.getName(),
                d.getSerializedName());
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
