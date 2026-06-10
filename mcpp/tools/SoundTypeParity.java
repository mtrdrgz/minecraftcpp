// Ground-truth generator for the volume()/pitch() float pair of every
// public-static SoundType constant in
//   net.minecraft.world.level.block.SoundType  (Minecraft 26.1.2)
//
// Calls the REAL net.minecraft class. Each SoundType constant's static initializer
// references SoundEvents.* which resolve through Registry.register(BuiltInRegistries
// .SOUND_EVENT, ...), so loading the SoundType class throws "Not bootstrapped"
// unless Bootstrap has run. We bootstrap up front.
//
// We reflect over SoundType's declared public static final fields whose type is
// SoundType (preserving Class.getDeclaredFields() order, which is source order),
// and read getVolume()/getPitch() (both public). The five Holder<SoundEvent>
// fields (break/step/place/hit/fall) are registry-coupled and intentionally NOT
// emitted — only the two plain floats.
//
// Output rows (tab-separated, to STDOUT — the runner captures stdout into the .tsv):
//   CNT  <number of SoundType constants>
//   ST   <name>  <volume %08x>  <pitch %08x>
//        floats dumped as Float.floatToRawIntBits hex (bit-exact).
//
//   tools/run_groundtruth.ps1 -Tool SoundTypeParity -Out mcpp/build/sound_type.tsv

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

import net.minecraft.world.level.block.SoundType;

public class SoundTypeParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        int count = 0;
        StringBuilder rows = new StringBuilder();
        for (Field f : SoundType.class.getDeclaredFields()) {
            int m = f.getModifiers();
            if (!Modifier.isPublic(m) || !Modifier.isStatic(m) || !Modifier.isFinal(m)) continue;
            if (f.getType() != SoundType.class) continue;
            f.setAccessible(true);
            SoundType st = (SoundType) f.get(null);
            float volume = st.getVolume();
            float pitch = st.getPitch();
            rows.append("ST\t").append(f.getName()).append('\t')
                .append(String.format("%08x", Float.floatToRawIntBits(volume))).append('\t')
                .append(String.format("%08x", Float.floatToRawIntBits(pitch))).append('\n');
            count++;
        }

        O.println("CNT\t" + count);
        O.print(rows);
    }
}
