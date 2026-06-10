// Ground-truth generator for net.minecraft.network.protocol.game.VecDeltaCodec parity.
//
// Drives the REAL VecDeltaCodec, the entity-position delta packer used by
// ClientboundMoveEntityPacket. Each coordinate is quantized via the package-private
// static encode(double) = Math.round(input * 4096.0) and reconstructed via
// decode(long) = v / 4096.0. The codec is STATEFUL — it holds a base Vec3.
//
// We exercise BOTH the raw statics and the full stateful instance API:
//   - encode(double) / decode(long): package-private statics via reflection
//   - setBase(Vec3); then for a sequence of target positions:
//       encodeX/Y/Z(Vec3) -> 3 packed delta longs,
//       decode(xa,ya,za)  -> reconstructed Vec3,
//       delta(Vec3)       -> raw (pos - base) Vec3,
//       getBase()         -> current base Vec3.
//
// Output rows (tab-separated). Doubles = %016x of Double.doubleToRawLongBits;
// longs = decimal.
//   ENC <input_d_hex> <encoded_long>
//   DEC <v_long> <decoded_d_hex>
//   SEQ <baseX_hex> <baseY_hex> <baseZ_hex> <posX_hex> <posY_hex> <posZ_hex>
//       <encX_long> <encY_long> <encZ_long>
//       <decX_hex> <decY_hex> <decZ_hex>
//       <deltaX_hex> <deltaY_hex> <deltaZ_hex>
//       <getBaseX_hex> <getBaseY_hex> <getBaseZ_hex>

import java.lang.reflect.Method;

import net.minecraft.network.protocol.game.VecDeltaCodec;
import net.minecraft.world.phys.Vec3;

public class VecDeltaCodecParity {
   static final java.io.PrintStream O = System.out;

   static String h(double v) {
      return String.format("%016x", Double.doubleToRawLongBits(v));
   }

   static Method encodeM; // static long encode(double)
   static Method decodeM; // static double decode(long)

   static void emitEnc(double input) throws Exception {
      long out = (Long) encodeM.invoke(null, input);
      O.println("ENC\t" + h(input) + "\t" + out);
   }

   static void emitDec(long v) throws Exception {
      double out = (Double) decodeM.invoke(null, v);
      O.println("DEC\t" + v + "\t" + h(out));
   }

   // Run a full stateful sequence: set base, then probe a target position.
   static void emitSeq(double bx, double by, double bz, double px, double py, double pz) {
      VecDeltaCodec codec = new VecDeltaCodec();
      Vec3 base = new Vec3(bx, by, bz);
      codec.setBase(base);

      Vec3 pos = new Vec3(px, py, pz);
      long ex = codec.encodeX(pos);
      long ey = codec.encodeY(pos);
      long ez = codec.encodeZ(pos);

      Vec3 dec = codec.decode(ex, ey, ez);
      Vec3 delta = codec.delta(pos);
      Vec3 gb = codec.getBase();

      StringBuilder sb = new StringBuilder("SEQ");
      sb.append('\t').append(h(bx)).append('\t').append(h(by)).append('\t').append(h(bz));
      sb.append('\t').append(h(px)).append('\t').append(h(py)).append('\t').append(h(pz));
      sb.append('\t').append(ex).append('\t').append(ey).append('\t').append(ez);
      sb.append('\t').append(h(dec.x)).append('\t').append(h(dec.y)).append('\t').append(h(dec.z));
      sb.append('\t').append(h(delta.x)).append('\t').append(h(delta.y)).append('\t').append(h(delta.z));
      sb.append('\t').append(h(gb.x)).append('\t').append(h(gb.y)).append('\t').append(h(gb.z));
      O.println(sb.toString());
   }

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      encodeM = VecDeltaCodec.class.getDeclaredMethod("encode", double.class);
      encodeM.setAccessible(true);
      decodeM = VecDeltaCodec.class.getDeclaredMethod("decode", long.class);
      decodeM.setAccessible(true);

      // ---- encode(double): the Math.round(x*4096) quantizer ----
      // exact integers, halves (round-half-up semantics of Math.round), tiny
      // fractions, the double-rounding trap value, negatives, large physical coords.
      double[] encIn = {
         0.0, 1.0, -1.0, 0.5, -0.5, 0.25, -0.25,
         0.000244140625,    // exactly 1/4096 -> 1
         0.0001220703125,   // exactly 0.5/4096 -> Math.round of 0.5 = 1
         0.00006103515625,  // 0.25/4096 -> 0
         63.0, 64.0, 64.5, -64.5, 100.5, 127.99,
         0.49999999999999994,            // the classic Math.round double-rounding trap
         123.45678901234, -987.6543210987,
         1.0E7, -1.0E7,                  // near worldborder magnitude
         29999984.0, -29999984.0,        // ~ world border
         0.1, 0.2, 0.3, -0.1,            // non-representable decimals
         255.5, 256.5, 0.7071067811865476,
         3.999755859375, 3.9998779296875 // just-below halfway around 4096 grid
      };
      for (double d : encIn) emitEnc(d);

      // ---- decode(long): v / 4096.0 ----
      long[] decIn = {
         0L, 1L, -1L, 4096L, -4096L, 2048L, -2048L, 1L << 20, -(1L << 20),
         63L * 4096L, 64L * 4096L + 2048L, 100L, -100L, 123456789L, -123456789L,
         29999984L * 4096L, -29999984L * 4096L, 7L, 9999L
      };
      for (long v : decIn) emitDec(v);

      // ---- Full stateful sequences: base + target ----
      // identical (zero delta -> base passthrough), small steps, large jumps,
      // mixed-axis-zero deltas, negative coords, physical entity-movement deltas.
      emitSeq(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);             // all-zero delta -> base
      emitSeq(10.0, 64.0, -20.0, 10.0, 64.0, -20.0);     // pos == base -> zero delta
      emitSeq(0.0, 0.0, 0.0, 1.0, 2.0, 3.0);
      emitSeq(100.5, 64.0, -200.25, 100.5625, 64.0, -200.25);  // y unchanged -> ya==0 branch
      emitSeq(100.5, 64.0, -200.25, 100.5, 64.5, -200.25);     // x,z unchanged
      emitSeq(0.0, 0.0, 0.0, 0.000244140625, -0.000244140625, 0.0); // sub-grid step
      emitSeq(-1234.5, 70.0, 5678.25, -1234.4998, 70.0001, 5678.2503);
      emitSeq(0.1, 0.2, 0.3, 0.10000000149, 0.19999998, 0.30000001); // float-ish drift
      emitSeq(29999984.0, 319.0, -29999984.0, 29999983.9, 318.9, -29999983.9);
      emitSeq(7.0, 7.0, 7.0, 7.5, 6.5, 8.25);
      emitSeq(0.0, -64.0, 0.0, 16.123, -63.877, -32.456);
      emitSeq(-0.5, 0.5, -0.5, 0.5, -0.5, 0.5);
      emitSeq(1000000.0, 200.0, -1000000.0, 1000000.0625, 199.9375, -999999.9375);
      emitSeq(3.0, 4.0, 5.0, 3.0, 4.0, 5.0);             // exact-int pos == base
      emitSeq(12.34, 56.78, 90.12, 12.3401, 56.7799, 90.1201); // tiny per-axis nudges
   }
}
