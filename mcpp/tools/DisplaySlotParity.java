// Ground truth for net.minecraft.world.scores.DisplaySlot (Minecraft 26.1.2).
//
// DisplaySlot is a StringRepresentable enum carrying an int id() and a
// serialized name; teamColorToSlot(ChatFormatting) maps the 16 vanilla colors
// to the TEAM_* slots and null for the 6 format codes. BY_ID is a continuous
// ByIdMap (ZERO out-of-bounds -> LIST). We call the REAL net.minecraft methods
// (all public) and emit one row per fact so the C++ port can diff bit/byte.
//
// O = System.out is captured at class-load BEFORE bootstrap so bootstrap chatter
// never pollutes the TSV; rows are buffered and printed once at the end.
//
// Row tags (tab-separated):
//   SLOT  <ordinal:int>  <id:int>  <name:string>  <serialized:string>
//   COUNT <values.length:int>
//   COLOR <colorOrdinal:int> <colorName:string> <slotOrdinalOrMinus1:int> <slotNameOrDASH:string>
//   BYID  <queryId:int> <resultOrdinal:int> <resultName:string>
//   CODEC <input:string> <present:int> <resolvedName:string|->
import net.minecraft.ChatFormatting;
import net.minecraft.world.scores.DisplaySlot;

public class DisplaySlotParity {
    static final java.io.PrintStream O = System.out;
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) throws Exception {
        // Harmless if not required, but keeps us robust if class init ever
        // touches registries (CODEC field).
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable ignored) {
        }

        DisplaySlot[] vals = DisplaySlot.values();

        // Per-constant facts.
        for (DisplaySlot s : vals) {
            row("SLOT",
                Integer.toString(s.ordinal()),
                Integer.toString(s.id()),
                s.name(),
                s.getSerializedName());
        }
        row("COUNT", Integer.toString(vals.length));

        // teamColorToSlot for every ChatFormatting constant (all 22).
        for (ChatFormatting c : ChatFormatting.values()) {
            DisplaySlot slot = DisplaySlot.teamColorToSlot(c);
            if (slot == null) {
                row("COLOR", Integer.toString(c.ordinal()), c.name(), "-1", "-");
            } else {
                row("COLOR", Integer.toString(c.ordinal()), c.name(),
                    Integer.toString(slot.ordinal()), slot.name());
            }
        }

        // BY_ID continuous map (ZERO out-of-bounds). Cover in-range ids,
        // negatives, and beyond-length (all clamp to LIST per ByIdMap.ZERO).
        int[] queries = {
            -100, -19, -2, -1,
            0, 1, 2, 3, 5, 9, 10, 15, 17, 18,
            19, 20, 50, 1000
        };
        for (int q : queries) {
            DisplaySlot r = DisplaySlot.BY_ID.apply(q);
            row("BYID", Integer.toString(q), Integer.toString(r.ordinal()), r.name());
        }

        // CODEC name resolution: every valid serialized name resolves; a few
        // invalid/edge strings do not.
        String[] codecInputs = {
            "list", "sidebar", "below_name",
            "sidebar.team.black", "sidebar.team.white", "sidebar.team.light_purple",
            "", "LIST", "Sidebar", "below name",
            "sidebar.team.cyan", "sidebar.team.", "nope"
        };
        for (String in : codecInputs) {
            DisplaySlot r = DisplaySlot.CODEC.byName(in);
            if (r == null) {
                row("CODEC", in, "0", "-");
            } else {
                row("CODEC", in, "1", r.name());
            }
        }

        O.print(OUT);
        O.flush();
    }

    static void row(String tag, String... cols) {
        OUT.append(tag);
        for (String c : cols) OUT.append('\t').append(c);
        OUT.append('\n');
    }
}
