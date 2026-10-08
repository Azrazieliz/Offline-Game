#include "Runtime/OGFoundationDiagnosticMenu.h"

#include "Combat/OGDiagnosticCombatComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Runtime/OGGameCoreSubsystem.h"

namespace
{
constexpr int32 ActionsPerPage = 4;
constexpr int64 DiagnosticDurationTicks = 5;
FName ActionId(const TCHAR* Prefix, uint8 Action)
{
    return FName(*FString::Printf(TEXT("OG.Diagnostic.%s.%u"), Prefix, static_cast<uint32>(Action)));
}
FOGFoundationDiagnosticMenuRow Row(FName Id, const FString& Label, bool Enabled = true, const FString& Reason = FString())
{
    FOGFoundationDiagnosticMenuRow R;
    R.Id = Id; R.Label = Label; R.bEnabled = Enabled; R.Reason = Reason;
    return R;
}
bool IsInventory(EOGFoundationCharacterDiagnosticAction A)
{
    using Action = EOGFoundationCharacterDiagnosticAction;
    return A == Action::AcquireTool || A == Action::EquipTool || A == Action::UnequipTool ||
        A == Action::DamageTool || A == Action::RestoreTool || A == Action::LearnKnownRecipe ||
        A == Action::CraftKnownTool || A == Action::PracticeAffinity || A == Action::PracticeProficiency;
}
bool IsKnowledge(EOGFoundationCharacterDiagnosticAction A)
{
    using Action = EOGFoundationCharacterDiagnosticAction;
    return A == Action::RecordNpcObservation || A == Action::RecordNpcMemory ||
        A == Action::PromoteNpc || A == Action::SpeakToNpc;
}
}

UOGFoundationDiagnosticMenu::UOGFoundationDiagnosticMenu()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UOGFoundationDiagnosticMenu::Resolve(UOGGameCoreSubsystem*& Core,
    UOGDiagnosticCombatComponent*& Combat, FOGFoundationCharacterContext& Character,
    int64& Tick, FString& Error) const
{
    Core = nullptr; Combat = nullptr; Tick = -1;
    Character = FOGFoundationCharacterContext();
    UWorld* World = GetWorld();
    if (!World || !World->GetGameInstance() || !GetOwner())
    { Error = TEXT("Diagnostic controls require a live gameplay owner."); return false; }
    Core = World->GetGameInstance()->GetSubsystem<UOGGameCoreSubsystem>();
    Combat = GetOwner()->FindComponentByClass<UOGDiagnosticCombatComponent>();
    if (!Core || !Core->IsCoreReady() || !Core->GetWorldStore() || !Combat || !Combat->IsActive())
    { Error = TEXT("Enter the ordinary Foundation integration course first."); return false; }
    Tick = Core->GetCanonicalWorldTick();
    if (Tick < 0) { Error = TEXT("Canonical clock is not ready."); return false; }
    FOGContentId Identity;
    bool HasManifestation = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Combat->TryGetControlledContext(Character.EntityId, Identity, HasManifestation, Manifestation, Error)) return false;
    Character.OwnerEntityId = HasManifestation ? Combat->GetRulerId() : Character.EntityId;
    if (HasManifestation) Character.ManifestationId = Character.EntityId;
    return true;
}

bool UOGFoundationDiagnosticMenu::Initialize()
{
    Category = ECategory::Home; Page = 0;
    UOGGameCoreSubsystem* Core; UOGDiagnosticCombatComponent* Combat;
    FOGFoundationCharacterContext Character; int64 Tick;
    if (!Resolve(Core, Combat, Character, Tick, Status)) return false;
    Status = TEXT("Foundation diagnostic actions use canonical character and world state.");
    return true;
}

bool UOGFoundationDiagnosticMenu::EnsureStrategy(UOGGameCoreSubsystem& Core,
    UOGDiagnosticCombatComponent& Combat, int64 Tick, FString& Error)
{
    auto* Runtime = Core.GetFoundationStrategicRuntime();
    auto* Clock = Core.GetCanonicalClockRuntime();
    if (!Runtime || !Clock) { Error = TEXT("Strategic runtime or canonical clock is unavailable."); return false; }
    // Reload selection handles after restore and keep this menu on the canonical Ruler.
    if (!Runtime->EnsureDiagnosticContext(Combat.GetRulerId(), Tick, StrategyContext, Error)) return false;
    return Runtime->BindScheduledActions(StrategyContext, *Clock, Error);
}

TArray<FOGFoundationCharacterDiagnosticMenuEntry> UOGFoundationDiagnosticMenu::CharacterEntries(
    const FOGFoundationCharacterContext& Character, IOGWorldStore& Store, FString& Error) const
{
    TArray<FOGFoundationCharacterDiagnosticMenuEntry> All, Result;
    if (!FOGFoundationCharacterDiagnostics(Store).BuildMenu(Character, All, Error)) return Result;
    for (const auto& Entry : All)
    {
        const bool Inventory = IsInventory(Entry.Action);
        const bool Knowledge = IsKnowledge(Entry.Action);
        if ((Category == ECategory::Inventory && Inventory) ||
            (Category == ECategory::Knowledge && Knowledge) ||
            (Category == ECategory::Character && !Inventory && !Knowledge)) Result.Add(Entry);
    }
    return Result;
}

TArray<FOGFoundationDiagnosticMenuRow> UOGFoundationDiagnosticMenu::GetRows()
{
    TArray<FOGFoundationDiagnosticMenuRow> Result;
    UOGGameCoreSubsystem* Core; UOGDiagnosticCombatComponent* Combat;
    FOGFoundationCharacterContext Character; int64 Tick; FString Error;
    if (!Resolve(Core, Combat, Character, Tick, Error))
    {
        Status = Error;
        Result.Add(Row(FName(TEXT("OG.Diagnostic.Unavailable")), TEXT("Diagnostic context unavailable"), false, Error));
        return Result;
    }
    if (Category == ECategory::Home)
    {
        Result.Add(Row(FName(TEXT("OG.Diagnostic.Category.Strategy")), TEXT("Territory / Domain / Army / Director")));
        Result.Add(Row(FName(TEXT("OG.Diagnostic.Category.Character")), TEXT("Character / progression / presentation")));
        Result.Add(Row(FName(TEXT("OG.Diagnostic.Category.Inventory")), TEXT("Inventory / equipment / Known crafting")));
        Result.Add(Row(FName(TEXT("OG.Diagnostic.Category.Knowledge")), TEXT("Resident / knowledge / memory / dialogue")));
        return Result;
    }
    Result.Add(Row(FName(TEXT("OG.Diagnostic.Home")), TEXT("Back to diagnostic categories")));
    TArray<FOGFoundationDiagnosticMenuRow> Actions;
    if (Category == ECategory::Strategy)
    {
        if (!EnsureStrategy(*Core, *Combat, Tick, Error))
        {
            Status = Error;
            Result.Add(Row(FName(TEXT("OG.Diagnostic.Unavailable")), TEXT("Strategic context unavailable"), false, Error));
            return Result;
        }
        for (const auto& Entry : FOGFoundationStrategicRuntime::GetMenuEntries(StrategyContext))
            Actions.Add(Row(ActionId(TEXT("Strategy"), static_cast<uint8>(Entry.Action)), Entry.Label, Entry.bEnabled, Entry.DisabledReason));
    }
    else
    {
        const auto Entries = CharacterEntries(Character, *Core->GetWorldStore(), Error);
        if (!Error.IsEmpty()) { Status = Error; return Result; }
        for (const auto& Entry : Entries)
            Actions.Add(Row(ActionId(TEXT("Character"), static_cast<uint8>(Entry.Action)), Entry.Label, Entry.bEnabled, Entry.DisabledReason));
    }
    const int32 Pages = FMath::Max(1, (Actions.Num() + ActionsPerPage - 1) / ActionsPerPage);
    Page = FMath::Clamp(Page, 0, Pages - 1);
    for (int32 I = Page * ActionsPerPage; I < FMath::Min(Actions.Num(), (Page + 1) * ActionsPerPage); ++I) Result.Add(Actions[I]);
    if (Pages > 1)
    {
        Result.Add(Row(FName(TEXT("OG.Diagnostic.Previous")), FString::Printf(TEXT("Previous page (%d/%d)"), Page + 1, Pages), Page > 0));
        Result.Add(Row(FName(TEXT("OG.Diagnostic.Next")), FString::Printf(TEXT("Next page (%d/%d)"), Page + 1, Pages), Page + 1 < Pages));
    }
    return Result;
}

bool UOGFoundationDiagnosticMenu::ActivateRow(FName Id)
{
    // Resolve fresh rows, including current selection and availability, before dispatch.
    const auto Rows = GetRows();
    const auto* Selected = Rows.FindByPredicate([Id](const auto& R) { return R.Id == Id; });
    if (!Selected) { Status = TEXT("This diagnostic action is no longer visible; refresh the menu."); return false; }
    if (!Selected->bEnabled) { Status = Selected->Reason.IsEmpty() ? TEXT("This action is unavailable.") : Selected->Reason; return false; }
    if (Id == FName(TEXT("OG.Diagnostic.Home"))) { Category = ECategory::Home; Page = 0; return true; }
    if (Id == FName(TEXT("OG.Diagnostic.Previous"))) { --Page; return true; }
    if (Id == FName(TEXT("OG.Diagnostic.Next"))) { ++Page; return true; }
    if (Id == FName(TEXT("OG.Diagnostic.Category.Strategy"))) { Category = ECategory::Strategy; Page = 0; GetRows(); return true; }
    if (Id == FName(TEXT("OG.Diagnostic.Category.Character"))) { Category = ECategory::Character; Page = 0; return true; }
    if (Id == FName(TEXT("OG.Diagnostic.Category.Inventory"))) { Category = ECategory::Inventory; Page = 0; return true; }
    if (Id == FName(TEXT("OG.Diagnostic.Category.Knowledge"))) { Category = ECategory::Knowledge; Page = 0; return true; }
    UOGGameCoreSubsystem* Core; UOGDiagnosticCombatComponent* Combat;
    FOGFoundationCharacterContext Character; int64 Tick; FString Error;
    if (!Resolve(Core, Combat, Character, Tick, Error)) { Status = Error; return false; }
    if (Combat->IsInTurn()) { Status = TEXT("Finish or leave the active turn encounter before changing diagnostic world state."); return false; }
    if (Category == ECategory::Strategy)
    {
        auto* Runtime = Core->GetFoundationStrategicRuntime();
        auto* Clock = Core->GetCanonicalClockRuntime();
        if (!Runtime || !Clock || !EnsureStrategy(*Core, *Combat, Tick, Error)) { Status = Error; return false; }
        for (const auto& Entry : FOGFoundationStrategicRuntime::GetMenuEntries(StrategyContext))
        {
            if (Id != ActionId(TEXT("Strategy"), static_cast<uint8>(Entry.Action))) continue;
            FOGFoundationStrategicCommandResult CommandResult;
            const bool Project = Entry.Action == EOGFoundationStrategicAction::StartProject;
            const bool Dispatch = Entry.Action == EOGFoundationStrategicAction::StartDispatch;
            IOGWorldStore& Store = *Core->GetWorldStore();
            const FOGFoundationStrategicContext Before = StrategyContext;
            if ((Project || Dispatch) && !Store.BeginTransaction(Error)) { Status = Error; return false; }
            auto RollBack = [&]()
            {
                if (Project || Dispatch)
                {
                    FString RollbackError;
                    if (!Store.RollbackTransaction(RollbackError)) Error += TEXT(" Rollback failed: ") + RollbackError;
                    StrategyContext = Before;
                }
                Status = Error;
                return false;
            };
            if (!Runtime->ExecuteDiagnosticAction(StrategyContext, Entry.Action, Tick, DiagnosticDurationTicks, CommandResult, Error))
                return RollBack();
            if (Project || Dispatch)
            {
                FOGWorldDirectorScheduleRecord Schedule;
                if (!Runtime->ScheduleDueAction(StrategyContext, CommandResult.EntityId,
                    FName(Project ? TEXT("project") : TEXT("dispatch")), Tick, Tick + DiagnosticDurationTicks, Schedule, Error))
                    return RollBack();
                if (!Store.CommitTransaction(Error)) return RollBack();
                // Registration has an in-memory clock cache. Bind only after the
                // canonical command and Director schedule have committed together.
                // EnsureStrategy retries this idempotent binding on the next refresh.
                if (!Runtime->BindScheduledActions(StrategyContext, *Clock, Error))
                {
                    Status = TEXT("Action and Director schedule saved; clock binding pending: ") + Error;
                    return false;
                }
            }
            Status = Entry.Label + TEXT(" — canonical action recorded.");
            return true;
        }
    }
    else
    {
        const auto Entries = CharacterEntries(Character, *Core->GetWorldStore(), Error);
        for (const auto& Entry : Entries)
        {
            if (Id != ActionId(TEXT("Character"), static_cast<uint8>(Entry.Action))) continue;
            FOGFoundationCharacterDiagnosticResult CommandResult;
            if (!FOGFoundationCharacterDiagnostics(*Core->GetWorldStore()).ExecuteDiagnosticAction(Character, Entry.Action, Tick, CommandResult, Error))
            { Status = Error; return false; }
            Status = CommandResult.Message;
            // Re-project the existing controlled character; never replace party slots or HP here.
            if (!Combat->RefreshCommandContext(Error)) Status += TEXT(" Presentation refresh pending: ") + Error;
            OnCanonicalCharacterChanged.Broadcast(CommandResult.ResolvedContext);
            return true;
        }
    }
    Status = Error.IsEmpty() ? TEXT("Unknown diagnostic action.") : Error;
    return false;
}
