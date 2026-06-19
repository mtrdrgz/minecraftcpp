// Parity test for mc::InteractionResult (world/InteractionResult.h) vs Java ground truth.
// Reads the TSV emitted by InteractionResultParity.java and compares value-for-value.
#include "world/InteractionResult.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc;

static std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Map a constant id to the ported InteractionResult value.
static const InteractionResult* constById(const std::string& id) {
    if (id == "SUCCESS") return &INTERACTION_SUCCESS;
    if (id == "SUCCESS_SERVER") return &INTERACTION_SUCCESS_SERVER;
    if (id == "CONSUME") return &INTERACTION_CONSUME;
    if (id == "FAIL") return &INTERACTION_FAIL;
    if (id == "PASS") return &INTERACTION_PASS;
    if (id == "TRY_WITH_EMPTY_HAND") return &INTERACTION_TRY_WITH_EMPTY_HAND;
    return nullptr;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: InteractionResultParityTest --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "SWING") {
            // SWING <ordinal> <name>
            ++cases;
            int ord = std::stoi(p[1]);
            const std::string& name = p[2];
            auto s = static_cast<InteractionResultSwingSource>(ord);
            if (name != std::string(interactionResultSwingSourceName(s))) {
                ++mism;
                std::cerr << "SWING mismatch ord=" << ord << " expected name=" << name
                          << " got=" << interactionResultSwingSourceName(s) << "\n";
            }
        } else if (tag == "CTX") {
            // CTX <which> <wasItemInteraction> <heldPresent>
            ++cases;
            const std::string& which = p[1];
            int wasItem = std::stoi(p[2]);
            int held = std::stoi(p[3]);
            const InteractionResultItemContext* ctx = nullptr;
            if (which == "NONE") ctx = &INTERACTION_ITEM_CONTEXT_NONE;
            else if (which == "DEFAULT") ctx = &INTERACTION_ITEM_CONTEXT_DEFAULT;
            if (!ctx) {
                ++mism;
                std::cerr << "CTX unknown which=" << which << "\n";
            } else {
                bool ok = (ctx->wasItemInteraction == (wasItem != 0))
                       && (ctx->heldItemTransformedToPresent == (held != 0));
                if (!ok) {
                    ++mism;
                    std::cerr << "CTX mismatch which=" << which
                              << " expected was=" << wasItem << " held=" << held
                              << " got was=" << ctx->wasItemInteraction
                              << " held=" << ctx->heldItemTransformedToPresent << "\n";
                }
            }
        } else if (tag == "CONST") {
            // CONST <id> <isSuccess> <consumesAction> <swingOrd|-1> <wasItem|-1> <held|-1>
            ++cases;
            const std::string& id = p[1];
            int isSuccess = std::stoi(p[2]);
            int consumes = std::stoi(p[3]);
            int swingOrd = std::stoi(p[4]);
            int wasItem = std::stoi(p[5]);
            int held = std::stoi(p[6]);
            const InteractionResult* r = constById(id);
            if (!r) {
                ++mism;
                std::cerr << "CONST unknown id=" << id << "\n";
            } else {
                bool gotSuccess = (r->kind == InteractionResultKind::SUCCESS);
                bool ok = (gotSuccess == (isSuccess != 0))
                       && (r->consumesAction() == (consumes != 0));
                if (gotSuccess) {
                    ok = ok
                       && (static_cast<int>(r->swingSource) == swingOrd)
                       && (static_cast<int>(r->wasItemInteraction()) == wasItem)
                       && (static_cast<int>(r->heldItemTransformedToPresent()) == held);
                } else {
                    // Java emits -1 for the non-Success swing/item fields.
                    ok = ok && (swingOrd == -1) && (wasItem == -1) && (held == -1);
                }
                if (!ok) {
                    ++mism;
                    std::cerr << "CONST mismatch id=" << id
                              << " expected isSuccess=" << isSuccess
                              << " consumes=" << consumes << " swing=" << swingOrd
                              << " was=" << wasItem << " held=" << held
                              << " | got isSuccess=" << gotSuccess
                              << " consumes=" << r->consumesAction()
                              << " swing=" << static_cast<int>(r->swingSource)
                              << " was=" << r->wasItemInteraction()
                              << " held=" << r->heldItemTransformedToPresent() << "\n";
                }
            }
        } else if (tag == "XFORM") {
            // XFORM <which> <consumesAction> <swingOrd> <wasItem> <held>
            ++cases;
            const std::string& which = p[1];
            int consumes = std::stoi(p[2]);
            int swingOrd = std::stoi(p[3]);
            int wasItem = std::stoi(p[4]);
            int held = std::stoi(p[5]);
            // Base is CONSUME (SwingSource.NONE / ItemContext.DEFAULT), matching the GT.
            InteractionResult base = INTERACTION_CONSUME;
            InteractionResult got;
            bool known = true;
            if (which == "WITHOUT_ITEM") got = base.withoutItem();
            else if (which == "HELD_TRANSFORMED") got = base.heldItemTransformedTo();
            else known = false;
            if (!known) {
                ++mism;
                std::cerr << "XFORM unknown which=" << which << "\n";
            } else {
                bool ok = (got.consumesAction() == (consumes != 0))
                       && (static_cast<int>(got.swingSource) == swingOrd)
                       && (static_cast<int>(got.wasItemInteraction()) == wasItem)
                       && (static_cast<int>(got.heldItemTransformedToPresent()) == held);
                if (!ok) {
                    ++mism;
                    std::cerr << "XFORM mismatch which=" << which
                              << " expected consumes=" << consumes << " swing=" << swingOrd
                              << " was=" << wasItem << " held=" << held
                              << " | got consumes=" << got.consumesAction()
                              << " swing=" << static_cast<int>(got.swingSource)
                              << " was=" << got.wasItemInteraction()
                              << " held=" << got.heldItemTransformedToPresent() << "\n";
                }
            }
        }
        // unknown tags ignored
    }

    std::cout << "InteractionResult cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
