// Ground truth for net.minecraft.client.resources.model.sprite.TextureSlots.Resolver.resolve +
// TextureSlots.getMaterial (TextureSlots.java:33-39, 88-162) — model texture-slot resolution
// (the '#ref' indirection fixpoint over a stack of Data layers). Pure data; no GL/JSON needed
// (Data built via the public Data.Builder). Serializes the layer stack as inputs so the C++
// rebuilds the resolver; sprite ids emitted normalized (Identifier.toString()) on both sides.
//
//   tools/run_groundtruth.ps1 -Tool TextureSlotsResolveParity -Out mcpp/build/texture_slots.tsv
//
// Per case (tab-separated; layers in this.entries / addLast order):
//   CASE
//   LAYER
//   V  slot  spriteId  forceTranslucent      # Data.Builder.addTexture
//   R  slot  target                          # Data.Builder.addReference
//   ...(more V/R, more LAYER)...
//   LOOKUPS
//   LK  name  present(0/1)  spriteId  forceTranslucent(0/1)
//   ...
//   END

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;
import net.minecraft.client.resources.model.ModelDebugName;
import net.minecraft.client.resources.model.sprite.Material;
import net.minecraft.client.resources.model.sprite.TextureSlots;
import net.minecraft.resources.Identifier;

public class TextureSlotsResolveParity {
    static final java.io.PrintStream O = System.out;
    static final ModelDebugName NAME = () -> "test";

    // A layer spec: list of (slot, isRef, target/spriteId, forceTranslucent).
    record Slot(String slot, boolean isRef, String data, boolean ft) {}

    static void emit(List<List<Slot>> layers, List<String> lookups) {
        O.println("CASE");
        TextureSlots.Resolver resolver = new TextureSlots.Resolver();
        for (List<Slot> layer : layers) {
            O.println("LAYER");
            TextureSlots.Data.Builder b = new TextureSlots.Data.Builder();
            for (Slot s : layer) {
                if (s.isRef) {
                    b.addReference(s.slot, s.data);
                    O.println("R\t" + s.slot + "\t" + s.data);
                } else {
                    Material m = new Material(Identifier.parse(s.data), s.ft);
                    b.addTexture(s.slot, m);
                    O.println("V\t" + s.slot + "\t" + m.sprite().toString() + "\t" + (s.ft ? 1 : 0));
                }
            }
            resolver.addLast(b.build());
        }
        TextureSlots resolved = resolver.resolve(NAME);
        O.println("LOOKUPS");
        for (String name : lookups) {
            Material m = resolved.getMaterial(name);
            if (m == null) O.println("LK\t" + name + "\t0\t-\t0");
            else O.println("LK\t" + name + "\t1\t" + m.sprite().toString() + "\t" + (m.forceTranslucent() ? 1 : 0));
        }
        O.println("END");
    }

    static Slot val(String slot, String id, boolean ft) { return new Slot(slot, false, id, ft); }
    static Slot ref(String slot, String target) { return new Slot(slot, true, target, false); }

    static List<String> names(List<List<Slot>> layers, String... extra) {
        Set<String> set = new LinkedHashSet<>();
        for (List<Slot> l : layers) for (Slot s : l) set.add(s.slot);
        for (String e : extra) set.add(e);
        return new ArrayList<>(set);
    }

    public static void main(String[] args) throws Exception {
        // Case A: single layer, direct values + an in-layer reference.
        {
            List<List<Slot>> layers = List.of(List.of(
                val("stone", "block/stone", false),
                val("top", "block/oak_log_top", true),
                ref("particle", "stone")));
            emit(layers, names(layers, "#stone", "missing"));
        }
        // Case B: parent (L0) + child (L1) override; child Value overrides parent Reference,
        // child Reference overrides parent Value.
        {
            List<List<Slot>> layers = List.of(
                List.of(val("a", "block/dirt", false), ref("b", "a"), val("c", "block/sand", false)),
                List.of(val("b", "block/grass", true), ref("c", "a")));
            emit(layers, names(layers));
        }
        // Case C: chained references a->b->c->Value, spread across two layers.
        {
            List<List<Slot>> layers = List.of(
                List.of(val("d", "block/cobblestone", false), ref("c", "d")),
                List.of(ref("b", "c"), ref("a", "b"), val("e", "block/glass", true)));
            emit(layers, names(layers, "#a"));
        }
        // Case D: unresolvable reference (target missing) + a 2-cycle (x->y, y->x).
        {
            List<List<Slot>> layers = List.of(List.of(
                val("ok", "block/iron_block", false),
                ref("dangling", "nope"),
                ref("x", "y"),
                ref("y", "x")));
            emit(layers, names(layers, "nope"));
        }
        // Case E: three layers, deep override + reference re-pointing, forceTranslucent flips.
        {
            List<List<Slot>> layers = List.of(
                List.of(val("base", "block/stone", false), ref("wall", "base")),
                List.of(val("base", "block/andesite", true)),
                List.of(ref("wall", "trim"), val("trim", "block/quartz", false)));
            emit(layers, names(layers, "#wall", "#base", "#trim"));
        }
        // Case F: empty resolver (no layers) -> EMPTY; lookups all absent.
        {
            emit(List.of(), List.of("anything", "#x"));
        }
    }
}
