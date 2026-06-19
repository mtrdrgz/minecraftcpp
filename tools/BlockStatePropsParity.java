// Ground-truth generator for the value<->name handling of
//   net.minecraft.world.level.block.state.properties.{IntegerProperty,
//   BooleanProperty, EnumProperty}.
//
// Calls the REAL net.minecraft classes via their public factories
//   IntegerProperty.create(name,min,max)
//   BooleanProperty.create(name)
//   EnumProperty.create(name, clazz)             (all constants)
//   EnumProperty.create(name, clazz, list...)    (explicit subset)
// over the Direction and Direction.Axis sample enums.
//
// These properties are pure string<->value maps with no registry/world state.
// Bootstrap is added defensively but should not be required.
//
// TSV columns (tab-separated, one row per probe):
//   IGETNAME    <propIdx>  <intValue>          -> <name string>
//   IGETVALUE   <propIdx>  <inputString>       -> <present 0/1>  <intValue|0>
//   IGETIDX     <propIdx>  <intValue>          -> <internalIndex>
//   IVALUES     <propIdx>                      -> <count>  <v0> <v1> ...
//   BGETNAME    <bool 0/1>                     -> <name string>
//   BGETVALUE   <inputString>                  -> <present 0/1>  <bool 0/1>
//   BGETIDX     <bool 0/1>                     -> <internalIndex>
//   BVALUES                                    -> <count>  <0/1> ...
//   EGETNAME    <enumIdx>  <ordinal>           -> <serialized name>
//   EGETVALUE   <enumIdx>  <inputString>       -> <present 0/1>  <ordinal|-1>
//   EGETIDX     <enumIdx>  <ordinal>           -> <internalIndex>
//   EVALUES     <enumIdx>                      -> <count>  <ord0> <ord1> ...
//
// Property indices for the integer battery are emitted alongside their min/max
// in an IMETA row so the C++ side reconstructs the identical property objects.
// Enum indices are emitted in an EMETA row that lists every constant's
// (ordinal, serializedName) plus the selected subset, so the C++ side rebuilds
// the same EnumProperty without needing any Java enum reflection at test time.
//
//   tools/run_groundtruth.ps1 -Tool BlockStatePropsParity -Out mcpp/build/block_state_props.tsv

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

import net.minecraft.core.Direction;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.level.block.state.properties.IntegerProperty;

public class BlockStatePropsParity {
    static final java.io.PrintStream O = System.out;

    // ---- integer test batteries: {name, min, max} ----
    static final Object[][] INT_PROPS = {
        {"age",   0, 1},
        {"age",   0, 25},      // a wide range
        {"level", 0, 15},      // classic block property
        {"power", 1, 9},       // non-zero min
        {"bites", 0, 7},
        {"dist",  3, 7},       // non-zero min, small span
    };

    // Strings to feed getValue (covers in-range, out-of-range, boundaries,
    // signs, leading zeros, non-numeric, empty, overflow).
    static final String[] PARSE_STRINGS = {
        "0", "1", "2", "3", "7", "8", "9", "15", "16", "25", "26",
        "-1", "+1", "+0", "-0", "00", "007", "010",
        "", " ", " 5", "5 ", "abc", "1a", "a1", "1.0", "0x5", "+", "-",
        "2147483647", "2147483648", "-2147483648", "-2147483649",
        "99999999999999999999", "3", "5",
    };

    static final boolean[] BOOLS = { true, false };
    static final String[] BOOL_STRINGS = {
        "true", "false", "True", "FALSE", "TRUE", "0", "1", "", "yes", "no", " true",
    };

    // ---- enum sample set: Direction and Direction.Axis, full + subsets ----
    // Each entry: { displayIndex, clazz, selectionOrdinals (null => all) }.
    @SuppressWarnings({"rawtypes", "unchecked"})
    static EnumProperty makeEnum(String name, Class clazz, int[] sel) throws Exception {
        if (sel == null) {
            return EnumProperty.create(name, clazz);
        }
        Object[] all = clazz.getEnumConstants();
        List<Object> list = new ArrayList<>();
        for (int o : sel) list.add(all[o]);
        // EnumProperty.create(String, Class, List)
        Method m = EnumProperty.class.getMethod("create", String.class, Class.class, List.class);
        return (EnumProperty) m.invoke(null, name, clazz, list);
    }

    static String serName(Object enumConst) throws Exception {
        // StringRepresentable.getSerializedName()
        Method m = enumConst.getClass().getMethod("getSerializedName");
        return (String) m.invoke(enumConst);
    }

    @SuppressWarnings({"rawtypes", "unchecked"})
    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) { /* pure string maps; not required */ }

        // ===== IntegerProperty =====
        for (int pi = 0; pi < INT_PROPS.length; pi++) {
            String nm = (String) INT_PROPS[pi][0];
            int min = (Integer) INT_PROPS[pi][1];
            int max = (Integer) INT_PROPS[pi][2];
            IntegerProperty prop = IntegerProperty.create(nm, min, max);

            O.println("IMETA\t" + pi + "\t" + min + "\t" + max);

            List<Integer> vals = prop.getPossibleValues();
            StringBuilder vb = new StringBuilder("IVALUES\t" + pi + "\t" + vals.size());
            for (Integer v : vals) vb.append('\t').append((int) v);
            O.println(vb.toString());

            // getName over the full range plus a couple beyond (Integer.toString
            // is total, so we probe min-1..max+1 to exercise it generally).
            for (int v = min; v <= max; v++) {
                O.println("IGETNAME\t" + pi + "\t" + v + "\t" + prop.getName(v));
                O.println("IGETIDX\t" + pi + "\t" + v + "\t" + prop.getInternalIndex(v));
            }
            // getInternalIndex for out-of-range high value (returns -1) and a low one.
            O.println("IGETNAME\t" + pi + "\t" + (max + 1) + "\t" + prop.getName(max + 1));
            O.println("IGETIDX\t" + pi + "\t" + (max + 1) + "\t" + prop.getInternalIndex(max + 1));
            O.println("IGETIDX\t" + pi + "\t" + max + "\t" + prop.getInternalIndex(max));

            for (String s : PARSE_STRINGS) {
                Optional<Integer> r = prop.getValue(s);
                O.println("IGETVALUE\t" + pi + "\t" + s + "\t" + (r.isPresent() ? 1 : 0)
                          + "\t" + (r.isPresent() ? r.get() : 0));
            }
        }

        // ===== BooleanProperty =====
        BooleanProperty bprop = BooleanProperty.create("powered");
        List<Boolean> bvals = bprop.getPossibleValues();
        StringBuilder bb = new StringBuilder("BVALUES\t" + bvals.size());
        for (Boolean b : bvals) bb.append('\t').append(b ? 1 : 0);
        O.println(bb.toString());
        for (boolean b : BOOLS) {
            O.println("BGETNAME\t" + (b ? 1 : 0) + "\t" + bprop.getName(b));
            O.println("BGETIDX\t" + (b ? 1 : 0) + "\t" + bprop.getInternalIndex(b));
        }
        for (String s : BOOL_STRINGS) {
            Optional<Boolean> r = bprop.getValue(s);
            O.println("BGETVALUE\t" + s + "\t" + (r.isPresent() ? 1 : 0)
                      + "\t" + (r.isPresent() && r.get() ? 1 : 0));
        }

        // ===== EnumProperty (Direction + Direction.Axis, full & subsets) =====
        Class dir = Direction.class;
        Class axis = Direction.Axis.class;
        // enum index -> (clazz, selection). null selection => all constants.
        Object[][] enumSpecs = {
            {dir,  null},                          // 0: all 6 directions
            {dir,  new int[]{2, 3, 4, 5}},         // 1: NORTH,SOUTH,WEST,EAST subset
            {dir,  new int[]{5, 4, 2, 3}},         // 2: reordered subset (indexOf/order matters)
            {dir,  new int[]{0, 1}},               // 3: DOWN,UP only
            {axis, null},                          // 4: all 3 axes
            {axis, new int[]{0, 2}},               // 5: X,Z subset
            {axis, new int[]{1}},                  // 6: Y only
        };
        // strings to probe getValue for each enum (valid + invalid + cross-enum).
        String[] dirStrings = {"down", "up", "north", "south", "east", "west", "DOWN", "x", "", "n", "norths"};
        String[] axisStrings = {"x", "y", "z", "X", "Y", "Z", "down", "", "xy"};

        for (int ei = 0; ei < enumSpecs.length; ei++) {
            Class clazz = (Class) enumSpecs[ei][0];
            int[] sel = (int[]) enumSpecs[ei][1];
            EnumProperty prop = makeEnum("dir" + ei, clazz, sel);

            Object[] all = clazz.getEnumConstants();
            // EMETA: <enumIdx> <numConstants> [<ord> <serName>]... <selCount> [<selOrd>]...
            StringBuilder mb = new StringBuilder("EMETA\t" + ei + "\t" + all.length);
            for (int o = 0; o < all.length; o++) {
                mb.append('\t').append(o).append('\t').append(serName(all[o]));
            }
            List<Object> selected = prop.getPossibleValues();
            mb.append('\t').append(selected.size());
            for (Object v : selected) {
                int ord = ((Enum) v).ordinal();
                mb.append('\t').append(ord);
            }
            O.println(mb.toString());

            // EVALUES: selected ordinals in order
            StringBuilder eb = new StringBuilder("EVALUES\t" + ei + "\t" + selected.size());
            for (Object v : selected) eb.append('\t').append(((Enum) v).ordinal());
            O.println(eb.toString());

            // getName + getInternalIndex over ALL constants (covers in-subset and
            // out-of-subset ordinals: getName is total, getInternalIndex -> -1 if
            // a constant is not in the subset).
            for (int o = 0; o < all.length; o++) {
                Object ec = all[o];
                O.println("EGETNAME\t" + ei + "\t" + o + "\t" + ((EnumProperty) prop).getName((Comparable) ec));
                O.println("EGETIDX\t" + ei + "\t" + o + "\t" + ((EnumProperty) prop).getInternalIndex((Enum) ec));
            }

            String[] probes = (clazz == axis) ? axisStrings : dirStrings;
            for (String s : probes) {
                Optional<Object> r = prop.getValue(s);
                int ord = (r.isPresent()) ? ((Enum) r.get()).ordinal() : -1;
                O.println("EGETVALUE\t" + ei + "\t" + s + "\t" + (r.isPresent() ? 1 : 0) + "\t" + ord);
            }
        }
    }
}
