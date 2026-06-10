import net.minecraft.stats.StatFormatter;

// Ground truth for mcpp/src/stats/StatFormatter.h.  Emits tab-separated rows from
// the REAL net.minecraft.stats.StatFormatter constants (26.1.2).
//
// StatFormatter is a public interface; DEFAULT / DIVIDE_BY_TEN / DISTANCE / TIME are
// public static final fields and format(int) is the public abstract method, so no
// reflection is required.
//
// Output strings are emitted RAW (they never contain a tab or newline: only digits,
// '.', ',', '-', 'E', and the unit suffixes " cm"/" m"/" km"/" s"/" min"/" h"/" d"/" y").
//
// Row formats (TAG \t value \t outputString):
//   DEFAULT   intValue   string
//   DIV10     intValue   string
//   DISTANCE  intValue   string
//   TIME      intValue   string
public class StatFormatterParity {
    static final java.io.PrintStream O = System.out;

    static void row(String tag, int v, StatFormatter f) {
        O.println(tag + "\t" + v + "\t" + f.format(v));
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ---- DEFAULT: NumberFormat.getIntegerInstance(Locale.US).format -----------
        // Grouping boundaries, sign, and Integer.MIN/MAX.
        int[] defVals = {
            0, 1, 9, 10, 99, 100, 999, 1000, 1001, 9999, 10000, 12345,
            100000, 999999, 1000000, 1234567, 12345678, 123456789, 1000000000,
            2147483647,
            -1, -9, -10, -99, -100, -999, -1000, -12345, -1234567, -2147483648,
            -2147483647, 7, 42, 305419896, -305419896, 500, 5000, 50000
        };
        for (int v : defVals) row("DEFAULT", v, StatFormatter.DEFAULT);

        // ---- DIVIDE_BY_TEN: DECIMAL_FORMAT.format(value * 0.1) ---------------------
        // value*0.1 exercises the 2-fraction-digit HALF_EVEN formatting.
        int[] divVals = {
            0, 1, 2, 3, 4, 5, 9, 10, 11, 15, 19, 20, 25, 49, 50, 51, 99, 100,
            101, 105, 123, 125, 135, 199, 200, 250, 999, 1000, 1234, 12345,
            100000, 1000000, 9999999, 12345678, 99999999,
            -1, -2, -5, -9, -10, -15, -25, -50, -99, -100, -125, -1234, -12345,
            -100000, -9999999,
            7, 13, 17, 23, 33, 45, 55, 65, 75, 85, 95, 305, 555, 1005, 10005
        };
        for (int v : divVals) row("DIV10", v, StatFormatter.DIVIDE_BY_TEN);

        // ---- DISTANCE: cm -> cm/m/km cascade --------------------------------------
        // Cover the cm (<=50), m (51..50000), and km (>50000) branches, plus the
        // exact > 0.5 boundaries (meters>0.5 at cm>50; km>0.5 at cm>50000).
        int[] distVals = {
            0, 1, 25, 49, 50, 51, 52, 99, 100, 101, 250, 500, 999, 1000,
            5000, 12345, 49999, 50000, 50001, 50050, 50500, 99999, 100000,
            100001, 123456, 1000000, 1234567, 12345678, 123456789, 1000000000,
            2147483647,
            45, 46, 47, 48, 53, 54, 55, 75, 150, 200, 333, 777,
            150000, 250000, 555555, 9999999
        };
        for (int v : distVals) row("DISTANCE", v, StatFormatter.DISTANCE);

        // ---- TIME: value -> s/min/h/d/y cascade -----------------------------------
        // seconds=value/20.  Branch boundaries (all use ">0.5"):
        //   minutes>0.5  : seconds>30      -> value>600
        //   hours>0.5    : minutes>30      -> value>36000
        //   days>0.5     : hours>12        -> value>864000
        //   years>0.5    : days>182.5      -> value>315360000
        // The seconds branch (value<=600) is the only one that hits Double.toString.
        int[] timeVals = {
            0, 1, 2, 3, 5, 7, 10, 13, 19, 20, 21, 25, 39, 40, 50, 100, 199,
            200, 300, 400, 500, 599, 600,                 // seconds branch
            601, 602, 700, 1000, 1200, 2000, 6000, 12000, 20000, 36000,
            36001, 40000, 72000, 144000, 500000, 864000,  // minutes/hours branch
            864001, 1000000, 2000000, 10000000, 100000000, 315360000,
            315360001, 400000000, 1000000000, 2000000000, 2147483647,  // days/years
            11, 33, 77, 123, 222, 444, 555, 666
        };
        for (int v : timeVals) row("TIME", v, StatFormatter.TIME);
    }
}
