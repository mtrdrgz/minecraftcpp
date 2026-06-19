// Ground truth for the model "display" deserializers ItemTransform / ItemTransforms
// (ItemTransform.java:46-82, ItemTransforms.java:49-78). Parses each display block via the PUBLIC
// CuboidModel.fromStream (which registers both deserializers), reads model.transforms().getTransform
// per ItemDisplayContext, and emits each transform's rotation/translation/scale.
//
//   tools/run_groundtruth.ps1 -Tool ItemTransformsParity -Out mcpp/build/item_transforms.tsv
//
// Row: IT \t <base64 display-block> \t [ per context (9): rx ry rz tx ty tz sx sy sz (%08x) ]

import java.nio.charset.StandardCharsets;
import java.util.Base64;
import net.minecraft.client.resources.model.cuboid.CuboidModel;
import net.minecraft.client.resources.model.cuboid.ItemTransform;
import net.minecraft.client.resources.model.cuboid.ItemTransforms;
import net.minecraft.world.item.ItemDisplayContext;
import org.joml.Vector3fc;

public class ItemTransformsParity {
    static final java.io.PrintStream O = System.out;
    static String f(float x) { return String.format("%08x", Float.floatToRawIntBits(x)); }

    // Query order MUST match the C++ ITEM_TRANSFORM_NAMES order.
    static final ItemDisplayContext[] CTX = {
        ItemDisplayContext.THIRD_PERSON_RIGHT_HAND, ItemDisplayContext.THIRD_PERSON_LEFT_HAND,
        ItemDisplayContext.FIRST_PERSON_RIGHT_HAND, ItemDisplayContext.FIRST_PERSON_LEFT_HAND,
        ItemDisplayContext.HEAD, ItemDisplayContext.GUI, ItemDisplayContext.GROUND,
        ItemDisplayContext.FIXED, ItemDisplayContext.ON_SHELF};

    static void emit(String displayBlock) {
        CuboidModel m = CuboidModel.fromStream(new java.io.StringReader("{\"display\":" + displayBlock + "}"));
        ItemTransforms tf = m.transforms();
        StringBuilder sb = new StringBuilder("IT\t");
        sb.append(Base64.getEncoder().encodeToString(displayBlock.getBytes(StandardCharsets.UTF_8)));
        for (ItemDisplayContext c : CTX) {
            ItemTransform it = tf.getTransform(c);
            Vector3fc r = it.rotation(), t = it.translation(), s = it.scale();
            sb.append('\t').append(f(r.x())).append('\t').append(f(r.y())).append('\t').append(f(r.z()));
            sb.append('\t').append(f(t.x())).append('\t').append(f(t.y())).append('\t').append(f(t.z()));
            sb.append('\t').append(f(s.x())).append('\t').append(f(s.y())).append('\t').append(f(s.z()));
        }
        O.println(sb);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // 1: a typical full item display (every context, varied values incl. the classic gui rotation).
        emit("""
        {
          "thirdperson_righthand": {"rotation":[0,90,0],"translation":[0,1,0],"scale":[0.55,0.55,0.55]},
          "thirdperson_lefthand": {"rotation":[0,-90,0],"translation":[0,1,0],"scale":[0.55,0.55,0.55]},
          "firstperson_righthand": {"rotation":[0,45,0],"translation":[0,4,2],"scale":[0.68,0.68,0.68]},
          "firstperson_lefthand": {"rotation":[0,225,0],"translation":[0,4,2],"scale":[0.68,0.68,0.68]},
          "head": {"rotation":[0,180,0],"translation":[0,13,7],"scale":[1,1,1]},
          "gui": {"rotation":[30,225,0],"translation":[0,0,0],"scale":[0.625,0.625,0.625]},
          "ground": {"rotation":[0,0,0],"translation":[0,3,0],"scale":[0.25,0.25,0.25]},
          "fixed": {"rotation":[0,0,0],"translation":[0,0,0],"scale":[0.5,0.5,0.5]},
          "on_shelf": {"rotation":[0,90,0],"translation":[0,0,-2],"scale":[0.4,0.4,0.4]}
        }""");
        // 2: partial — only gui + ground set; others default to NO_TRANSFORM.
        emit("{\"gui\": {\"rotation\":[15,-15,0],\"scale\":[0.6,0.6,0.6]}, \"ground\": {\"translation\":[0,2,0]}}");
        // 3: clamping — translation 96*0.0625=6 -> clamp 5; scale 7 -> clamp 4; negatives too.
        emit("{\"head\": {\"translation\":[96,-96,40],\"scale\":[7,-7,2.5]}}");
        // 4: only-rotation, only-translation, only-scale (defaults fill the rest).
        emit("{\"fixed\": {\"rotation\":[22.5,45,-90]}, \"gui\": {\"translation\":[8,8,8]}, \"ground\": {\"scale\":[2,3,4]}}");
        // 5: empty display -> all NO_TRANSFORM.
        emit("{}");
        // 6: fractional translation (16 -> *0.0625 = 1.0 exact; 5 -> 0.3125 exact).
        emit("{\"firstperson_righthand\": {\"rotation\":[0,0,0],\"translation\":[16,5,-16],\"scale\":[1,0.5,0.25]}}");
    }
}
