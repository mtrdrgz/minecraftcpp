import net.minecraft.core.FrontAndTop;
import net.minecraft.core.Direction;
import java.lang.reflect.Method;

// Ground-truth dumper for net.minecraft.core.FrontAndTop (MC 26.1.2).
// Emits tab-separated rows consumed by FrontAndTopParityTest.cpp.
//
// TAGS:
//   CONST   <ordinal> <name> <front.ordinal> <top.ordinal>
//   LOOKUP  <front.ordinal> <top.ordinal> <lookupKey>
//   FROM    <front.ordinal> <top.ordinal> <resultOrdinal | -1 if null>
public class FrontAndTopParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // FrontAndTop itself needs no registry bootstrap, but bootstrap defensively
        // in case classloading pulls in registry-touching dependencies.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — FrontAndTop is a plain enum and does not require it
        }

        // FrontAndTop.lookupKey is private static — reach it via reflection.
        Method lookupKey = FrontAndTop.class.getDeclaredMethod("lookupKey", Direction.class, Direction.class);
        lookupKey.setAccessible(true);

        // Per-constant: ordinal, serialized name, front, top.
        for (FrontAndTop v : FrontAndTop.values()) {
            O.println("CONST\t" + v.ordinal() + "\t" + v.getSerializedName()
                    + "\t" + v.front().ordinal() + "\t" + v.top().ordinal());
        }

        Direction[] dirs = Direction.values(); // DOWN,UP,NORTH,SOUTH,WEST,EAST (ordinals 0..5)

        // lookupKey over ALL 36 (front, top) combinations.
        for (Direction front : dirs) {
            for (Direction top : dirs) {
                int key = (Integer) lookupKey.invoke(null, front, top);
                O.println("LOOKUP\t" + front.ordinal() + "\t" + top.ordinal() + "\t" + key);
            }
        }

        // fromFrontAndTop over ALL 36 combinations. Java returns null for non-enumerated
        // pairs (the array slot stays null); we encode null as ordinal -1.
        for (Direction front : dirs) {
            for (Direction top : dirs) {
                FrontAndTop r = FrontAndTop.fromFrontAndTop(front, top);
                int ord = (r == null) ? -1 : r.ordinal();
                O.println("FROM\t" + front.ordinal() + "\t" + top.ordinal() + "\t" + ord);
            }
        }
    }
}
