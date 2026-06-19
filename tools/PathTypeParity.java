import java.lang.reflect.Method;

// Ground-truth emitter for net.minecraft.world.level.pathfinder.PathType (26.1.2).
// Emits one tab-separated row per enum constant:
//   PT  <ordinal:int>  <name:string>  <getMalus:%08x bits>
// plus a single COUNT row with values().length.
//
// Reflects the REAL net.minecraft enum so the constant order, names, and the
// per-constant getMalus() values are pulled straight from the shipped class.
public class PathTypeParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        Class<?> ptC = Class.forName("net.minecraft.world.level.pathfinder.PathType");

        // getMalus() is public (PathType.java:38) but call via reflection to stay
        // uniform with the repo's GT pattern.
        Method getMalus = ptC.getMethod("getMalus");
        getMalus.setAccessible(true);

        Object[] values = ((Class) ptC).getEnumConstants();
        O.println("COUNT\t" + values.length);

        for (Object e : values) {
            Enum<?> en = (Enum<?>) e;
            int ordinal = en.ordinal();
            String name = en.name();
            float malus = (float) getMalus.invoke(e);
            O.println("PT\t" + ordinal + "\t" + name + "\t" + f(malus));
        }
    }
}
