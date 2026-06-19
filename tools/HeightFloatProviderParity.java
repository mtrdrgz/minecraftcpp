// Reference generator for the C++ VerticalAnchor / HeightProvider / FloatProvider
// ports. Runs the real decompiled classes from client.jar.
//
//   javac -cp 26.1.2/client.jar:26.1.2/libs/* -d <out> mcpp/tools/HeightFloatProviderParity.java
//   java  -cp <out>:26.1.2/client.jar:26.1.2/libs/* HeightFloatProviderParity out.tsv
//
// WorldGenerationContext's only constructor needs a ChunkGenerator, so we build
// one with Unsafe and set its two int fields directly. RandomSource is a
// LegacyRandomSource (already verified 1:1) reset per case; floats are emitted
// as raw IEEE bits.
import net.minecraft.util.RandomSource;
import net.minecraft.util.valueproviders.ClampedNormalFloat;
import net.minecraft.util.valueproviders.ConstantFloat;
import net.minecraft.util.valueproviders.FloatProvider;
import net.minecraft.util.valueproviders.TrapezoidFloat;
import net.minecraft.util.valueproviders.UniformFloat;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.heightproviders.BiasedToBottomHeight;
import net.minecraft.world.level.levelgen.heightproviders.ConstantHeight;
import net.minecraft.world.level.levelgen.heightproviders.HeightProvider;
import net.minecraft.world.level.levelgen.heightproviders.TrapezoidHeight;
import net.minecraft.world.level.levelgen.heightproviders.UniformHeight;
import net.minecraft.world.level.levelgen.heightproviders.VeryBiasedToBottomHeight;

import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.util.LinkedHashMap;
import java.util.Map;

public class HeightFloatProviderParity {
    static WorldGenerationContext makeContext(int minY, int height) throws Exception {
        Field theUnsafe = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        sun.misc.Unsafe unsafe = (sun.misc.Unsafe) theUnsafe.get(null);
        WorldGenerationContext c = (WorldGenerationContext) unsafe.allocateInstance(WorldGenerationContext.class);
        Field fMinY = WorldGenerationContext.class.getDeclaredField("minY");
        Field fHeight = WorldGenerationContext.class.getDeclaredField("height");
        unsafe.putInt(c, unsafe.objectFieldOffset(fMinY), minY);
        unsafe.putInt(c, unsafe.objectFieldOffset(fHeight), height);
        return c;
    }

    static Map<String, VerticalAnchor> anchors() {
        Map<String, VerticalAnchor> m = new LinkedHashMap<>();
        m.put("abs50", VerticalAnchor.absolute(50));
        m.put("ab10", VerticalAnchor.aboveBottom(10));
        m.put("bt20", VerticalAnchor.belowTop(20));
        m.put("bottom", VerticalAnchor.bottom());
        m.put("top", VerticalAnchor.top());
        return m;
    }

    static Map<String, HeightProvider> heights() {
        Map<String, HeightProvider> m = new LinkedHashMap<>();
        m.put("const50", ConstantHeight.of(VerticalAnchor.absolute(50)));
        m.put("uni_full", UniformHeight.of(VerticalAnchor.aboveBottom(0), VerticalAnchor.belowTop(0)));
        m.put("uni_abs", UniformHeight.of(VerticalAnchor.absolute(60), VerticalAnchor.absolute(70)));
        m.put("bias", BiasedToBottomHeight.of(VerticalAnchor.absolute(-20), VerticalAnchor.absolute(40), 1));
        m.put("verybias", VeryBiasedToBottomHeight.of(VerticalAnchor.absolute(0), VerticalAnchor.absolute(50), 2));
        m.put("trap", TrapezoidHeight.of(VerticalAnchor.absolute(0), VerticalAnchor.absolute(100), 20));
        return m;
    }

    static Map<String, FloatProvider> floats() {
        Map<String, FloatProvider> m = new LinkedHashMap<>();
        m.put("cf", ConstantFloat.of(0.5F));
        m.put("uf01", UniformFloat.of(0.0F, 1.0F));
        m.put("uf_neg", UniformFloat.of(-2.0F, 3.0F));
        m.put("cnf", ClampedNormalFloat.of(0.0F, 1.0F, -2.0F, 2.0F));
        m.put("tf", TrapezoidFloat.of(0.0F, 10.0F, 4.0F));
        return m;
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        WorldGenerationContext ctx = makeContext(-64, 384);
        long[] seeds = { 0L, 1L, 42L, 123456789L, -987654321L };

        try (PrintWriter out = new PrintWriter(args.length > 0 ? args[0] : "height_float_cases.tsv")) {
            for (Map.Entry<String, VerticalAnchor> e : anchors().entrySet()) {
                out.println("ANCHOR\t" + e.getKey() + "\t" + e.getValue().resolveY(ctx));
            }
            for (long seed : seeds) {
                for (Map.Entry<String, HeightProvider> e : heights().entrySet()) {
                    RandomSource r = new LegacyRandomSource(seed);
                    StringBuilder sb = new StringBuilder("HEIGHT\t").append(e.getKey()).append('\t').append(seed);
                    for (int i = 0; i < 8; i++) sb.append('\t').append(e.getValue().sample(r, ctx));
                    out.println(sb);
                }
                for (Map.Entry<String, FloatProvider> e : floats().entrySet()) {
                    RandomSource r = new LegacyRandomSource(seed);
                    StringBuilder sb = new StringBuilder("FLOAT\t").append(e.getKey()).append('\t').append(seed);
                    for (int i = 0; i < 8; i++) sb.append('\t').append(Float.floatToRawIntBits(e.getValue().sample(r)));
                    out.println(sb);
                }
            }
        }
    }
}
