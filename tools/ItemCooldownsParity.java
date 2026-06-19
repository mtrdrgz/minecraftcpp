// Ground-truth generator for net.minecraft.world.item.ItemCooldowns (26.1.2).
//
// We drive a REAL ItemCooldowns instance through a deterministic program of
// operations (ADD group time / TICK / QUERY group partialTick), then emit, for
// every QUERY, the cooldown percentage and the on-cooldown boolean.
//
//  * addCooldown(Identifier, int) and tick() are PUBLIC and have empty
//    onCooldownStarted/onCooldownEnded hooks, so we call them directly on the
//    real object — its private `cooldowns` HashMap and `tickCount` mutate exactly
//    as in vanilla.
//  * getCooldownPercent(ItemStack, float) / isOnCooldown(ItemStack) take an
//    ItemStack and resolve a group via the live Item registry + UseCooldown data
//    component, which we deliberately do NOT exercise (registry/component-coupled).
//    Instead we replicate the *body* of getCooldownPercent VERBATIM
//    (ItemCooldowns.java:24-30) against the REAL net.minecraft.util.Mth.clamp and
//    the REAL instance's reflected state (cooldowns map, tickCount, and the
//    CooldownInstance record's startTime/endTime). The arithmetic and the clamp
//    are byte-identical to what getCooldownPercent would compute for the same
//    resolved group; only the ItemStack->group resolution is bypassed.
//
//   tools/run_groundtruth.ps1 -Tool ItemCooldownsParity -Out mcpp/build/item_cooldowns.tsv

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Map;

import net.minecraft.resources.Identifier;
import net.minecraft.util.Mth;
import net.minecraft.world.item.ItemCooldowns;

public class ItemCooldownsParity {
    static final java.io.PrintStream O = System.out;

    static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // Distinct real Identifier keys, one per integer group id used in the program.
    static Identifier[] GROUPS;

    // Reflected access to ItemCooldowns private state + the CooldownInstance record.
    static Field fCooldowns;     // Map<Identifier, CooldownInstance>
    static Field fTickCount;     // int
    static Field fStartTime;     // CooldownInstance.startTime
    static Field fEndTime;       // CooldownInstance.endTime
    static Method mAddGroup;     // addCooldown(Identifier, int)
    static Method mTick;         // tick()

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Build distinct group identifiers id 0..7.
        GROUPS = new Identifier[8];
        for (int g = 0; g < GROUPS.length; g++) {
            GROUPS[g] = Identifier.withDefaultNamespace("parity_group_" + g);
        }

        fCooldowns = ItemCooldowns.class.getDeclaredField("cooldowns"); fCooldowns.setAccessible(true);
        fTickCount = ItemCooldowns.class.getDeclaredField("tickCount"); fTickCount.setAccessible(true);

        // ItemCooldowns.CooldownInstance is a private nested record.
        Class<?> ci = null;
        for (Class<?> c : ItemCooldowns.class.getDeclaredClasses()) {
            if (c.getSimpleName().equals("CooldownInstance")) { ci = c; break; }
        }
        if (ci == null) throw new IllegalStateException("CooldownInstance not found");
        fStartTime = ci.getDeclaredField("startTime"); fStartTime.setAccessible(true);
        fEndTime   = ci.getDeclaredField("endTime");   fEndTime.setAccessible(true);

        mAddGroup = ItemCooldowns.class.getDeclaredMethod("addCooldown", Identifier.class, int.class);
        mAddGroup.setAccessible(true);
        mTick = ItemCooldowns.class.getDeclaredMethod("tick");
        mTick.setAccessible(true);

        // ── A battery of deterministic programs ──────────────────────────────
        // Each program is its own ItemCooldowns instance. The C++ test replays
        // the SAME op stream (OP rows) and must reproduce every QUERY row.

        // Partial-tick samples used at QUERY points (finite, in/around [0,1]).
        float[] PT = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };

        int prog = 0;

        // Program 0: single group, short cooldown, query every tick around expiry.
        prog = startProgram(prog);
        {
            ItemCooldowns ic = new ItemCooldowns();
            add(prog, ic, 0, 10);
            for (int t = 0; t <= 14; t++) {
                for (float pt : PT) query(prog, ic, 0, pt);
                tick(prog, ic);
            }
            for (float pt : PT) query(prog, ic, 0, pt);
            // query a never-added group -> 0
            for (float pt : PT) query(prog, ic, 5, pt);
        }

        // Program 1: re-add (overwrite) before expiry; longer duration.
        prog = startProgram(prog);
        {
            ItemCooldowns ic = new ItemCooldowns();
            add(prog, ic, 1, 40);
            for (int t = 0; t < 20; t++) tick(prog, ic);
            for (float pt : PT) query(prog, ic, 1, pt);
            add(prog, ic, 1, 100);                 // overwrite: new start=tickCount
            for (float pt : PT) query(prog, ic, 1, pt);
            for (int t = 0; t < 50; t++) { tick(prog, ic); if ((t % 7) == 0) for (float pt : PT) query(prog, ic, 1, pt); }
        }

        // Program 2: many groups with different durations interleaved.
        prog = startProgram(prog);
        {
            ItemCooldowns ic = new ItemCooldowns();
            add(prog, ic, 0, 5);
            add(prog, ic, 1, 13);
            add(prog, ic, 2, 1);
            add(prog, ic, 3, 64);
            add(prog, ic, 4, 200);
            for (int t = 0; t <= 70; t++) {
                if ((t % 3) == 0) for (int g = 0; g <= 5; g++) for (float pt : PT) query(prog, ic, g, pt);
                tick(prog, ic);
            }
        }

        // Program 3: removeCooldown mid-flight, then re-add.
        prog = startProgram(prog);
        {
            ItemCooldowns ic = new ItemCooldowns();
            add(prog, ic, 2, 30);
            for (int t = 0; t < 10; t++) tick(prog, ic);
            for (float pt : PT) query(prog, ic, 2, pt);
            remove(prog, ic, 2);
            for (float pt : PT) query(prog, ic, 2, pt);
            add(prog, ic, 2, 8);
            for (int t = 0; t < 12; t++) { for (float pt : PT) query(prog, ic, 2, pt); tick(prog, ic); }
        }

        // Program 4: time=0 and time=1 edge durations (instant / one-tick).
        prog = startProgram(prog);
        {
            ItemCooldowns ic = new ItemCooldowns();
            add(prog, ic, 6, 0);                   // endTime==startTime: duration 0
            for (float pt : PT) query(prog, ic, 6, pt);
            tick(prog, ic);                        // endTime<=tickCount -> swept
            for (float pt : PT) query(prog, ic, 6, pt);
            add(prog, ic, 7, 1);
            for (float pt : PT) query(prog, ic, 7, pt);
            tick(prog, ic);
            for (float pt : PT) query(prog, ic, 7, pt);
        }

        // Program 5: large duration + long tick run (exercise bigger float magnitudes).
        prog = startProgram(prog);
        {
            ItemCooldowns ic = new ItemCooldowns();
            add(prog, ic, 0, 1000);
            for (int t = 0; t < 1000; t++) { if ((t % 97) == 0) for (float pt : PT) query(prog, ic, 0, pt); tick(prog, ic); }
            for (float pt : PT) query(prog, ic, 0, pt);
        }
    }

    // ── Emit OP rows (the replayable program) + QUERY ground-truth rows ──────

    static int startProgram(int prog) { return prog; }

    static void add(int prog, ItemCooldowns ic, int group, int time) throws Exception {
        mAddGroup.invoke(ic, GROUPS[group], time);
        O.println("OP\t" + prog + "\tADD\t" + group + "\t" + time);
    }

    static void remove(int prog, ItemCooldowns ic, int group) {
        ic.removeCooldown(GROUPS[group]);  // public
        O.println("OP\t" + prog + "\tREMOVE\t" + group + "\t0");
    }

    static void tick(int prog, ItemCooldowns ic) throws Exception {
        mTick.invoke(ic);
        O.println("OP\t" + prog + "\tTICK\t0\t0");
    }

    // Replicates getCooldownPercent(ItemStack, float) body (ItemCooldowns.java:21-31)
    // for an already-resolved group, against the REAL Mth.clamp + reflected state.
    @SuppressWarnings("unchecked")
    static void query(int prog, ItemCooldowns ic, int group, float partialTick) throws Exception {
        Map<Identifier, Object> map = (Map<Identifier, Object>) fCooldowns.get(ic);
        int tickCount = fTickCount.getInt(ic);
        Object cooldown = map.get(GROUPS[group]);
        float percent;
        if (cooldown != null) {
            int startTime = fStartTime.getInt(cooldown);
            int endTime   = fEndTime.getInt(cooldown);
            float duration  = endTime - startTime;
            float remaining = endTime - (tickCount + partialTick);
            percent = Mth.clamp(remaining / duration, 0.0F, 1.0F);
        } else {
            percent = 0.0F;
        }
        // isOnCooldown(item) (ItemCooldowns.java:17-18) is getCooldownPercent(item, 0.0F) > 0.0F:
        // it ALWAYS evaluates at partialTick=0.0F, NOT the query's partialTick. Recompute the
        // percent at 0.0F so onCd matches the real isOnCooldown rather than the partialTick percent.
        float onCdPercent;
        if (cooldown != null) {
            int startTime0 = fStartTime.getInt(cooldown);
            int endTime0   = fEndTime.getInt(cooldown);
            float duration0  = endTime0 - startTime0;
            float remaining0 = endTime0 - (tickCount + 0.0F);
            onCdPercent = Mth.clamp(remaining0 / duration0, 0.0F, 1.0F);
        } else {
            onCdPercent = 0.0F;
        }
        boolean onCd = onCdPercent > 0.0F;  // isOnCooldown (ItemCooldowns.java:17-18)
        // QUERY <prog> <group> <partialTickBits> <percentBits> <onCooldown>
        O.println("QUERY\t" + prog + "\t" + group + "\t" + f(partialTick) + "\t" + f(percent) + "\t" + (onCd ? 1 : 0));
    }
}
