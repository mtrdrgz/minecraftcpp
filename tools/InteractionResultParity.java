import net.minecraft.world.InteractionResult;
import net.minecraft.world.item.ItemStack;

// Ground-truth dumper for net.minecraft.world.InteractionResult (MC 26.1.2).
// Emits tab-separated rows consumed by InteractionResultParityTest.cpp.
//
// All values come from REAL net.minecraft.world.InteractionResult constants/methods.
//
// TAGS:
//   SWING   <ordinal> <name>
//       — every InteractionResult.SwingSource constant.
//   CTX     <which> <wasItemInteraction> <heldPresent>
//       — ItemContext.NONE / ItemContext.DEFAULT booleans (held==null -> 0).
//   CONST   <id> <isSuccess> <consumesAction> <swingOrd|-1> <wasItemInteraction|-1> <heldPresent|-1>
//       — the six predefined InteractionResult constants. For non-Success kinds
//         the swing/item fields are -1 (no such accessor exists in Java).
//   XFORM   <which> <consumesAction> <swingOrd> <wasItemInteraction> <heldPresent>
//       — Success transformations: withoutItem() and heldItemTransformedTo(stack).
@SuppressWarnings({"unchecked", "deprecation"})
public class InteractionResultParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // ItemStack.EMPTY (used for the heldItemTransformedTo transform) touches the
        // registry, so bootstrap first.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // --- SwingSource enum: ordinal + name for every constant. ---
        for (InteractionResult.SwingSource s : InteractionResult.SwingSource.values()) {
            O.println("SWING\t" + s.ordinal() + "\t" + s.name());
        }

        // --- ItemContext.NONE / DEFAULT: reflect the package-private static fields. ---
        Class<?> ctxCls = Class.forName("net.minecraft.world.InteractionResult$ItemContext");
        java.lang.reflect.Field fNone = ctxCls.getDeclaredField("NONE");
        java.lang.reflect.Field fDefault = ctxCls.getDeclaredField("DEFAULT");
        fNone.setAccessible(true);
        fDefault.setAccessible(true);
        Object ctxNone = fNone.get(null);
        Object ctxDefault = fDefault.get(null);
        // record accessors wasItemInteraction() and heldItemTransformedTo()
        java.lang.reflect.Method ctxWasItem = ctxCls.getDeclaredMethod("wasItemInteraction");
        java.lang.reflect.Method ctxHeld = ctxCls.getDeclaredMethod("heldItemTransformedTo");
        ctxWasItem.setAccessible(true);
        ctxHeld.setAccessible(true);
        O.println("CTX\tNONE\t" + b((Boolean) ctxWasItem.invoke(ctxNone))
                + "\t" + b(ctxHeld.invoke(ctxNone) != null));
        O.println("CTX\tDEFAULT\t" + b((Boolean) ctxWasItem.invoke(ctxDefault))
                + "\t" + b(ctxHeld.invoke(ctxDefault) != null));

        // --- The six predefined InteractionResult constants. ---
        emitConst("SUCCESS", InteractionResult.SUCCESS);
        emitConst("SUCCESS_SERVER", InteractionResult.SUCCESS_SERVER);
        emitConst("CONSUME", InteractionResult.CONSUME);
        emitConst("FAIL", InteractionResult.FAIL);
        emitConst("PASS", InteractionResult.PASS);
        emitConst("TRY_WITH_EMPTY_HAND", InteractionResult.TRY_WITH_EMPTY_HAND);

        // --- Success transformations. Start from CONSUME (SwingSource.NONE) so the
        //     preserved swingSource is observable in the result. ---
        InteractionResult.Success base = InteractionResult.CONSUME; // NONE / DEFAULT
        // withoutItem(): new Success(swingSource, ItemContext.NONE) -> wasItem=false.
        InteractionResult.Success without = base.withoutItem();
        emitXform("WITHOUT_ITEM", without);
        // heldItemTransformedTo(stack): non-null stack -> wasItem=true, held!=null.
        InteractionResult.Success transformed = base.heldItemTransformedTo(ItemStack.EMPTY);
        emitXform("HELD_TRANSFORMED", transformed);
    }

    static void emitConst(String id, InteractionResult r) {
        boolean isSuccess = r instanceof InteractionResult.Success;
        int swingOrd = -1;
        int wasItem = -1;
        int held = -1;
        if (isSuccess) {
            InteractionResult.Success s = (InteractionResult.Success) r;
            swingOrd = s.swingSource().ordinal();
            wasItem = s.wasItemInteraction() ? 1 : 0;
            held = (s.heldItemTransformedTo() != null) ? 1 : 0;
        }
        O.println("CONST\t" + id + "\t" + b(isSuccess) + "\t" + b(r.consumesAction())
                + "\t" + swingOrd + "\t" + wasItem + "\t" + held);
    }

    static void emitXform(String which, InteractionResult.Success s) {
        O.println("XFORM\t" + which + "\t" + b(s.consumesAction())
                + "\t" + s.swingSource().ordinal()
                + "\t" + b(s.wasItemInteraction())
                + "\t" + b(s.heldItemTransformedTo() != null));
    }

    static int b(boolean v) { return v ? 1 : 0; }
}
