import net.minecraft.world.level.material.FogType;

// Ground-truth emitter for net.minecraft.world.level.material.FogType
// (Minecraft 26.1.2). Emits tab-separated rows consumed by
// FogTypeParityTest.cpp.
//
// FogType is a bare enum (no fields/methods); the only observable state is the
// constant set, its declaration order (ordinals) and the names. We read those
// directly from the REAL enum via values()/ordinal()/name() — no reflection or
// bootstrap is required because the type has no static registry dependencies.
//
// Tags:
//   COUNT <values().length>
//   ENUM  <ordinal> <name>            one row per constant, in values() order
public class FogTypeParity {
   static final java.io.PrintStream O = System.out;

   public static void main(String[] args) throws Exception {
      FogType[] all = FogType.values();

      // COUNT <number of constants>
      O.println("COUNT\t" + all.length);

      // ENUM <ordinal> <name> for every constant in declaration order.
      for (FogType t : all) {
         O.println("ENUM\t" + t.ordinal() + "\t" + t.name());
      }
   }
}
