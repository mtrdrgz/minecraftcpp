// Ground-truth generator for net.minecraft.util.SequencedPriorityIterator<T>.
//
// A Guava AbstractIterator that yields elements highest-priority-first and FIFO within
// a priority. We drive the REAL class with a deterministic stream of operations and dump,
// for every operation, exactly what the real iterator did, so the C++ replay can verify
// the SAME element ordering and the SAME exhaustion behavior op-for-op.
//
// We instantiate it as SequencedPriorityIterator<Integer>; payloads are ints so the TSV
// is trivially comparable. The op stream is emitted per sequence id; the C++ test keeps
// one iterator per seq and replays each op in order.
//
// Op rows (tab-separated):
//   ADD  <seq> <value> <priority>
//        echoes the add; no observable return.
//   NEXT <seq> <hasNext:0|1> <value-or-(-1)>
//        calls iterator.hasNext(); if true, value = iterator.next(); else value = -1.
//        (AbstractIterator caches one element across hasNext/next, and latches DONE once
//         computeNext() returns endOfData() — a later ADD must NOT revive it. The op
//         stream below exercises that latch explicitly.)
//
// Pure: SequencedPriorityIterator depends only on Guava AbstractIterator/ArrayDeque and
// fastutil Int2ObjectOpenHashMap — no Bootstrap, registries, or GL. We still run the
// SharedConstants/Bootstrap preamble for uniformity with the other gates; it is a no-op
// for this class.
//
//   tools/run_groundtruth.ps1 -Tool SequencedPriorityIteratorParity -Out mcpp/build/sequenced_priority_iterator.tsv

import java.util.Iterator;
import net.minecraft.util.SequencedPriorityIterator;

public class SequencedPriorityIteratorParity {
    static final java.io.PrintStream O = System.out;

    static void add(SequencedPriorityIterator<Integer> it, int seq, int value, int priority) {
        it.add(value, priority);
        O.println("ADD\t" + seq + "\t" + value + "\t" + priority);
    }

    static void next(Iterator<Integer> it, int seq) {
        if (it.hasNext()) {
            int v = it.next();
            O.println("NEXT\t" + seq + "\t1\t" + v);
        } else {
            O.println("NEXT\t" + seq + "\t0\t-1");
        }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        int seq = 0;

        // Seq 1: straight descending-priority drain. Add three priorities (FIFO within
        // each), then drain fully — expect 30,31, 20,21,22, 10, then exhausted.
        {
            var it = new SequencedPriorityIterator<Integer>();
            seq++;
            add(it, seq, 10, 1);
            add(it, seq, 20, 2);
            add(it, seq, 21, 2);
            add(it, seq, 22, 2);
            add(it, seq, 30, 3);
            add(it, seq, 31, 3);
            for (int k = 0; k < 8; k++) next(it, seq);  // 2 past the end to prove exhaustion
        }

        // Seq 2: interleave adds and nexts so the highestPrio cache moves around. A higher
        // priority added mid-drain must pre-empt the current one.
        {
            var it = new SequencedPriorityIterator<Integer>();
            seq++;
            add(it, seq, 100, 5);
            next(it, seq);            // -> 100, cache empties -> switch (nothing)
            add(it, seq, 200, 5);     // re-adds at same prio AFTER drain: tests DONE latch
            next(it, seq);            // AbstractIterator already DONE? -> exhausted
            next(it, seq);
        }

        // Seq 3: add lower then higher priority before any next; higher must come first.
        {
            var it = new SequencedPriorityIterator<Integer>();
            seq++;
            add(it, seq, 1, 0);
            add(it, seq, 2, 0);
            add(it, seq, 9, 9);       // priority 9 >> 0
            add(it, seq, 5, 5);       // priority 5 between
            next(it, seq);            // 9
            next(it, seq);            // 5
            next(it, seq);            // 1
            next(it, seq);            // 2
            next(it, seq);            // exhausted
        }

        // Seq 4: equal-to-current-highest adds while draining (the priority==highestPrio
        // && highestPrioQueue!=null fast path), plus a mid-drain higher add.
        {
            var it = new SequencedPriorityIterator<Integer>();
            seq++;
            add(it, seq, 1, 7);
            next(it, seq);            // 1 ; cache empties, highestPrio resets toward MIN
            add(it, seq, 2, 7);       // DONE latch test again at same prio
            add(it, seq, 3, 8);
            next(it, seq);
            next(it, seq);
        }

        // Seq 5: negative priorities including Integer.MIN_VALUE — exercises the signed
        // comparisons and the highestPrio seed (INT32_MIN). The FIRST add at MIN_VALUE
        // must still install the cache (priority >= highestPrio is true: MIN >= MIN).
        {
            var it = new SequencedPriorityIterator<Integer>();
            seq++;
            add(it, seq, 50, Integer.MIN_VALUE);
            add(it, seq, 51, Integer.MIN_VALUE);
            add(it, seq, 60, -1);
            add(it, seq, 70, Integer.MAX_VALUE);
            next(it, seq);            // 70 (MAX)
            next(it, seq);            // 60 (-1)
            next(it, seq);            // 50 (MIN)
            next(it, seq);            // 51 (MIN)
            next(it, seq);            // exhausted
        }

        // Seq 6: the early-break trap. After draining the top priority P, the scan looks
        // for P-1; provide an unbroken run P, P-1, P-2, ... so the "prio == highestPrio-1"
        // break fires every switch. Result order must be a clean descending drain.
        {
            var it = new SequencedPriorityIterator<Integer>();
            seq++;
            for (int p = 10; p >= 0; p--) add(it, seq, 1000 + p, p);
            for (int k = 0; k < 13; k++) next(it, seq);
        }

        // Seq 7: gaps in priority (no P-1 queue) so the early-break never fires and the
        // full scan must still find the true max each time.
        {
            var it = new SequencedPriorityIterator<Integer>();
            seq++;
            add(it, seq, 1, 1);
            add(it, seq, 100, 100);
            add(it, seq, 50, 50);
            add(it, seq, 1000, 1000);
            for (int k = 0; k < 6; k++) next(it, seq);
        }

        // Seq 8: never call next at all on a fresh iterator (no observable state), then a
        // single next on an empty iterator — must be exhausted from the start.
        {
            var it = new SequencedPriorityIterator<Integer>();
            seq++;
            next(it, seq);            // exhausted immediately (highestPrioQueue == null)
            next(it, seq);
            add(it, seq, 5, 5);       // DONE latch: add after first endOfData
            next(it, seq);
        }

        // Seq 9: big randomized-but-deterministic stress. Fixed LCG so it is reproducible
        // without RandomSource. Interleave many adds/nexts across a wide priority band.
        {
            var it = new SequencedPriorityIterator<Integer>();
            seq++;
            long s = 0x9E3779B97F4A7C15L;
            int val = 0;
            for (int step = 0; step < 400; step++) {
                s = s * 6364136223846793005L + 1442695040888963407L;
                int r = (int)(s >>> 40) & 0x3;
                if (r != 0) {
                    s = s * 6364136223846793005L + 1442695040888963407L;
                    int prio = (int)(s >>> 33) % 17 - 4;  // band roughly [-4, 12]
                    add(it, seq, val++, prio);
                } else {
                    next(it, seq);
                }
            }
            // Drain whatever remains.
            for (int k = 0; k < 600; k++) next(it, seq);
        }

        O.flush();
    }
}
