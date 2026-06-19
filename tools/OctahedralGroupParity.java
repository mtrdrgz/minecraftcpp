// Ground-truth generator for com.mojang.math.OctahedralGroup + SymmetricGroup3 (the
// 48-element octahedral symmetry group used by Rotation/Mirror.rotation() and block
// model baking). Pure enum/permutation math. The C++ port (render/model/McMath)
// must agree on compose (cayley table + enum ordering) and rotate(Direction).
//
//   tools/run_groundtruth.ps1 -Tool OctahedralGroupParity -Out mcpp/build/octahedral.tsv

import com.mojang.math.OctahedralGroup;
import com.mojang.math.SymmetricGroup3;
import net.minecraft.core.Direction;

public class OctahedralGroupParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) {
        SymmetricGroup3[] sym = SymmetricGroup3.values();
        for (SymmetricGroup3 f : sym) {
            for (int i = 0; i < 3; i++) O.println("SYM3P\t" + f.ordinal() + "\t" + i + "\t" + f.permute(i));
            for (SymmetricGroup3 s : sym) O.println("SYM3C\t" + f.ordinal() + "\t" + s.ordinal() + "\t" + f.compose(s).ordinal());
        }

        OctahedralGroup[] octa = OctahedralGroup.values();
        O.println("OCTALEN\t" + octa.length);
        Direction[] dirs = Direction.values();
        for (OctahedralGroup g : octa) {
            for (Direction d : dirs) O.println("OCTA_R\t" + g.ordinal() + "\t" + d.ordinal() + "\t" + g.rotate(d).ordinal());
            for (OctahedralGroup s : octa) O.println("OCTA_C\t" + g.ordinal() + "\t" + s.ordinal() + "\t" + g.compose(s).ordinal());
            O.println("OCTA_INV\t" + g.ordinal() + "\t" + g.inverse().ordinal());
        }
    }
}
