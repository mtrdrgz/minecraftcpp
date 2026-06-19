import net.minecraft.network.chat.HoverEvent;

// Ground-truth dumper for net.minecraft.network.chat.HoverEvent.Action (MC 26.1.2).
// Emits tab-separated rows consumed by HoverEventActionParityTest.cpp.
//
// TAGS:
//   CONST <ordinal> <name()> <getSerializedName> <isAllowedFromServer> <toString>
//
// All queried methods (ordinal, name, getSerializedName, isAllowedFromServer,
// toString) are public on the enum, so they are called directly. The enum's
// static initializer references HoverEvent.ShowText/ShowItem/ShowEntity .CODEC
// MapCodecs, which pull in DataFixerUpper + the component/registry machinery, so
// we bootstrap defensively first to avoid "Not bootstrapped".
public class HoverEventActionParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // best effort — values() below still drives the dump
        }

        for (HoverEvent.Action v : HoverEvent.Action.values()) {
            O.println("CONST\t"
                    + v.ordinal() + "\t"
                    + v.name() + "\t"
                    + v.getSerializedName() + "\t"
                    + (v.isAllowedFromServer() ? 1 : 0) + "\t"
                    + v.toString());
        }
    }
}
