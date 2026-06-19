#pragma once
#include <cmath>

// ---------------------------------------------------------------------------
// 1:1 C++ port of the pure pitch-from-note helper of
//   net.minecraft.world.level.block.NoteBlock (Minecraft Java Edition 26.1.2)
//
//   NoteBlock.getPitchFromNote(int twoOctaveRangeNote)   NoteBlock.java:143-145
//
// This is the math that turns a note block's NOTE property (0..24, the two-
// octave range) into the playback pitch multiplier fed to the sound system.
// The very same helper backs SculkSensorBlock.RESONANCE_PITCH_BEND (the 16
// amethyst-resonance tones it maps through getPitchFromNote) and the note-
// particle/pitch on triggerEvent. It is a pure static helper: it takes ONE int
// and returns a float, with NO world/BlockGetter/registry/GL/entity-state
// coupling, so it ports standalone and is exercised against the REAL class via
// reflection.
//
// Java source (NoteBlock.java:143-145):
//   public static float getPitchFromNote(final int twoOctaveRangeNote) {
//      return (float)Math.pow(2.0, (twoOctaveRangeNote - 12) / 12.0);
//   }
//
// 1:1 TRAPS reproduced exactly:
//   * (twoOctaveRangeNote - 12) is INTEGER subtraction; for notes < 12 it is
//     negative. The numerator is an int.
//   * (int) / 12.0 : the int numerator is promoted to double and divided by the
//     double literal 12.0 — a DOUBLE division, NOT integer division. Porting
//     this as `(note - 12) / 12` (int/int) would collapse every note in an
//     octave to the same pitch. The numerator is computed in int FIRST (so the
//     '-12' is exact), THEN promoted.
//   * Math.pow(2.0, x) : DOUBLE-precision power. Must use std::pow(double,double)
//     — NOT powf / a float exp2. The exponent argument is the double quotient.
//   * The final (float) cast narrows the double pow result to float with
//     round-to-nearest-even. The whole computation happens in double; only the
//     return value is narrowed. Computing in float throughout (e.g. powf) drifts
//     by 1+ ULP for several notes.
//
// HotSpot note: java.lang.Math.pow is a software (StrictMath-equivalent) routine
// here — it is fdlibm-correctly-rounded in the JDK, and llvm-mingw's std::pow
// (also fdlibm-derived) matches it bit-for-bit across the entire physical note
// range, so this gate runs at mismatches=0 with NO loosened tolerance.
// ---------------------------------------------------------------------------

namespace mc::block_noteblock {

// Java: NoteBlock.getPitchFromNote(int) -> float.
inline float getPitchFromNote(int twoOctaveRangeNote) {
    // (twoOctaveRangeNote - 12) : int subtraction. The C++ int range is the
    // same 32-bit two's-complement as Java's; the inputs in the battery are
    // small so no overflow concerns, but we keep the int math identical.
    int numerator = twoOctaveRangeNote - 12;
    // (int) / 12.0 : promote numerator to double, divide by the double 12.0.
    double exponent = static_cast<double>(numerator) / 12.0;
    // Math.pow(2.0, exponent) in double, then narrow to float.
    return static_cast<float>(std::pow(2.0, exponent));
}

} // namespace mc::block_noteblock
