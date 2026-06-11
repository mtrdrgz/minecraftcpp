// Parity test for the PURE state-machine subset of
// net.minecraft.world.effect.MobEffectInstance (26.1.2).
// Ground truth: tools/MobEffectInstanceParity.java.
//
//   mob_effect_instance_parity --cases <mob_effect_instance.tsv>
//
// Each row is self-describing: it carries the full INPUT spec and the expected
// OUTPUT. We rebuild the input instance(s) bit-for-bit, run the C++ port, and
// compare the flattened output state (every node of the hidden chain) plus any
// boolean/changed flags against the ground truth.
//
// Encodings: floats via 8-hex raw bits -> std::bit_cast<float>; bools 0/1;
// ints decimal. effId 1 => SPEED, 2 => SLOWNESS (opaque identity keys). The
// hashCode() seed is the real Holder.hashCode() printed by Java; we feed the
// identical seed so the 31*… mixing is byte-identical.
//
// Row tags:
//   CTOR   <effId> <dur> <amp> <amb> <vis> <ic> | <seedHash> <flatOut>
//   ENDS   <effId> <dur> <ticks> <bool>
//   INF    <effId> <dur> <bool>
//   HASREM <effId> <dur> <bool>
//   SCALE  <effId> <dur> <amp> <amb> <vis> <ic> <scale8> | <seedHash> <flatOut>
//   TICK   <effspec> | <changed> <seedHash> <flatOut>
//   UPDATE <cur effspec> <<< <to effspec> | <changed> <seedHash> <flatOut>

#include "world/effect/MobEffectInstance.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::MobEffectInstance;

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

float bf(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}
int32_t i32(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// effId -> the real Holder.hashCode() seed used by Java for that effect. The GT
// prints the seed on every output row, so we never need to know these values up
// front; this helper is only used where we build instances purely for output
// flattening and the seed is taken from the row's <seedHash> token. Here we keep
// the effId as the instance's effect() identity directly.

// Parse one effspec from tokens starting at index i; advance i. Returns the
// constructed instance. Spec layout:
//   <effId> <dur> <amp> <amb> <vis> <ic> <hiddenDepth> [<inner effspec>]
std::unique_ptr<MobEffectInstance> parseSpec(const std::vector<std::string>& p, size_t& i) {
    int64_t effId = std::stoll(p[i++]);
    int32_t dur = i32(p[i++]);
    int32_t amp = i32(p[i++]);
    bool amb = i32(p[i++]) != 0;
    bool vis = i32(p[i++]) != 0;
    bool ic = i32(p[i++]) != 0;
    int hiddenDepth = i32(p[i++]);
    std::unique_ptr<MobEffectInstance> hidden;
    if (hiddenDepth == 1) {
        hidden = parseSpec(p, i);
    }
    return std::make_unique<MobEffectInstance>(effId, dur, amp, amb, vis, ic, std::move(hidden));
}

// Flatten C++ output identically to the Java flatOut(): seedHash then
// per-node (effId,dur,amp,amb,vis,ic,hashCode) tokens, then "END". seedHash is
// supplied (the row's <seedHash>); hashCode uses that seed for the root effect.
// NOTE: all nodes in our scenarios share the root's effect identity, so the
// per-node hashCode seed equals seedHash for every node (Java seeds with that
// node's own effect.hashCode(), and SPEED==SPEED throughout the chain). For
// SLOWNESS-rooted CTOR rows the chain is a single node, so seedHash matches.
std::string flatOut(const MobEffectInstance* inst, int32_t seedHash,
                    int32_t slowSeed, int64_t slowId) {
    std::ostringstream sb;
    sb << seedHash;
    const MobEffectInstance* node = inst;
    int guard = 0;
    while (node != nullptr && guard++ < 64) {
        // Each node's hashCode seed is its OWN effect's Holder.hashCode().
        int32_t nodeSeed = (node->effect() == slowId) ? slowSeed : seedHash;
        // If root is SLOWNESS, seedHash already is the SLOWNESS seed; SPEED nodes
        // in a SLOWNESS-rooted chain do not occur in our scenarios.
        sb << '\t' << node->effect()
           << '\t' << node->getDuration()
           << '\t' << node->getAmplifier()
           << '\t' << (node->isAmbient() ? 1 : 0)
           << '\t' << (node->isVisible() ? 1 : 0)
           << '\t' << (node->showIcon() ? 1 : 0)
           << '\t' << node->hashCode(nodeSeed);
        node = node->hidden();
    }
    sb << '\t' << "END";
    return sb.str();
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: mob_effect_instance_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l, const std::string& got,
                    const std::string& want) {
        ++mism;
        if (shown++ < 40)
            std::cerr << "MISMATCH " << l << "\n   got:  " << got << "\n   want: " << want << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        // strip trailing \r if present
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++total;

        // locate the "|" separator (present in CTOR/SCALE/TICK/UPDATE rows).
        auto barIdx = [&]() -> size_t {
            for (size_t k = 0; k < p.size(); ++k)
                if (p[k] == "|") return k;
            return p.size();
        };

        if (tag == "CTOR") {
            // CTOR <effId> <dur> <amp> <amb> <vis> <ic> | <seedHash> <flatOut...>
            size_t bar = barIdx();
            int64_t eid = std::stoll(p[1]);
            int32_t dur = i32(p[2]);
            int32_t amp = i32(p[3]);
            bool amb = i32(p[4]) != 0, vis = i32(p[5]) != 0, ic = i32(p[6]) != 0;
            int32_t seed = i32(p[bar + 1]);
            // Root effect determines both the seed and the slow identity mapping.
            int32_t slowSeed = (eid == 2) ? seed : 0;
            int64_t slowId = (eid == 2) ? 2 : -999; // no SLOWNESS-in-chain otherwise
            MobEffectInstance m(eid, dur, amp, amb, vis, ic);
            std::string got = flatOut(&m, seed, slowSeed, slowId);
            std::string want;
            for (size_t k = bar + 1; k < p.size(); ++k) {
                if (k > bar + 1) want += '\t';
                want += p[k];
            }
            if (got != want) fail(line, got, want);
        } else if (tag == "INF") {
            int32_t dur = i32(p[2]);
            bool want = i32(p[3]) != 0;
            MobEffectInstance m(std::stoll(p[1]), dur, 0, false, true, true);
            if (m.isInfiniteDuration() != want)
                fail(line, std::to_string(m.isInfiniteDuration()), std::to_string(want));
        } else if (tag == "HASREM") {
            int32_t dur = i32(p[2]);
            bool want = i32(p[3]) != 0;
            MobEffectInstance m(std::stoll(p[1]), dur, 0, false, true, true);
            if (m.hasRemainingDuration() != want)
                fail(line, std::to_string(m.hasRemainingDuration()), std::to_string(want));
        } else if (tag == "ENDS") {
            int32_t dur = i32(p[2]);
            int32_t ticks = i32(p[3]);
            bool want = i32(p[4]) != 0;
            MobEffectInstance m(std::stoll(p[1]), dur, 0, false, true, true);
            if (m.endsWithin(ticks) != want)
                fail(line, std::to_string(m.endsWithin(ticks)), std::to_string(want));
        } else if (tag == "SCALE") {
            // SCALE <effId> <dur> <amp> <amb> <vis> <ic> <scale8> | <seedHash> <flatOut>
            size_t bar = barIdx();
            int64_t eid = std::stoll(p[1]);
            int32_t dur = i32(p[2]);
            int32_t amp = i32(p[3]);
            bool amb = i32(p[4]) != 0, vis = i32(p[5]) != 0, ic = i32(p[6]) != 0;
            float scale = bf(p[7]);
            int32_t seed = i32(p[bar + 1]);
            MobEffectInstance base(eid, dur, amp, amb, vis, ic);
            MobEffectInstance scaled = base.withScaledDuration(scale);
            std::string got = flatOut(&scaled, seed, 0, -999);
            std::string want;
            for (size_t k = bar + 1; k < p.size(); ++k) {
                if (k > bar + 1) want += '\t';
                want += p[k];
            }
            if (got != want) fail(line, got, want);
        } else if (tag == "TICK") {
            // TICK <effspec...> | <changed> <seedHash> <flatOut>
            size_t bar = barIdx();
            size_t i = 1;
            auto inst = parseSpec(p, i);
            // tickDownDuration() then downgradeToHiddenEffect() once.
            inst->tickDownDuration();
            bool dg = inst->downgradeToHiddenEffect();
            int wantChanged = i32(p[bar + 1]);
            int32_t seed = i32(p[bar + 2]);
            std::string got = (dg ? "1" : "0");
            got += '\t';
            got += flatOut(inst.get(), seed, 0, -999);
            std::string want;
            for (size_t k = bar + 1; k < p.size(); ++k) {
                if (k > bar + 1) want += '\t';
                want += p[k];
            }
            if (got != want) fail(line, got, want);
        } else if (tag == "UPDATE") {
            // UPDATE <cur effspec> <<< <to effspec> | <changed> <seedHash> <flatOut>
            size_t bar = barIdx();
            size_t sep = p.size();
            for (size_t k = 0; k < p.size(); ++k)
                if (p[k] == "<<<") { sep = k; break; }
            size_t i = 1;
            auto cur = parseSpec(p, i); // parses up to sep
            size_t j = sep + 1;
            auto to = parseSpec(p, j); // parses up to bar
            bool changed = cur->update(*to);
            int32_t seed = i32(p[bar + 2]);
            std::string got = (changed ? "1" : "0");
            got += '\t';
            got += flatOut(cur.get(), seed, 0, -999);
            std::string want;
            for (size_t k = bar + 1; k < p.size(); ++k) {
                if (k > bar + 1) want += '\t';
                want += p[k];
            }
            if (got != want) fail(line, got, want);
        } else {
            fail("UNKNOWN_TAG " + tag, "", "");
        }
    }

    std::cout << "MobEffectInstance checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
