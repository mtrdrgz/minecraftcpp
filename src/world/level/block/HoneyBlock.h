#pragma once

// 1:1 port of the pure vertical-velocity math in
// net.minecraft.world.level.block.HoneyBlock (Minecraft 26.1.2).
//
// HoneyBlock governs how an entity slides down the side of a honey block. The two
// private statics below convert between the "real" per-tick deltaY and the value
// used by the slide logic. They are the only world-free, primitive-in/primitive-out
// arithmetic in the class, and they carry a genuine 1:1 fidelity trap: the literal
// 0.98F is a *float*, but it appears inside a *double* expression, so Java widens it
// to double (0.9800000190734863) BEFORE the divide/multiply. Porting it as a plain
// double 0.98 would silently diverge. We therefore keep the constant as a float and
// rely on the same float->double widening C++ performs for `deltaY / 0.98f`.
//
//   HoneyBlock.java:76-78  getOldDeltaY(double deltaY):
//       return deltaY / 0.98F + 0.08;
//   HoneyBlock.java:80-82  getNewDeltaY(double deltaY):
//       return (deltaY - 0.08) * 0.98F;
//
// Here 0.08 is a double literal, so it stays exactly 0.08 (no widening question).
//
// Certified bit-for-bit by honey_block_parity (ground truth: tools/HoneyBlockParity.java
// vs the real net.minecraft.world.level.block.HoneyBlock private statics, invoked
// reflectively). The slide-decision thresholds in HoneyBlock.isSlidingDown /
// doSlideMovement (-0.08, -0.13, the -0.05/getOldDeltaY horizontal factor and
// getNewDeltaY(-0.05) vertical clamp) are all expressed *in terms of* these two
// functions; this header ports the two primitives faithfully so any caller composing
// them reproduces the slide behaviour exactly.

namespace mc::block {

// HoneyBlock.java:76-78. 0.98F is a float widened to double inside a double expression.
inline double honeyGetOldDeltaY(double deltaY) {
    return deltaY / 0.98F + 0.08;
}

// HoneyBlock.java:80-82.
inline double honeyGetNewDeltaY(double deltaY) {
    return (deltaY - 0.08) * 0.98F;
}

}  // namespace mc::block
