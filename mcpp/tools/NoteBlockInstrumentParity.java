import net.minecraft.world.level.block.state.properties.NoteBlockInstrument;

// Ground-truth dumper for
// net.minecraft.world.level.block.state.properties.NoteBlockInstrument (MC 26.1.2).
// Emits tab-separated rows consumed by NoteBlockInstrumentParityTest.cpp.
//
// TAGS:
//   CONST <ordinal> <serializedName> <isTunable> <hasCustomSound> <worksAboveNoteBlock>
//
// All four queried methods (getSerializedName, isTunable, hasCustomSound,
// worksAboveNoteBlock) are public, so they are called directly. The enum constants
// reference SoundEvents.* during class init, which can pull in the registry — so
// we bootstrap defensively first.
public class NoteBlockInstrumentParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // The enum's static initializer references SoundEvents.NOTE_BLOCK_* holders;
        // those classes touch the registry, so bootstrap to avoid "Not bootstrapped".
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — best effort; values() below still drives the dump
        }

        for (NoteBlockInstrument v : NoteBlockInstrument.values()) {
            O.println("CONST\t"
                    + v.ordinal() + "\t"
                    + v.getSerializedName() + "\t"
                    + (v.isTunable() ? 1 : 0) + "\t"
                    + (v.hasCustomSound() ? 1 : 0) + "\t"
                    + (v.worksAboveNoteBlock() ? 1 : 0));
        }
    }
}
