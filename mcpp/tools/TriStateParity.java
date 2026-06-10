import net.minecraft.util.TriState;

// Ground truth for mcpp/src/util/TriState.h. Emits tab-separated rows from the
// REAL net.minecraft.util.TriState enum. All exercised methods are public, so no
// reflection is needed.
//
// 26.1.2 TriState has exactly three constants (TRUE, FALSE, DEFAULT) and the
// helpers from(boolean), toBoolean(boolean), getSerializedName(). There is NO
// toBooleanOrElse method in this version. The Codec is serialization-coupled and
// intentionally not exercised here (the value gate covers getSerializedName,
// which is the string the codec relies on).
//
// Row formats (TAG \t inputs... \t outputs...):
//   FROM       value(0/1)            | ordinal(int)               (TriState.from(value))
//   TOBOOL     ordinal default(0/1)  | result(0/1)               (state.toBoolean(default))
//   NAME       ordinal               | serializedName(string)    (state.getSerializedName())
//   ORDINAL    name(string)          | ordinal(int)              (enum declaration order)
public class TriStateParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        TriState[] all = TriState.values();

        // ORDINAL: declaration order (TRUE=0, FALSE=1, DEFAULT=2) and the names enum.
        for (TriState t : all) {
            O.println("ORDINAL\t" + t.name() + "\t" + t.ordinal());
        }

        // FROM: from(true) -> TRUE, from(false) -> FALSE. Emit the resulting ordinal.
        boolean[] bools = { false, true };
        for (boolean v : bools) {
            TriState r = TriState.from(v);
            O.println("FROM\t" + (v ? 1 : 0) + "\t" + r.ordinal());
        }

        // TOBOOL: every state crossed with every default value.
        for (TriState t : all) {
            for (boolean d : bools) {
                boolean res = t.toBoolean(d);
                O.println("TOBOOL\t" + t.ordinal() + "\t" + (d ? 1 : 0) + "\t" + (res ? 1 : 0));
            }
        }

        // NAME: getSerializedName() for each state.
        for (TriState t : all) {
            O.println("NAME\t" + t.ordinal() + "\t" + t.getSerializedName());
        }
    }
}
