import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.saveddata.maps.MapDecorationType;

// Ground-truth dumper for net.minecraft.world.level.saveddata.maps.MapDecorationType
// and the built-in registrations from MapDecorationTypes (MC 26.1.2).
//
// We drive the REAL BuiltInRegistries.MAP_DECORATION_TYPE (populated by Bootstrap),
// reading each registered MapDecorationType's record accessors and hasMapColor().
// Emits tab-separated rows consumed by MapDecorationTypeParityTest.cpp.
//
// TAGS:
//   ENTRY <regNamespace> <regPath> <assetNamespace> <assetPath> <showOnItemFrame> \
//         <mapColor> <explorationMapElement> <trackCount> <hasMapColor>
//   CONST <NO_MAP_COLOR>
public class MapDecorationTypeParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Registry access requires the data bootstrap; without it MAP_DECORATION_TYPE is empty.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        O.println("CONST\t" + MapDecorationType.NO_MAP_COLOR);

        // Iterate in registration order. Registry iteration order for a frozen
        // MappedRegistry is registration order, matching MapDecorationTypes field order.
        var reg = BuiltInRegistries.MAP_DECORATION_TYPE;
        for (Holder.Reference<MapDecorationType> ref : reg.listElements().toList()) {
            Identifier regId = ref.key().identifier();
            MapDecorationType t = ref.value();
            Identifier assetId = t.assetId();
            O.println("ENTRY"
                    + "\t" + regId.getNamespace()
                    + "\t" + regId.getPath()
                    + "\t" + assetId.getNamespace()
                    + "\t" + assetId.getPath()
                    + "\t" + (t.showOnItemFrame() ? 1 : 0)
                    + "\t" + t.mapColor()
                    + "\t" + (t.explorationMapElement() ? 1 : 0)
                    + "\t" + (t.trackCount() ? 1 : 0)
                    + "\t" + (t.hasMapColor() ? 1 : 0));
        }
    }
}
