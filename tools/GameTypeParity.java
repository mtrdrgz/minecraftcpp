import net.minecraft.world.level.GameType;

// Ground-truth dumper for net.minecraft.world.level.GameType (MC 26.1.2).
// Emits tab-separated rows consumed by GameTypeParityTest.cpp.
//
// Every value comes from the REAL net.minecraft.world.level.GameType enum via its
// public API (getId / getName / getSerializedName / isCreative / isSurvival /
// isBlockPlacingRestricted / byId / byName / getNullableId / byNullableId / isValidId).
//
// TAGS:
//   CONST     <ordinal> <name()> <getId()> <getName()> <getSerializedName()>
//             <isCreative> <isSurvival> <isBlockPlacingRestricted>
//   COUNT     <values().length>
//   BYID      <id> <byId(id).name()>
//   BYNAME    <input> <byName(input).name()>                 (1-arg; default SURVIVAL)
//   BYNAME2   <input> <defaultOrdinalOr-1> <result.name()|null>  (2-arg)
//   NULID     <ordinalOr-1> <getNullableId(arg)>             (arg=null when ord==-1)
//   BYNULID   <id> <byNullableId(id).name()|null>
//   VALIDID   <id> <isValidId(id)>
//
// ordinals/ids/counts decimal; booleans decimal (0/1 — printed via the int below);
// names/serialized ids raw strings; "null" literal for Java null.
public class GameTypeParity {
    static final java.io.PrintStream O = System.out;

    static int b(boolean x) { return x ? 1 : 0; }

    public static void main(String[] args) throws Exception {
        // GameType's ctor builds Component.translatable(...) shortName/longName, which can
        // pull in chat/registry classloading. Bootstrap defensively (harmless if unneeded).
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore
        }

        GameType[] vals = GameType.values();

        for (GameType v : vals) {
            O.println("CONST"
                    + "\t" + v.ordinal()
                    + "\t" + v.name()
                    + "\t" + v.getId()
                    + "\t" + v.getName()
                    + "\t" + v.getSerializedName()
                    + "\t" + b(v.isCreative())
                    + "\t" + b(v.isSurvival())
                    + "\t" + b(v.isBlockPlacingRestricted()));
        }

        O.println("COUNT" + "\t" + vals.length);

        // byId across in-range and out-of-range (ZERO strategy: OOB -> SURVIVAL).
        // Only finite/physical ints; include the boundaries that exercise the bound check.
        int[] ids = { -2147483648, -100, -2, -1, 0, 1, 2, 3, 4, 5, 100, 2147483647 };
        for (int id : ids) {
            O.println("BYID" + "\t" + id + "\t" + GameType.byId(id).name());
        }

        // byName(1-arg): matched serialized names + non-matches (default SURVIVAL),
        // plus case/whitespace variants that must NOT match (linear .equals scan).
        String[] names = {
            "survival", "creative", "adventure", "spectator",
            "SURVIVAL", "Survival", "", "nope", "surviv", "survival ", " survival",
            "record", "ui"
        };
        for (String n : names) {
            O.println("BYNAME" + "\t" + n + "\t" + GameType.byName(n).name());
        }

        // byName(2-arg): same inputs, with a few explicit defaults (incl. null default).
        GameType[] defs = { GameType.SURVIVAL, GameType.CREATIVE, GameType.SPECTATOR, null };
        for (String n : names) {
            for (GameType d : defs) {
                GameType r = GameType.byName(n, d);
                O.println("BYNAME2"
                        + "\t" + n
                        + "\t" + (d == null ? -1 : d.ordinal())
                        + "\t" + (r == null ? "null" : r.name()));
            }
        }

        // getNullableId: null -> -1, else getId().
        O.println("NULID" + "\t" + (-1) + "\t" + GameType.getNullableId(null));
        for (GameType v : vals) {
            O.println("NULID" + "\t" + v.ordinal() + "\t" + GameType.getNullableId(v));
        }

        // byNullableId: -1 -> null, else byId(id).
        int[] nids = { -1, -2, -100, 0, 1, 2, 3, 4, 100, 2147483647, -2147483648 };
        for (int id : nids) {
            GameType r = GameType.byNullableId(id);
            O.println("BYNULID" + "\t" + id + "\t" + (r == null ? "null" : r.name()));
        }

        // isValidId.
        int[] vids = { -2147483648, -100, -1, 0, 1, 2, 3, 4, 5, 100, 2147483647 };
        for (int id : vids) {
            O.println("VALIDID" + "\t" + id + "\t" + b(GameType.isValidId(id)));
        }

        // updatePlayerAbilities: gamemode -> player-ability flags. Start `flying` false AND true
        // to verify CREATIVE leaves it unchanged while SPECTATOR forces it true.
        //   ABIL <gtId> <startFlying> <mayfly> <instabuild> <invulnerable> <flying> <mayBuild>
        for (GameType v : vals) {
            for (boolean sf : new boolean[]{ false, true }) {
                net.minecraft.world.entity.player.Abilities ab = new net.minecraft.world.entity.player.Abilities();
                ab.flying = sf;
                v.updatePlayerAbilities(ab);
                O.println("ABIL" + "\t" + v.getId() + "\t" + b(sf)
                        + "\t" + b(ab.mayfly) + "\t" + b(ab.instabuild) + "\t" + b(ab.invulnerable)
                        + "\t" + b(ab.flying) + "\t" + b(ab.mayBuild));
            }
        }
    }
}
