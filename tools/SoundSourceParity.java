import net.minecraft.sounds.SoundSource;

// Ground-truth dumper for net.minecraft.sounds.SoundSource (MC 26.1.2).
// Emits tab-separated rows consumed by SoundSourceParityTest.cpp.
//
// TAGS:
//   CONST  <ordinal> <name()> <getName()>
//   COUNT  <values().length>
//
// ordinal / count are decimal; name() (UPPERCASE identifier) and getName()
// (lowercase serialized id) are raw strings. All values come from the REAL
// net.minecraft.sounds.SoundSource enum via its public API.
public class SoundSourceParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // SoundSource is a plain enum and needs no registry bootstrap, but bootstrap
        // defensively in case classloading pulls in registry-touching dependencies.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — SoundSource does not require it
        }

        SoundSource[] vals = SoundSource.values();
        for (SoundSource v : vals) {
            O.println("CONST"
                    + "\t" + v.ordinal()
                    + "\t" + v.name()
                    + "\t" + v.getName());
        }
        O.println("COUNT" + "\t" + vals.length);
    }
}
