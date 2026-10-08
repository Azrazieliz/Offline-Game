#include "Combat/OGWorldActionRuntime.h"
#include "Combat/OGActionCombatAdapter.h"
#include "World/OGTraversalFramework.h"

bool FOGActionCancelWindow::Allows(float ElapsedSeconds, EOGActionCancelDestination Destination) const
{
    return ElapsedSeconds >= OpensAtSeconds &&
        ElapsedSeconds <= ClosesAtSeconds &&
        Destinations.Contains(Destination);
}

UOGWorldActionRuntimeComponent::UOGWorldActionRuntimeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UOGWorldActionRuntimeComponent::BeginAuthoredAction(const FOGWorldActionState& State)
{
    ActiveAction = State;
    ActiveAction.StartedAtSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    bHasActiveAction = true;
}

void UOGWorldActionRuntimeComponent::EndAuthoredAction()
{
    bHasActiveAction = false;
    ActiveAction = FOGWorldActionState();
}

bool UOGWorldActionRuntimeComponent::CanCancelTo(EOGActionCancelDestination Destination) const
{
    if (!bHasActiveAction || !GetWorld())
    {
        return true;
    }

    const float Elapsed = GetWorld()->GetTimeSeconds() - ActiveAction.StartedAtSeconds;
    for (const FOGActionCancelWindow& Window : ActiveAction.CancelWindows)
    {
        if (Window.Allows(Elapsed, Destination))
        {
            return true;
        }
    }
    return false;
}

bool UOGWorldActionRuntimeComponent::CanContinueInTraversalMode(uint8 TraversalMode) const
{
    if (!bHasActiveAction)
    {
        return true;
    }

    switch (static_cast<EOGTraversalMode>(TraversalMode))
    {
    case EOGTraversalMode::Climbing: return ActiveAction.bAllowWhileClimbing;
    case EOGTraversalMode::Swimming: return ActiveAction.bAllowWhileSwimming;
    case EOGTraversalMode::Diving: return ActiveAction.bAllowWhileDiving;
    case EOGTraversalMode::Flying: return ActiveAction.bAllowWhileFlying;
    case EOGTraversalMode::Mounted:
    case EOGTraversalMode::Vehicle: return ActiveAction.bAllowWhileMounted;
    default: return ActiveAction.bAllowWhileAirborne;
    }
}

UOGWorldPartyRuntimeComponent::UOGWorldPartyRuntimeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UOGWorldPartyRuntimeComponent::ConfigureParty(const TArray<FOGCombatUnitState>& Units, FString& OutError)
{
    if (!FOGActionCombatAdapter::ValidateSwitchParty(Units, OutError)) return false;
    Slots.Reset(); ControlledSlot = INDEX_NONE;
    for (const FOGCombatUnitState& Unit : Units)
    {
        FOGWorldPartySlot& Slot = Slots.AddDefaulted_GetRef();
        Slot.Unit = FOGActionCombatAdapter::MakeActionSnapshot(Unit);
        Slot.bDefeated = !Slot.Unit.IsAlive();
        Slot.bAvailable = !Slot.bDefeated;
        Slot.Unit.Presence = Slot.bDefeated ? EOGCombatPresence::Defeated : EOGCombatPresence::Reserve;
        if (ControlledSlot == INDEX_NONE && Slot.bAvailable) ControlledSlot = Slots.Num() - 1;
    }
    if (Slots.IsValidIndex(ControlledSlot)) Slots[ControlledSlot].Unit.Presence = EOGCombatPresence::Active;
    return true;
}

bool UOGWorldPartyRuntimeComponent::TrySwitchTo(int32 SlotIndex, bool bStateAllowsSwitch)
{
    if (!bStateAllowsSwitch || !Slots.IsValidIndex(SlotIndex) || SlotIndex == ControlledSlot ||
        !Slots[SlotIndex].bAvailable || Slots[SlotIndex].bDefeated || !Slots[SlotIndex].Unit.IsAlive()) return false;
    const int32 Previous = ControlledSlot;
    if (Slots.IsValidIndex(Previous) && !Slots[Previous].bDefeated)
        Slots[Previous].Unit.Presence = EOGCombatPresence::Reserve;
    ControlledSlot = SlotIndex;
    Slots[ControlledSlot].Unit.Presence = EOGCombatPresence::Active;
    Slots[ControlledSlot].bQteReady = false;
    OnControlTransferRequested(Previous, ControlledSlot);
    return true;
}

bool UOGWorldPartyRuntimeComponent::TryTriggerQte(int32 SlotIndex)
{
    if (!Slots.IsValidIndex(ControlledSlot) || !Slots[ControlledSlot].Unit.IsAlive() ||
        !Slots.IsValidIndex(SlotIndex) || SlotIndex == ControlledSlot ||
        !Slots[SlotIndex].bAvailable || Slots[SlotIndex].bDefeated ||
        !Slots[SlotIndex].Unit.IsAlive() || !Slots[SlotIndex].bQteReady) return false;
    Slots[SlotIndex].bQteReady = false;
    OnQteRequested(ControlledSlot, SlotIndex);
    return true;
}

int32 UOGWorldPartyRuntimeComponent::MarkDefeatedAndResolveFallback(int32 SlotIndex)
{
    if (!Slots.IsValidIndex(SlotIndex)) return ControlledSlot;
    Slots[SlotIndex].Unit.CurrentHp = FOGLargeNumber();
    Slots[SlotIndex].Unit.Presence = EOGCombatPresence::Defeated;
    Slots[SlotIndex].bDefeated = true; Slots[SlotIndex].bAvailable = false;
    Slots[SlotIndex].bQteReady = false;
    if (ControlledSlot != SlotIndex) return ControlledSlot;
    const int32 Previous = ControlledSlot;
    ControlledSlot = INDEX_NONE;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
        if (Slots[Index].bAvailable && !Slots[Index].bDefeated && Slots[Index].Unit.IsAlive())
        { ControlledSlot = Index; Slots[Index].Unit.Presence = EOGCombatPresence::Active; Slots[Index].bQteReady = false; break; }
    OnControlTransferRequested(Previous, ControlledSlot);
    return ControlledSlot;
}

void UOGWorldPartyRuntimeComponent::SetQteReady(int32 SlotIndex, bool bReady)
{
    if (Slots.IsValidIndex(SlotIndex))
        Slots[SlotIndex].bQteReady = bReady && SlotIndex != ControlledSlot &&
            Slots[SlotIndex].bAvailable && !Slots[SlotIndex].bDefeated && Slots[SlotIndex].Unit.IsAlive();
}


// REVIEW FRAGMENT: add declaration in UOGWorldPartyRuntimeComponent public:
// bool ApplyResolvedHp(int32 SlotIndex, const FOGEntityId& ExpectedUnitId,
//     const FOGLargeNumber& NewHp, FString& OutError);
// No UFUNCTION is needed: only the native canonical combat integration calls it.
// This mutation does not own damage math, progression, or authored revive rules.

bool UOGWorldPartyRuntimeComponent::ApplyResolvedHp(int32 SlotIndex,
    const FOGEntityId& ExpectedUnitId, const FOGLargeNumber& NewHp, FString& OutError)
{
    OutError.Reset();
    if (!Slots.IsValidIndex(SlotIndex) || !ExpectedUnitId.IsValid() ||
        Slots[SlotIndex].Unit.UnitEntityId != ExpectedUnitId || NewHp.GetSign() < 0)
    {
        OutError = TEXT("Party HP update requires the expected slot entity and nonnegative resolved HP.");
        return false;
    }
    FOGWorldPartySlot& Slot = Slots[SlotIndex];
    if (Slot.bDefeated && NewHp.GetSign() > 0)
    {
        OutError = TEXT("Defeated party units require an authored revival operation.");
        return false;
    }
    // Do not clamp to MaxHp: explicit overflow-HP mechanics belong to effects.
    Slot.Unit.CurrentHp = NewHp;
    if (NewHp.IsZero() && !Slot.bDefeated)
    {
        // Existing runtime owns defeat flags, QTE removal and fallback choice.
        MarkDefeatedAndResolveFallback(SlotIndex);
    }
    return true;
}

// Add public native declarations to the existing party runtime:
// bool AssignResolvedCompanion(int32 SlotIndex, const FOGCombatUnitState& Unit, FString& Error);
// bool RestorePresentationParty(const TArray<FOGWorldPartySlot>& SavedSlots, int32 SavedControl, FString& Error);
// Content callers must prove ownership/physical anchoring before assignment.

bool UOGWorldPartyRuntimeComponent::AssignResolvedCompanion(int32 SlotIndex,
    const FOGCombatUnitState& Unit, FString& Error)
{
    Error.Reset();
    if (SlotIndex < 1 || SlotIndex > 2 || SlotIndex > Slots.Num() || !Unit.IsAlive())
    { Error = TEXT("Companion assignment requires a living resolved unit in companion slot 1 or 2."); return false; }
    TArray<FOGCombatUnitState> Candidate;
    for (const FOGWorldPartySlot& Slot : Slots) Candidate.Add(Slot.Unit);
    if (SlotIndex == Candidate.Num()) Candidate.Add(Unit); else Candidate[SlotIndex] = Unit;
    if (!FOGActionCombatAdapter::ValidateSwitchParty(Candidate, Error)) return false;
    FOGWorldPartySlot Replacement;
    Replacement.Unit = Unit;
    Replacement.Unit.Presence = SlotIndex == ControlledSlot ? EOGCombatPresence::Active : EOGCombatPresence::Reserve;
    const bool bSameUnit = Slots.IsValidIndex(SlotIndex) &&
        Slots[SlotIndex].Unit.UnitEntityId == Unit.UnitEntityId;
    if (!bSameUnit)
    {
        if (SlotIndex == Slots.Num()) Slots.Add(Replacement); else Slots[SlotIndex] = Replacement;
    }
    // Selecting a different living owned unit can recover a defeated party;
    // assigning an existing unit never restores or replaces its persisted HP.
    if (ControlledSlot == INDEX_NONE && Slots[SlotIndex].Unit.IsAlive())
        return TrySwitchTo(SlotIndex, true);
    return true;
}

bool UOGWorldPartyRuntimeComponent::RestorePresentationParty(
    const TArray<FOGWorldPartySlot>& SavedSlots, int32 SavedControl, FString& Error)
{
    Error.Reset();
    if (SavedSlots.IsEmpty() || SavedSlots.Num() > 3 ||
        (SavedControl != INDEX_NONE && !SavedSlots.IsValidIndex(SavedControl)))
    { Error = TEXT("Saved presentation party/control is invalid."); return false; }
    TArray<FOGCombatUnitState> Units;
    for (const FOGWorldPartySlot& Slot : SavedSlots) Units.Add(Slot.Unit);
    if (!FOGActionCombatAdapter::ValidateSwitchParty(Units, Error)) return false;
    if (SavedControl != INDEX_NONE &&
        (!SavedSlots[SavedControl].bAvailable || SavedSlots[SavedControl].bDefeated || !SavedSlots[SavedControl].Unit.IsAlive()))
    { Error = TEXT("Saved controlled slot is unavailable/defeated."); return false; }
    for (int32 Index = 0; Index < SavedSlots.Num(); ++Index)
    {
        const FOGWorldPartySlot& Slot = SavedSlots[Index];
        if (Slot.bDefeated != !Slot.Unit.IsAlive() || (!Slot.Unit.IsAlive() && Slot.bAvailable))
        { Error = TEXT("Saved HP and defeat/availability flags disagree."); return false; }
    }
    Slots = SavedSlots;
    ControlledSlot = SavedControl;
    for (int32 Index = 0; Index < Slots.Num(); ++Index)
        Slots[Index].Unit.Presence = Slots[Index].bDefeated ? EOGCombatPresence::Defeated :
            Index == ControlledSlot ? EOGCombatPresence::Active : EOGCombatPresence::Reserve;
    return true;
}

bool UOGWorldPartyRuntimeComponent::ApplyResolvedCharacterProjection(int32 SlotIndex, const FOGCombatUnitState& Projection, FString& Error)
{
    if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].Unit.UnitEntityId != Projection.UnitEntityId ||
        Slots[SlotIndex].Unit.IdentityId != Projection.IdentityId || Projection.Stats.MaxHp.GetSign() <= 0)
    { Error = TEXT("Character projection does not match the current canonical party unit."); return false; }
    auto& Unit = Slots[SlotIndex].Unit;
    Unit.Stats = Projection.Stats;
    Unit.RankProjection = Projection.RankProjection;
    Unit.SkillSet = Projection.SkillSet;
    return true;
}
