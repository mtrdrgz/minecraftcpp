// Ground truth for net.minecraft.client.resources.model.ResolvedModel parent-chain resolution
// (ResolvedModel.java:21-86): findTopAmbientOcclusion / findTopGuiLight / findTopGeometry /
// findTopTextureSlots. Drives the REAL static methods with inline ResolvedModel + UnbakedModel
// stubs (both are interfaces with default/overridable methods); no GL/JSON. Serializes the
// chain so the C++ rebuilds it.
//
//   tools/run_groundtruth.ps1 -Tool ResolvedModelResolveParity -Out mcpp/build/resolved_model.tsv
//
// Per case (tab-separated; nodes in top..root order = this, parent, ..., root):
//   CASE
//   NODE  hasAO aoVal  hasGL glOrd  hasGeom geomId
//   V slot spriteId ft        # this node's textureSlots() layer
//   R slot target
//   ENDNODE
//   ...(more NODE/.../ENDNODE)...
//   RESULT  aoResult(0/1)  glOrd  geomId(-1=EMPTY)
//   LK  name present spriteId ft     # findTopTextureSlots() lookups
//   END

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import net.minecraft.client.resources.model.ModelDebugName;
import net.minecraft.client.resources.model.ResolvedModel;
import net.minecraft.client.resources.model.UnbakedModel;
import net.minecraft.client.resources.model.geometry.QuadCollection;
import net.minecraft.client.resources.model.geometry.UnbakedGeometry;
import net.minecraft.client.resources.model.sprite.Material;
import net.minecraft.client.resources.model.sprite.TextureSlots;
import net.minecraft.resources.Identifier;
import org.jspecify.annotations.Nullable;

public class ResolvedModelResolveParity {
    static final java.io.PrintStream O = System.out;

    // Node spec (top..root order).
    record Slot(String slot, boolean isRef, String data, boolean ft) {}
    record NodeSpec(@Nullable Boolean ao, UnbakedModel.@Nullable GuiLight gl, boolean hasGeom, List<Slot> slots) {}

    static Slot val(String slot, String id, boolean ft) { return new Slot(slot, false, id, ft); }
    static Slot ref(String slot, String target) { return new Slot(slot, true, target, false); }

    static UnbakedModel model(final NodeSpec spec, final UnbakedGeometry geom) {
        TextureSlots.Data.Builder b = new TextureSlots.Data.Builder();
        for (Slot s : spec.slots()) {
            if (s.isRef()) b.addReference(s.slot(), s.data());
            else b.addTexture(s.slot(), new Material(Identifier.parse(s.data()), s.ft()));
        }
        TextureSlots.Data data = b.build();
        return new UnbakedModel() {
            @Override public @Nullable Boolean ambientOcclusion() { return spec.ao(); }
            @Override public UnbakedModel.@Nullable GuiLight guiLight() { return spec.gl(); }
            @Override public @Nullable UnbakedGeometry geometry() { return geom; }
            @Override public TextureSlots.Data textureSlots() { return data; }
        };
    }

    static ResolvedModel resolved(final UnbakedModel w, final @Nullable ResolvedModel parent) {
        return new ResolvedModel() {
            @Override public UnbakedModel wrapped() { return w; }
            @Override public @Nullable ResolvedModel parent() { return parent; }
            @Override public String debugName() { return "test"; }
        };
    }

    static void emit(List<NodeSpec> nodes) {
        O.println("CASE");
        // build geometry stubs (distinct identity per node that declares one) + chain root..top
        UnbakedGeometry[] geoms = new UnbakedGeometry[nodes.size()];
        for (int i = 0; i < nodes.size(); i++)
            geoms[i] = nodes.get(i).hasGeom() ? (t, b, s, n) -> QuadCollection.EMPTY : null;

        ResolvedModel parent = null;
        ResolvedModel[] chainBottomUp = new ResolvedModel[nodes.size()];
        // nodes are top..root; build from root (last) up to top (first)
        for (int i = nodes.size() - 1; i >= 0; i--) {
            UnbakedModel m = model(nodes.get(i), geoms[i]);
            parent = resolved(m, parent);
            chainBottomUp[i] = parent;
        }
        ResolvedModel top = nodes.isEmpty() ? null : chainBottomUp[0];

        // serialize nodes (top..root)
        for (int i = 0; i < nodes.size(); i++) {
            NodeSpec sp = nodes.get(i);
            O.println("NODE\t" + (sp.ao() != null ? 1 : 0) + "\t" + (sp.ao() != null && sp.ao() ? 1 : 0)
                + "\t" + (sp.gl() != null ? 1 : 0) + "\t" + (sp.gl() != null ? sp.gl().ordinal() : -1)
                + "\t" + (sp.hasGeom() ? 1 : 0) + "\t" + (sp.hasGeom() ? i : -1));
            for (Slot s : sp.slots()) {
                if (s.isRef()) O.println("R\t" + s.slot() + "\t" + s.data());
                else {
                    Material m = new Material(Identifier.parse(s.data()), s.ft());
                    O.println("V\t" + s.slot() + "\t" + m.sprite().toString() + "\t" + (s.ft() ? 1 : 0));
                }
            }
            O.println("ENDNODE");
        }

        // drive the REAL methods
        boolean ao = top != null ? ResolvedModel.findTopAmbientOcclusion(top) : true;
        UnbakedModel.GuiLight gl = top != null ? ResolvedModel.findTopGuiLight(top) : UnbakedModel.GuiLight.SIDE;
        UnbakedGeometry geom = top != null ? ResolvedModel.findTopGeometry(top) : UnbakedGeometry.EMPTY;
        int geomId = -1;
        for (int i = 0; i < geoms.length; i++) if (geoms[i] == geom) { geomId = i; break; }
        O.println("RESULT\t" + (ao ? 1 : 0) + "\t" + gl.ordinal() + "\t" + geomId);

        // findTopTextureSlots lookups
        TextureSlots slots = top != null ? ResolvedModel.findTopTextureSlots(top) : TextureSlots.EMPTY;
        Set<String> names = new LinkedHashSet<>();
        for (NodeSpec sp : nodes) for (Slot s : sp.slots()) names.add(s.slot());
        for (String name : new ArrayList<>(names)) {
            Material m = slots.getMaterial(name);
            if (m == null) O.println("LK\t" + name + "\t0\t-\t0");
            else O.println("LK\t" + name + "\t1\t" + m.sprite().toString() + "\t" + (m.forceTranslucent() ? 1 : 0));
        }
        O.println("END");
    }

    static UnbakedModel.GuiLight FRONT = UnbakedModel.GuiLight.FRONT;
    static UnbakedModel.GuiLight SIDE = UnbakedModel.GuiLight.SIDE;

    public static void main(String[] args) throws Exception {
        // Case 1: single node with everything set.
        emit(List.of(new NodeSpec(false, FRONT, true,
            List.of(val("all", "block/stone", true), ref("particle", "all")))));

        // Case 2: child inherits ao/gl/geometry from parent (child sets none).
        emit(List.of(
            new NodeSpec(null, null, false, List.of(ref("top", "side"))),        // top (child)
            new NodeSpec(true, SIDE, true, List.of(val("side", "block/dirt", false)))));  // parent

        // Case 3: three-deep; geometry only on the middle, ao only on root, gl only on top.
        emit(List.of(
            new NodeSpec(null, FRONT, false, List.of(val("a", "block/sand", false))),     // top
            new NodeSpec(null, null, true, List.of(ref("b", "a"), val("c", "block/glass", true))),  // mid (geometry)
            new NodeSpec(false, null, false, List.of(val("d", "block/cobblestone", false)))));        // root (ao)

        // Case 4: nothing set anywhere -> defaults (ao=true, gl=SIDE, geometry EMPTY).
        emit(List.of(
            new NodeSpec(null, null, false, List.of(ref("x", "y"))),
            new NodeSpec(null, null, false, List.of())));

        // Case 5: child geometry overrides parent geometry (top wins); texture override chain.
        emit(List.of(
            new NodeSpec(true, SIDE, true, List.of(val("base", "block/quartz", false), ref("wall", "base"))),  // top (geometry id 0)
            new NodeSpec(false, FRONT, true, List.of(val("base", "block/andesite", true)))));                    // parent (geometry id 1, not chosen)

        // Case 6: empty (no nodes) -> all defaults, no lookups.
        emit(List.of());
    }
}
