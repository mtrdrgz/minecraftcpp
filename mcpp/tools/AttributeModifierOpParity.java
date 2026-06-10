// Ground-truth generator for the net.minecraft.world.entity.ai.attributes
//   .AttributeModifier.Operation enum (id/ordinal/name/getSerializedName) AND the
// AttributeInstance value-combine formula it drives (MC 26.1.2).
//
// Calls REAL net.minecraft types:
//   - AttributeModifier.Operation.values()  -> ordinal()/id()/name()/getSerializedName()
//   - AttributeInstance.calculateValue()     (private, via reflection) for the
//       authoritative base + ADD_VALUE + ADD_MULTIPLIED_BASE + ADD_MULTIPLIED_TOTAL
//       accumulation over a fixed list of double modifiers.
//
// Output rows (tab-separated, STDOUT):
//   OP   <ordinal> <id> <name> <serializedName>
//   CALC <base> <min> <max> <nAdd> [add...] <nMulBase> [mulBase...]
//          <nMulTotal> [mulTotal...] <result>
//
// doubles  = %016x of Double.doubleToRawLongBits; ints decimal; strings raw.
// The per-operation amount lists are emitted in Java's actual map-iteration order
// (read back via the package-private AttributeInstance.getModifiers(Operation)) so
// the C++ side reproduces the floating-point accumulation bit-for-bit.
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

import net.minecraft.core.Holder;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.ai.attributes.Attribute;
import net.minecraft.world.entity.ai.attributes.AttributeInstance;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.entity.ai.attributes.RangedAttribute;

public class AttributeModifierOpParity {
   static final java.io.PrintStream O = System.out;

   static String h(double v) {
      return String.format("%016x", Double.doubleToRawLongBits(v));
   }

   static final class Mod {
      final AttributeModifier.Operation op;
      final double amount;
      Mod(AttributeModifier.Operation op, double amount) { this.op = op; this.amount = amount; }
   }

   static Method calcMethod;
   static Method getModifiersMethod;
   static int counter = 0;

   static void emit(double base, double min, double max, List<Mod> mods) throws Exception {
      // RangedAttribute requires min<=max and min<=default<=max. The default is
      // irrelevant to calculateValue (we override base) — pick a valid in-range one.
      double def = Math.max(min, Math.min(max, 0.0));
      if (def < min) def = min;
      if (def > max) def = max;
      RangedAttribute attr = new RangedAttribute("test", def, min, max);
      Holder<Attribute> holder = Holder.direct(attr);
      AttributeInstance inst = new AttributeInstance(holder, ai -> {});
      inst.setBaseValue(base);

      for (Mod m : mods) {
         Identifier id = Identifier.fromNamespaceAndPath("mcpp", "m" + (counter++));
         AttributeModifier modifier = new AttributeModifier(id, m.amount, m.op);
         inst.addOrUpdateTransientModifier(modifier);
      }

      List<Double> add = orderedAmounts(inst, AttributeModifier.Operation.ADD_VALUE);
      List<Double> mulBase = orderedAmounts(inst, AttributeModifier.Operation.ADD_MULTIPLIED_BASE);
      List<Double> mulTotal = orderedAmounts(inst, AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL);

      double result = (Double) calcMethod.invoke(inst);

      StringBuilder sb = new StringBuilder("CALC\t");
      sb.append(h(base)).append('\t').append(h(min)).append('\t').append(h(max));
      appendList(sb, add);
      appendList(sb, mulBase);
      appendList(sb, mulTotal);
      sb.append('\t').append(h(result));
      O.println(sb.toString());
   }

   @SuppressWarnings("unchecked")
   static List<Double> orderedAmounts(AttributeInstance inst, AttributeModifier.Operation op) throws Exception {
      Map<Identifier, AttributeModifier> m =
         (Map<Identifier, AttributeModifier>) getModifiersMethod.invoke(inst, op);
      List<Double> out = new ArrayList<>();
      for (AttributeModifier mod : m.values()) {
         out.add(mod.amount());
      }
      return out;
   }

   static void appendList(StringBuilder sb, List<Double> xs) {
      sb.append('\t').append(xs.size());
      for (double x : xs) {
         sb.append('\t').append(h(x));
      }
   }

   static Mod av(double a) { return new Mod(AttributeModifier.Operation.ADD_VALUE, a); }
   static Mod mb(double a) { return new Mod(AttributeModifier.Operation.ADD_MULTIPLIED_BASE, a); }
   static Mod mt(double a) { return new Mod(AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL, a); }

   static List<Mod> mods(Mod... ms) {
      List<Mod> l = new ArrayList<>();
      for (Mod m : ms) l.add(m);
      return l;
   }

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      calcMethod = AttributeInstance.class.getDeclaredMethod("calculateValue");
      calcMethod.setAccessible(true);
      getModifiersMethod = AttributeInstance.class.getDeclaredMethod("getModifiers", AttributeModifier.Operation.class);
      getModifiersMethod.setAccessible(true);

      // ---- Operation enum metadata: ordinal / id() / name() / getSerializedName() ----
      for (AttributeModifier.Operation op : AttributeModifier.Operation.values()) {
         O.println("OP"
                 + "\t" + op.ordinal()
                 + "\t" + op.id()
                 + "\t" + op.name()
                 + "\t" + op.getSerializedName());
      }

      // ---- value-combine formula battery (finite/physical inputs only) ----
      final double INF = Double.POSITIVE_INFINITY;
      final double NINF = Double.NEGATIVE_INFINITY;
      final double MAXR = 1024.0; // a common vanilla-ish max
      final double WIDE = Double.MAX_VALUE;

      // No modifiers
      emit(0.0, 0.0, MAXR, mods());
      emit(20.0, 0.0, MAXR, mods());
      emit(-5.0, NINF, INF, mods());
      emit(0.0, NINF, INF, mods());

      // ADD_VALUE only
      emit(10.0, 0.0, MAXR, mods(av(5.0)));
      emit(10.0, 0.0, MAXR, mods(av(5.0), av(-3.0), av(0.25)));
      emit(0.0, NINF, INF, mods(av(1.0), av(2.0), av(3.0), av(4.0)));
      emit(100.0, 0.0, MAXR, mods(av(-50.0), av(-60.0))); // below 0 -> clamp to min

      // ADD_MULTIPLIED_BASE only
      emit(20.0, 0.0, MAXR, mods(mb(0.5)));
      emit(20.0, 0.0, MAXR, mods(mb(0.1), mb(0.2), mb(0.3)));
      emit(8.0, NINF, INF, mods(mb(-0.5)));
      emit(8.0, NINF, INF, mods(mb(-1.0), mb(-1.0)));

      // ADD_MULTIPLIED_TOTAL only
      emit(10.0, 0.0, MAXR, mods(mt(0.5)));
      emit(10.0, 0.0, MAXR, mods(mt(0.1), mt(0.2), mt(0.3)));
      emit(10.0, NINF, INF, mods(mt(-0.5), mt(-0.5)));
      emit(10.0, NINF, INF, mods(mt(-1.0))); // -> 0
      emit(10.0, NINF, INF, mods(mt(2.0), mt(-0.5)));

      // Mixed (all three groups, interleaved insertion order)
      emit(20.0, 0.0, MAXR, mods(av(5.0), mb(0.1), mt(0.2)));
      emit(20.0, 0.0, MAXR, mods(av(5.0), av(2.5), mb(0.1), mb(0.05), mt(0.2), mt(0.1)));
      emit(1.0, NINF, INF, mods(av(3.0), mb(2.0), mt(1.0)));
      emit(0.10000000149011612, NINF, INF,
           mods(av(0.025), mb(0.30000001192092896), mt(0.20000000298023224)));
      emit(0.10000000149011612, 0.0, 1024.0,
           mods(mb(0.2), mb(0.2), mb(0.2), mt(0.1))); // movement-speed-like fp

      // Clamping edges
      emit(500.0, 0.0, MAXR, mods(av(1000.0)));        // over max -> clamp to max
      emit(10.0, 5.0, 10.0, mods(av(-20.0)));          // under min -> clamp to min
      emit(10.0, 10.0, 10.0, mods());                  // min==max
      emit(7.5, 7.5, 7.5, mods(av(100.0)));            // min==max with mods
      emit(20.0, 0.0, MAXR, mods(av(2000.0), mt(-1.0))); // huge then *0 -> 0

      // Large magnitudes / wide range
      emit(1e308, NINF, WIDE, mods(mb(1.0)));
      emit(1.0, 0.0, WIDE, mods(mt(1e308), mt(1e308)));
      emit(1234.56789, -1e9, 1e9, mods(av(0.000001), mb(1e-9), mt(1e-9)));

      // Many modifiers in a group (ordering / accumulation)
      {
         List<Mod> many = new ArrayList<>();
         for (int i = 0; i < 16; i++) many.add(av(0.1));
         emit(0.0, NINF, INF, many);
      }
      {
         List<Mod> many = new ArrayList<>();
         for (int i = 0; i < 10; i++) many.add(mt(0.1));
         emit(1.0, NINF, INF, many);
      }
      {
         List<Mod> many = new ArrayList<>();
         for (int i = 0; i < 8; i++) many.add(mb(0.05));
         for (int i = 0; i < 8; i++) many.add(av((double) i));
         for (int i = 0; i < 8; i++) many.add(mt(-0.01 * i));
         emit(50.0, NINF, INF, many);
      }
   }
}
