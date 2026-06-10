// 1:1 C++ port of net.minecraft.stats.StatFormatter (Minecraft 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/stats/StatFormatter.java):
//   DecimalFormat DECIMAL_FORMAT = new DecimalFormat("########0.00",
//       DecimalFormatSymbols.getInstance(Locale.ROOT));
//   StatFormatter DEFAULT       = NumberFormat.getIntegerInstance(Locale.US)::format;
//   StatFormatter DIVIDE_BY_TEN = value -> DECIMAL_FORMAT.format(value * 0.1);
//   StatFormatter DISTANCE      = cm -> { meters=cm/100.0; km=meters/1000.0;
//        km>0.5 ? fmt(km)+" km" : meters>0.5 ? fmt(meters)+" m" : cm+" cm"; };
//   StatFormatter TIME          = value -> { sec=value/20.0; min=sec/60; hr=min/60;
//        day=hr/24; yr=day/365; yr>0.5?fmt(yr)+" y":day>0.5?fmt(day)+" d":
//        hr>0.5?fmt(hr)+" h":min>0.5?fmt(min)+" min":sec+" s"; };
//   String format(int value);
//
// The three string primitives the formatters depend on are pure JDK library
// functions of the IEEE-754 double value (no Minecraft state):
//   * usIntegerFormat  -> java.text.NumberFormat.getIntegerInstance(Locale.US).format(long)
//                         (decimal, ',' grouping every 3 digits, '-' sign)
//   * decimalFormat2   -> java.text.DecimalFormat("########0.00", ROOT).format(double)
//                         (>=1 integer digit, exactly 2 fraction digits, no grouping,
//                          RoundingMode.HALF_EVEN — DecimalFormat's default)
//   * javaDoubleToString -> java.lang.Double.toString(double) (shortest round-trip,
//                         per the Double.toString spec)
// These are reproduced here with EXACT integer/decimal arithmetic derived from the
// double's mantissa/exponent — not floating-point approximations — so the output
// strings match the JDK byte-for-byte. Verified by StatFormatterParityTest against
// the real net.minecraft.stats.StatFormatter constants.

#ifndef MCPP_STATS_STATFORMATTER_H
#define MCPP_STATS_STATFORMATTER_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace mc {
namespace stats {

// ---------------------------------------------------------------------------
// Minimal arbitrary-precision unsigned big integer (base 1e9 limbs), enough for
// the exact decimal expansion of any finite double.  Only the operations the
// formatters need are implemented: multiply by small, add small, compare,
// multiply by another bigint, and conversion to a decimal digit string.
// ---------------------------------------------------------------------------
namespace detail {

struct BigUInt {
    // little-endian base-1e9 limbs; empty == 0.
    std::vector<uint32_t> d;
    static constexpr uint32_t BASE = 1000000000u;

    BigUInt() = default;
    explicit BigUInt(uint64_t v) {
        while (v) {
            d.push_back(static_cast<uint32_t>(v % BASE));
            v /= BASE;
        }
    }

    bool isZero() const { return d.empty(); }

    void trim() {
        while (!d.empty() && d.back() == 0) d.pop_back();
    }

    void mulSmall(uint32_t m) {
        if (m == 0) { d.clear(); return; }
        uint64_t carry = 0;
        for (size_t i = 0; i < d.size(); ++i) {
            uint64_t cur = static_cast<uint64_t>(d[i]) * m + carry;
            d[i] = static_cast<uint32_t>(cur % BASE);
            carry = cur / BASE;
        }
        while (carry) {
            d.push_back(static_cast<uint32_t>(carry % BASE));
            carry /= BASE;
        }
    }

    void addSmall(uint32_t a) {
        uint64_t carry = a;
        for (size_t i = 0; i < d.size() && carry; ++i) {
            uint64_t cur = static_cast<uint64_t>(d[i]) + carry;
            d[i] = static_cast<uint32_t>(cur % BASE);
            carry = cur / BASE;
        }
        while (carry) {
            d.push_back(static_cast<uint32_t>(carry % BASE));
            carry /= BASE;
        }
    }

    // multiply by 2^n
    void mulPow2(int n) {
        for (; n >= 30; n -= 30) mulSmall(1u << 30);
        if (n > 0) mulSmall(1u << n);
    }

    // -1 if *this<o, 0 if ==, +1 if >
    int cmp(const BigUInt& o) const {
        if (d.size() != o.d.size())
            return d.size() < o.d.size() ? -1 : 1;
        for (size_t i = d.size(); i-- > 0;) {
            if (d[i] != o.d[i]) return d[i] < o.d[i] ? -1 : 1;
        }
        return 0;
    }

    // Decimal digit string (no leading zeros; "0" if zero).
    std::string toDecimal() const {
        if (d.empty()) return "0";
        std::string s;
        // most-significant limb without padding
        s += std::to_string(d.back());
        char buf[10];
        for (size_t i = d.size() - 1; i-- > 0;) {
            std::snprintf(buf, sizeof(buf), "%09u", d[i]);
            s += buf;
        }
        return s;
    }
};

// Decompose a finite double into sign, and a *non-negative* exact value
// mantissa * 2^exp2  (mantissa is a 53-bit-ish integer, exp2 may be negative).
struct DoubleParts {
    bool negative;
    uint64_t mantissa;  // integer significand
    int exp2;           // power of two: value = mantissa * 2^exp2
    bool isZero;
};

inline DoubleParts decompose(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    DoubleParts p;
    p.negative = (bits >> 63) != 0;
    int rawExp = static_cast<int>((bits >> 52) & 0x7FFu);
    uint64_t frac = bits & 0x000FFFFFFFFFFFFFull;
    if (rawExp == 0) {
        // subnormal (or zero)
        p.mantissa = frac;
        p.exp2 = -1074;
    } else {
        p.mantissa = frac | 0x0010000000000000ull;  // implicit leading 1
        p.exp2 = rawExp - 1075;
    }
    p.isZero = (p.mantissa == 0);
    return p;
}

// Build numerator/denominator (both positive BigUInt) for |value| = num/den.
inline void asFraction(const DoubleParts& p, BigUInt& num, BigUInt& den) {
    num = BigUInt(p.mantissa);
    den = BigUInt(1);
    if (p.exp2 >= 0) {
        num.mulPow2(p.exp2);
    } else {
        den.mulPow2(-p.exp2);
    }
}

// java.text.DecimalFormat("########0.00", ROOT).format(value): exactly 2 fraction
// digits, >=1 integer digit, RoundingMode.HALF_EVEN, '.' decimal sep, '-' sign.
inline std::string decimalFormat2(double value) {
    DoubleParts p = decompose(value);
    if (p.isZero) {
        // DecimalFormat collapses -0.00 of an exact +0/-0... +0.0 -> "0.00".
        // (HALF_EVEN of exactly 0 -> 0; pattern never emits a sign for zero magnitude.)
        return "0.00";
    }
    BigUInt num, den;
    asFraction(p, num, den);
    // scaled = |value| * 100 = num*100 / den ; round to nearest integer, ties->even.
    num.mulSmall(100);
    // quotient q = floor(num/den), remainder r.
    // Long division of BigUInt by BigUInt via repeated subtraction is too slow in
    // general, but here den is a power of two (when exp2<0) or 1 (when exp2>=0),
    // and num is then exact.  Implement generic division by binary long division
    // on the decimal-limb representation is awkward; instead exploit structure:
    //   - if exp2 >= 0: den == 1  -> q = num, r = 0.
    //   - else: den == 2^k       -> divide by repeatedly... we instead use the
    //     fact that num/den with den=2^k: q = num >> k (in exact integer terms).
    // To stay generic & exact we perform division through a schoolbook routine.
    // q = num / den, r = num % den.
    BigUInt q, r;
    {
        // Schoolbook long division base 1e9: process limbs MSB->LSB.
        q.d.assign(num.d.size(), 0);
        BigUInt cur;  // running remainder
        for (size_t i = num.d.size(); i-- > 0;) {
            // cur = cur*BASE + num.d[i]
            cur.mulSmall(BigUInt::BASE);
            cur.addSmall(num.d[i]);
            cur.trim();
            // find largest digit x in [0,BASE) with den*x <= cur (binary search)
            uint32_t lo = 0, hi = BigUInt::BASE - 1, x = 0;
            while (lo <= hi) {
                uint32_t mid = lo + (hi - lo) / 2;
                BigUInt t = den;
                t.mulSmall(mid);
                if (t.cmp(cur) <= 0) {
                    x = mid;
                    if (mid == BigUInt::BASE - 1) break;
                    lo = mid + 1;
                } else {
                    if (mid == 0) break;
                    hi = mid - 1;
                }
            }
            q.d[i] = x;
            BigUInt t = den;
            t.mulSmall(x);
            // cur -= t  (cur >= t guaranteed)
            // subtract t from cur
            int64_t borrow = 0;
            for (size_t j = 0; j < cur.d.size(); ++j) {
                int64_t sub = static_cast<int64_t>(cur.d[j]) - borrow -
                              (j < t.d.size() ? static_cast<int64_t>(t.d[j]) : 0);
                if (sub < 0) { sub += BigUInt::BASE; borrow = 1; }
                else borrow = 0;
                cur.d[j] = static_cast<uint32_t>(sub);
            }
            cur.trim();
        }
        q.trim();
        r = cur;
    }
    // HALF_EVEN rounding of q + r/den.  Compare 2*r vs den.
    BigUInt twoR = r;
    twoR.mulSmall(2);
    int c = twoR.cmp(den);
    bool roundUp = false;
    if (c > 0) {
        roundUp = true;
    } else if (c == 0) {
        // tie -> round to even: up iff current quotient is odd.
        uint32_t lowLimb = q.d.empty() ? 0u : q.d[0];
        roundUp = (lowLimb & 1u) != 0;
    }
    if (roundUp) q.addSmall(1);

    std::string digits = q.toDecimal();  // this is |value|*100 rounded, as integer
    // Insert decimal point 2 places from the right; ensure >=1 integer digit.
    std::string out;
    if (digits.size() <= 2) {
        out = "0.";
        out += std::string(2 - digits.size(), '0');
        out += digits;
    } else {
        out = digits.substr(0, digits.size() - 2);
        out += '.';
        out += digits.substr(digits.size() - 2);
    }
    // Sign: only if the rounded magnitude is non-zero (DecimalFormat drops -0.00).
    bool nonZero = !(digits == "0" || digits.find_first_not_of('0') == std::string::npos);
    if (p.negative && nonZero) out = "-" + out;
    return out;
}

// java.text.NumberFormat.getIntegerInstance(Locale.US).format(long): decimal with
// ',' grouping every 3 digits and a '-' sign for negatives.
inline std::string usIntegerFormat(int64_t value) {
    bool neg = value < 0;
    // magnitude as unsigned to handle INT64/Long.MIN safely.
    uint64_t mag = neg ? (~static_cast<uint64_t>(value) + 1ull) : static_cast<uint64_t>(value);
    std::string digits = std::to_string(mag);
    std::string grouped;
    int cnt = 0;
    for (size_t i = digits.size(); i-- > 0;) {
        grouped.push_back(digits[i]);
        if (++cnt % 3 == 0 && i != 0) grouped.push_back(',');
    }
    std::string out(grouped.rbegin(), grouped.rend());
    if (neg) out = "-" + out;
    return out;
}

// ---- java.lang.Double.toString(double) -----------------------------------
// Shortest decimal string that uniquely round-trips to the double, formatted per
// the Double.toString specification:
//   * one digit before the decimal point may be 0; at least one fraction digit.
//   * 1e-3 <= m < 1e7  -> plain decimal notation.
//   * otherwise        -> "computerized scientific notation"  d.dddE±xx.
// We compute the shortest digit sequence with exact big-integer (Dragon4-style)
// arithmetic, then apply the formatting.
//
// Returns the decimal digit string (no point), and via *outExp10 the power of ten
// of the first (most-significant) emitted digit, i.e. value ~= digits * 10^(exp10
// - (len-1)).  Sign handled by caller.
inline std::string shortestDigits(const DoubleParts& p, int& decExp) {
    // value = f * 2^e, f = mantissa, e = exp2.
    // Use the classic free-format Dragon4 with R/S/m+/m- big integers.
    // Reference: Steele & White / "Printing Floating-Point Numbers Quickly and
    // Accurately".  Produces the shortest correctly-rounded (HALF_EVEN at tie)
    // decimal that round-trips, matching Java's Double.toString digit selection.
    uint64_t f = p.mantissa;
    int e = p.exp2;

    // Steele & White boundary rule: when the significand is even the rounding
    // boundaries are *inclusive* (use <= / >=), when odd they are exclusive
    // (< / >).  Java's Double.toString uses round-to-nearest/HALF_EVEN, which
    // corresponds exactly to this even-significand boundary inclusion.
    bool evenMantissa = (f & 1ull) == 0ull;

    BigUInt R, S, mPlus, mMinus;
    bool unequalGaps = (f == 0x0010000000000000ull);  // mantissa is a power-of-two boundary

    if (e >= 0) {
        BigUInt be(1); be.mulPow2(e);
        // R = f * 2^e * 2 ; S = 2 ; mPlus = 2^e ; mMinus = 2^e
        R = BigUInt(f);
        R.mulPow2(e);
        R.mulSmall(2);
        S = BigUInt(2);
        mPlus = be;
        mMinus = be;
    } else {
        // R = f*2 ; S = 2^(-e) * 2 ; mPlus=1 ; mMinus=1
        R = BigUInt(f);
        R.mulSmall(2);
        S = BigUInt(1);
        S.mulPow2(-e);
        S.mulSmall(2);
        mPlus = BigUInt(1);
        mMinus = BigUInt(1);
    }
    if (unequalGaps) {
        // scale up so the smaller (lower) gap stays mMinus, larger gap is doubled.
        R.mulSmall(2);
        S.mulSmall(2);
        mPlus.mulSmall(2);
        // mMinus stays
    }

    // Estimate k = ceil(log10(value)) and scale S so that R/S in [1/10,1).
    // Simple, robust scaling loop (value range here is modest).
    int k = 0;
    // Scale up: while R + mPlus > S (i.e. value's MSD >= 1 in current scale), x10 S.
    {
        // increase k until (R + mPlus) <= S
        for (;;) {
            BigUInt rp = R;
            // rp = R + mPlus
            // add mPlus to rp
            {
                size_t n = std::max(rp.d.size(), mPlus.d.size());
                rp.d.resize(n, 0);
                uint64_t carry = 0;
                for (size_t i = 0; i < n; ++i) {
                    uint64_t cur = static_cast<uint64_t>(rp.d[i]) + carry +
                                   (i < mPlus.d.size() ? mPlus.d[i] : 0);
                    rp.d[i] = static_cast<uint32_t>(cur % BigUInt::BASE);
                    carry = cur / BigUInt::BASE;
                }
                if (carry) rp.d.push_back(static_cast<uint32_t>(carry));
            }
            // condition for "high"-style: if R+mPlus exceeds the upper boundary
            // we need a bigger S (one more leading place).  Boundary inclusive for
            // an even significand, exclusive for odd.
            int c = rp.cmp(S);
            bool tooBig = evenMantissa ? (c >= 0) : (c > 0);
            if (tooBig) {
                S.mulSmall(10);
                ++k;
            } else {
                break;
            }
        }
        // decrease: while (R + mPlus)*10 <= S, multiply R, mPlus, mMinus by 10.
        for (;;) {
            BigUInt rp = R;
            {
                size_t n = std::max(rp.d.size(), mPlus.d.size());
                rp.d.resize(n, 0);
                uint64_t carry = 0;
                for (size_t i = 0; i < n; ++i) {
                    uint64_t cur = static_cast<uint64_t>(rp.d[i]) + carry +
                                   (i < mPlus.d.size() ? mPlus.d[i] : 0);
                    rp.d[i] = static_cast<uint32_t>(cur % BigUInt::BASE);
                    carry = cur / BigUInt::BASE;
                }
                if (carry) rp.d.push_back(static_cast<uint32_t>(carry));
            }
            rp.mulSmall(10);
            int c = rp.cmp(S);
            bool stillFits = evenMantissa ? (c <= 0) : (c < 0);
            if (stillFits) {
                R.mulSmall(10);
                mPlus.mulSmall(10);
                mMinus.mulSmall(10);
                --k;
            } else {
                break;
            }
        }
    }

    // Generate digits.
    std::string digits;
    bool low = false, high = false;
    for (;;) {
        R.mulSmall(10);
        mPlus.mulSmall(10);
        mMinus.mulSmall(10);
        // d = floor(R / S)
        uint32_t dlo = 0, dhi = 9, dd = 0;
        while (dlo <= dhi) {
            uint32_t mid = dlo + (dhi - dlo) / 2;
            BigUInt t = S; t.mulSmall(mid);
            if (t.cmp(R) <= 0) { dd = mid; if (mid == 9) break; dlo = mid + 1; }
            else { if (mid == 0) break; dhi = mid - 1; }
        }
        // R = R - dd*S
        {
            BigUInt t = S; t.mulSmall(dd);
            int64_t borrow = 0;
            R.d.resize(std::max(R.d.size(), t.d.size()), 0);
            for (size_t j = 0; j < R.d.size(); ++j) {
                int64_t sub = static_cast<int64_t>(R.d[j]) - borrow -
                              (j < t.d.size() ? static_cast<int64_t>(t.d[j]) : 0);
                if (sub < 0) { sub += BigUInt::BASE; borrow = 1; } else borrow = 0;
                R.d[j] = static_cast<uint32_t>(sub);
            }
            R.trim();
        }
        // low  : R below the lower rounding boundary (mMinus)
        // high : R + mPlus above the upper rounding boundary (S)
        // even significand -> boundaries inclusive (<= / >=), odd -> exclusive.
        {
            int cl = R.cmp(mMinus);
            low = evenMantissa ? (cl <= 0) : (cl < 0);
        }
        {
            BigUInt rp = R;
            size_t n = std::max(rp.d.size(), mPlus.d.size());
            rp.d.resize(n, 0);
            uint64_t carry = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t cur = static_cast<uint64_t>(rp.d[i]) + carry +
                               (i < mPlus.d.size() ? mPlus.d[i] : 0);
                rp.d[i] = static_cast<uint32_t>(cur % BigUInt::BASE);
                carry = cur / BigUInt::BASE;
            }
            if (carry) rp.d.push_back(static_cast<uint32_t>(carry));
            int ch = rp.cmp(S);
            high = evenMantissa ? (ch >= 0) : (ch > 0);
        }
        if (low || high) {
            // terminate; decide final digit
            if (low && !high) {
                // dd stays
            } else if (high && !low) {
                ++dd;
            } else {
                // both: round to nearest by comparing 2R vs S
                BigUInt twoR = R; twoR.mulSmall(2);
                int c = twoR.cmp(S);
                if (c > 0) ++dd;
                else if (c == 0) {
                    // tie -> round to even
                    if (dd & 1u) ++dd;
                }
                // c<0 -> keep dd
            }
            digits.push_back(static_cast<char>('0' + dd));
            break;
        }
        digits.push_back(static_cast<char>('0' + dd));
    }

    decExp = k;  // exponent of the digit BEFORE the first generated one;
                 // i.e. value = 0.digits * 10^k  => first digit has place 10^(k-1).
    return digits;
}

inline std::string javaDoubleToString(double value) {
    DoubleParts p = decompose(value);
    if (p.isZero) {
        return p.negative ? std::string("-0.0") : std::string("0.0");
    }
    int k = 0;
    std::string digits = shortestDigits(p, k);
    // value = 0.<digits> x 10^k  ->  first digit corresponds to 10^(k-1).
    // Build decimal exponent of MSD: msdExp = k - 1.
    int msdExp = k - 1;
    int len = static_cast<int>(digits.size());

    std::string mantissaSign = p.negative ? "-" : "";
    std::string out;

    // Decimal range test on magnitude m: 1e-3 <= m < 1e7.
    // m's order of magnitude is msdExp (m in [10^msdExp, 10^(msdExp+1))).
    // Java's Double.toString uses: if (e >= -3 && e < 7) decimal else scientific,
    // where e is the decimal exponent such that m = d.dddd * 10^e (e == msdExp).
    if (msdExp >= -3 && msdExp < 7) {
        // plain decimal
        if (msdExp >= 0) {
            int intDigits = msdExp + 1;
            if (len <= intDigits) {
                out = digits + std::string(intDigits - len, '0') + ".0";
            } else {
                out = digits.substr(0, intDigits) + "." + digits.substr(intDigits);
            }
        } else {
            // 0.00...digits
            out = "0." + std::string(-msdExp - 1, '0') + digits;
        }
        out = mantissaSign + out;
    } else {
        // scientific: d.ddddE(exp)
        std::string m;
        m.push_back(digits[0]);
        m.push_back('.');
        if (len > 1) m += digits.substr(1);
        else m.push_back('0');
        out = mantissaSign + m + "E" + std::to_string(msdExp);
    }
    return out;
}

}  // namespace detail

// ---------------------------------------------------------------------------
// The five formatters.  Each takes a Java int and returns the exact Java String.
// ---------------------------------------------------------------------------

inline std::string formatDefault(int32_t value) {
    return detail::usIntegerFormat(static_cast<int64_t>(value));
}

inline std::string formatDivideByTen(int32_t value) {
    return detail::decimalFormat2(static_cast<double>(value) * 0.1);
}

inline std::string formatDistance(int32_t cm) {
    double meters = cm / 100.0;
    double kilometers = meters / 1000.0;
    if (kilometers > 0.5) {
        return detail::decimalFormat2(kilometers) + " km";
    } else if (meters > 0.5) {
        return detail::decimalFormat2(meters) + " m";
    } else {
        // cm + " cm"  ->  cm is an int: plain Integer.toString.
        return std::to_string(cm) + " cm";
    }
}

inline std::string formatTime(int32_t value) {
    double seconds = value / 20.0;
    double minutes = seconds / 60.0;
    double hours = minutes / 60.0;
    double days = hours / 24.0;
    double years = days / 365.0;
    if (years > 0.5) {
        return detail::decimalFormat2(years) + " y";
    } else if (days > 0.5) {
        return detail::decimalFormat2(days) + " d";
    } else if (hours > 0.5) {
        return detail::decimalFormat2(hours) + " h";
    } else if (minutes > 0.5) {
        return detail::decimalFormat2(minutes) + " min";
    } else {
        // seconds + " s"  ->  seconds is a double: Double.toString.
        return detail::javaDoubleToString(seconds) + " s";
    }
}

}  // namespace stats
}  // namespace mc

#endif  // MCPP_STATS_STATFORMATTER_H
