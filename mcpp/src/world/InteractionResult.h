#pragma once
#include <cstdint>

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/InteractionResult.java (Minecraft Java Edition 26.1.2).
//
// InteractionResult is a `sealed interface` with four permitted implementations
// (InteractionResult.java:6-10):
//   record Success(SwingSource swingSource, ItemContext itemContext)  — InteractionResult.java:33-54
//   record Fail()                                                     — InteractionResult.java:22-23
//   record Pass()                                                     — InteractionResult.java:30-31
//   record TryEmptyHandInteraction()                                  — InteractionResult.java:62-63
//
// Six interface-level constants are predefined (InteractionResult.java:11-16):
//   SUCCESS            = new Success(SwingSource.CLIENT, ItemContext.DEFAULT)
//   SUCCESS_SERVER     = new Success(SwingSource.SERVER, ItemContext.DEFAULT)
//   CONSUME            = new Success(SwingSource.NONE,   ItemContext.DEFAULT)
//   FAIL               = new Fail()
//   PASS               = new Pass()
//   TRY_WITH_EMPTY_HAND= new TryEmptyHandInteraction()
//
// consumesAction() (InteractionResult.java:18-20, overridden at 34-37):
//   default => false; Success overrides => true.
//   => true ONLY for SUCCESS / SUCCESS_SERVER / CONSUME; false for FAIL / PASS /
//      TRY_WITH_EMPTY_HAND.
//
// SwingSource enum (InteractionResult.java:56-60): NONE=0, CLIENT=1, SERVER=2.
//
// ItemContext record (InteractionResult.java:25-28):
//   field wasItemInteraction (boolean), heldItemTransformedTo (@Nullable ItemStack).
//   NONE    = new ItemContext(false, null)   (InteractionResult.java:26)
//   DEFAULT = new ItemContext(true,  null)   (InteractionResult.java:27)
//
// Success transformations:
//   wasItemInteraction()        => itemContext.wasItemInteraction       (InteractionResult.java:47-49)
//   withoutItem()               => new Success(swingSource, ItemContext.NONE)      (43-45)
//   heldItemTransformedTo(stk)  => new Success(swingSource, ItemContext(true, stk)) (39-41)
//   heldItemTransformedTo()     => itemContext.heldItemTransformedTo     (51-53)
//
// NOT ported (un-ported dependency): the ItemStack-carrying payload of
// heldItemTransformedTo(ItemStack) — there is no ItemStack port yet. The boolean
// surface (wasItemInteraction, and whether heldItemTransformedTo is null) is fully
// modelled; the actual stack object is represented only by a null/non-null flag.
// ---------------------------------------------------------------------------

namespace mc {

// Java: InteractionResult.SwingSource (InteractionResult.java:56-60). Ordinals 0..2.
enum class InteractionResultSwingSource : int32_t {
    NONE = 0,
    CLIENT = 1,
    SERVER = 2,
};

// Java: SwingSource.name() — the enum constant identifier (InteractionResult.java:57-59).
constexpr const char* interactionResultSwingSourceName(InteractionResultSwingSource s) noexcept {
    switch (s) {
        case InteractionResultSwingSource::NONE: return "NONE";
        case InteractionResultSwingSource::CLIENT: return "CLIENT";
        case InteractionResultSwingSource::SERVER: return "SERVER";
    }
    return "";
}

// Java: which permitted record kind an InteractionResult is.
// (InteractionResult.java:7-10 permits list order.)
enum class InteractionResultKind : int32_t {
    SUCCESS = 0,             // record Success
    FAIL = 1,                // record Fail
    PASS = 2,                // record Pass
    TRY_EMPTY_HAND = 3,      // record TryEmptyHandInteraction
};

// Java: InteractionResult.ItemContext (InteractionResult.java:25-28).
// heldItemTransformedTo is @Nullable ItemStack — modelled here as a non-null flag,
// since no ItemStack port exists. The two predefined contexts both have a null stack.
struct InteractionResultItemContext {
    bool wasItemInteraction;
    bool heldItemTransformedToPresent; // false == Java null

    constexpr bool operator==(const InteractionResultItemContext&) const = default;
};

// Java: ItemContext.NONE = new ItemContext(false, null) (InteractionResult.java:26).
inline constexpr InteractionResultItemContext INTERACTION_ITEM_CONTEXT_NONE{false, false};
// Java: ItemContext.DEFAULT = new ItemContext(true, null) (InteractionResult.java:27).
inline constexpr InteractionResultItemContext INTERACTION_ITEM_CONTEXT_DEFAULT{true, false};

// Modelled value of an InteractionResult. For non-Success kinds the swingSource /
// itemContext fields are inert (Java has no such accessors on Fail/Pass/TryEmptyHand).
struct InteractionResult {
    InteractionResultKind kind;
    // Only meaningful when kind == SUCCESS:
    InteractionResultSwingSource swingSource;
    InteractionResultItemContext itemContext;

    // Java: consumesAction() (InteractionResult.java:18-20, override 34-37).
    // default false; Success => true.
    constexpr bool consumesAction() const noexcept {
        return kind == InteractionResultKind::SUCCESS;
    }

    // Java: Success.wasItemInteraction() (InteractionResult.java:47-49).
    // Only valid on Success.
    constexpr bool wasItemInteraction() const noexcept {
        return itemContext.wasItemInteraction;
    }

    // Java: Success.heldItemTransformedTo() (InteractionResult.java:51-53) — null check.
    constexpr bool heldItemTransformedToPresent() const noexcept {
        return itemContext.heldItemTransformedToPresent;
    }

    // Java: Success.withoutItem() (InteractionResult.java:43-45).
    // new Success(this.swingSource, ItemContext.NONE).
    constexpr InteractionResult withoutItem() const noexcept {
        return InteractionResult{InteractionResultKind::SUCCESS, swingSource,
                                 INTERACTION_ITEM_CONTEXT_NONE};
    }

    // Java: Success.heldItemTransformedTo(ItemStack) (InteractionResult.java:39-41).
    // new Success(this.swingSource, new ItemContext(true, itemStack)).
    // The stack is non-null here (the caller passes a real ItemStack).
    constexpr InteractionResult heldItemTransformedTo() const noexcept {
        return InteractionResult{InteractionResultKind::SUCCESS, swingSource,
                                 InteractionResultItemContext{true, true}};
    }

    constexpr bool operator==(const InteractionResult&) const = default;
};

// Java predefined constants (InteractionResult.java:11-16).
// SUCCESS = new Success(CLIENT, DEFAULT).
inline constexpr InteractionResult INTERACTION_SUCCESS{
    InteractionResultKind::SUCCESS, InteractionResultSwingSource::CLIENT,
    INTERACTION_ITEM_CONTEXT_DEFAULT};
// SUCCESS_SERVER = new Success(SERVER, DEFAULT).
inline constexpr InteractionResult INTERACTION_SUCCESS_SERVER{
    InteractionResultKind::SUCCESS, InteractionResultSwingSource::SERVER,
    INTERACTION_ITEM_CONTEXT_DEFAULT};
// CONSUME = new Success(NONE, DEFAULT).
inline constexpr InteractionResult INTERACTION_CONSUME{
    InteractionResultKind::SUCCESS, InteractionResultSwingSource::NONE,
    INTERACTION_ITEM_CONTEXT_DEFAULT};
// FAIL = new Fail(). swingSource/itemContext inert.
inline constexpr InteractionResult INTERACTION_FAIL{
    InteractionResultKind::FAIL, InteractionResultSwingSource::NONE,
    INTERACTION_ITEM_CONTEXT_NONE};
// PASS = new Pass().
inline constexpr InteractionResult INTERACTION_PASS{
    InteractionResultKind::PASS, InteractionResultSwingSource::NONE,
    INTERACTION_ITEM_CONTEXT_NONE};
// TRY_WITH_EMPTY_HAND = new TryEmptyHandInteraction().
inline constexpr InteractionResult INTERACTION_TRY_WITH_EMPTY_HAND{
    InteractionResultKind::TRY_EMPTY_HAND, InteractionResultSwingSource::NONE,
    INTERACTION_ITEM_CONTEXT_NONE};

} // namespace mc
