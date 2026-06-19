import net.minecraft.network.chat.ClickEvent;

// Ground-truth dumper for net.minecraft.network.chat.ClickEvent.Action (MC 26.1.2).
// Emits tab-separated rows consumed by ClickEventActionParityTest.cpp.
//
// TAGS:
//   CONST  <ordinal> <name()> <getSerializedName()> <isAllowedFromServer 0|1>
//
// ordinal / the boolean are decimal; name() and getSerializedName() are raw
// strings. All values come from REAL public accessors on the enum
// (getSerializedName, isAllowedFromServer). The codec surface is intentionally
// NOT dumped (out of scope). No reflection needed — both accessors are public.
public class ClickEventActionParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // ClickEvent.Action is a plain StringRepresentable enum and needs no
        // registry bootstrap, but bootstrap defensively in case classloading
        // pulls in registry/codec-touching dependencies.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — ClickEvent.Action does not require it
        }

        for (ClickEvent.Action v : ClickEvent.Action.values()) {
            O.println("CONST"
                    + "\t" + v.ordinal()
                    + "\t" + v.name()
                    + "\t" + v.getSerializedName()
                    + "\t" + (v.isAllowedFromServer() ? 1 : 0));
        }
    }
}
