import net.minecraft.world.entity.player.ChatVisiblity;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.contents.TranslatableContents;
import java.lang.reflect.Field;
import java.util.function.IntFunction;

// Ground-truth dumper for net.minecraft.world.entity.player.ChatVisiblity (MC 26.1.2).
// Emits tab-separated rows consumed by ChatVisibilityParityTest.cpp.
//
// TAGS:
//   CONST  <ordinal> <name()> <id field> <translation key>
//   COUNT  <values().length>
//   BYID   <inputId> <ordinal of BY_ID.apply(inputId)>   (LEGACY_CODEC decode, WRAP)
//
// ordinal / id / count / inputId are decimal; name() and the translation key are raw
// strings. Every value comes from the REAL net.minecraft ChatVisiblity enum:
//   * `id` is private with no getter -> read via reflection (setAccessible).
//   * the key is consumed into a translatable Component -> recovered from
//     caption().getContents() as a TranslatableContents.getKey().
//   * the WRAP id->enum mapping is read from the private static BY_ID IntFunction
//     (which is exactly what LEGACY_CODEC decodes through).
public class ChatVisibilityParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings("unchecked")  // reflective IntFunction<ChatVisiblity> BY_ID cast (strict runner treats the javac warning as fatal)
    public static void main(String[] args) throws Exception {
        // Component.translatable() touches no registry, but bootstrap defensively in
        // case classloading pulls in registry-touching dependencies.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — ChatVisiblity does not require it
        }

        ChatVisiblity[] vals = ChatVisiblity.values();

        // private final int id;
        Field idField = ChatVisiblity.class.getDeclaredField("id");
        idField.setAccessible(true);

        for (ChatVisiblity v : vals) {
            int id = idField.getInt(v);
            // Recover the constructor `key` string from caption()'s TranslatableContents.
            Component caption = v.caption();
            String key = ((TranslatableContents) caption.getContents()).getKey();
            O.println("CONST"
                    + "\t" + v.ordinal()
                    + "\t" + v.name()
                    + "\t" + id
                    + "\t" + key);
        }
        O.println("COUNT" + "\t" + vals.length);

        // private static final IntFunction<ChatVisiblity> BY_ID; LEGACY_CODEC decodes
        // through BY_ID::apply. Read it via reflection and probe a range of ids
        // (including negatives) to certify the WRAP (Math.floorMod) behaviour.
        @SuppressWarnings("unchecked")
        Field byIdField = ChatVisiblity.class.getDeclaredField("BY_ID");
        byIdField.setAccessible(true);
        IntFunction<ChatVisiblity> byId = (IntFunction<ChatVisiblity>) byIdField.get(null);

        for (int id = -9; id <= 11; id++) {
            ChatVisiblity decoded = byId.apply(id);
            O.println("BYID"
                    + "\t" + id
                    + "\t" + decoded.ordinal());
        }
    }
}
