import net.minecraft.world.entity.HumanoidArm;
import java.lang.reflect.Field;

// Ground-truth dumper for net.minecraft.world.entity.HumanoidArm (MC 26.1.2).
// Emits tab-separated rows consumed by HumanoidArmParityTest.cpp.
//
// TAGS:
//   CONST   <ordinal> <name> <serializedName> <id> <opposite.ordinal>
public class HumanoidArmParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // HumanoidArm is a plain StringRepresentable enum and needs no registry,
        // but the constructor calls Component.translatable(...) — bootstrap
        // defensively in case classloading the enum touches that machinery.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — emission below does not require a successful bootstrap
        }

        // The `id` field is private — reach it via reflection.
        Field idField = HumanoidArm.class.getDeclaredField("id");
        idField.setAccessible(true);

        // Per-constant: ordinal, name(), getSerializedName(), id, getOpposite().ordinal().
        for (HumanoidArm v : HumanoidArm.values()) {
            int id = idField.getInt(v);
            O.println("CONST\t" + v.ordinal()
                    + "\t" + v.name()
                    + "\t" + v.getSerializedName()
                    + "\t" + id
                    + "\t" + v.getOpposite().ordinal());
        }
    }
}
