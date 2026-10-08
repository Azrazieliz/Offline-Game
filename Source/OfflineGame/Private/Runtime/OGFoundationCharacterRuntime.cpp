#include "Runtime/OGFoundationCharacterRuntime.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Progression/OGCharacterProgressionService.h"
#include "Progression/OGClassRecognitionService.h"
#include "Progression/OGFactorService.h"
#include "Progression/OGGrandConvergenceService.h"
#include "Progression/OGRankService.h"
#include "World/OGCharacterWorldStateService.h"
#include "World/OGSharedWorldStateService.h"

namespace
{
bool ReadObject(const FString& Json, TSharedPtr<FJsonObject>& Out, FString& Error)
{
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Out) || !Out.IsValid())
    { Error = TEXT("Canonical state is not a JSON object; preserving existing state."); return false; }
    return true;
}
FString WriteObject(const TSharedPtr<FJsonObject>& Object)
{
    FString Json;
    FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
    return Json;
}
bool Finish(IOGWorldStore& Store, bool bSuccess, FString& Error)
{
    if (bSuccess && Store.CommitTransaction(Error)) return true;
    FString RollbackError;
    Store.RollbackTransaction(RollbackError);
    return false;
}
void AddHistory(const TSharedPtr<FJsonObject>& Object, const FString& Action,
    const FOGEntityId& Event, int64 Tick)
{
    const TArray<TSharedPtr<FJsonValue>>* Previous = nullptr;
    TArray<TSharedPtr<FJsonValue>> History;
    if (Object->TryGetArrayField(TEXT("foundation_changes"), Previous)) History = *Previous;
    TSharedPtr<FJsonObject> Change = MakeShared<FJsonObject>();
    Change->SetStringField(TEXT("action"), Action);
    Change->SetStringField(TEXT("source_event_id"), Event.ToString());
    // Integer ticks are serialized as strings to preserve 64-bit precision.
    Change->SetStringField(TEXT("world_tick"), LexToString(Tick));
    History.Add(MakeShared<FJsonValueObject>(Change));
    Object->SetArrayField(TEXT("foundation_changes"), History);
}
bool IsBroken(const FOGItemInstanceRecord& Item)
{
    TSharedPtr<FJsonObject> State;
    FString Error;
    bool Broken = false;
    if (!ReadObject(Item.DurabilityStateJson, State, Error)) return true;
    State->TryGetBoolField(TEXT("broken"), Broken);
    double Condition = 10000;
    State->TryGetNumberField(TEXT("condition_bps"), Condition);
    return Broken || !FMath::IsFinite(Condition) || Condition <= 0 || Condition > 10000;
}
bool FactMatches(const FOGKnowledgeFactRecord& Fact, FName Key,
    const FOGEntityId& Subject, int32 Confidence, FName Belief)
{
    return Fact.FactKey == Key && (!Subject.IsValid() || Fact.SubjectEntityId == Subject) &&
        Fact.ConfidenceBps >= Confidence && (Belief.IsNone() || Fact.BeliefState == Belief);
}
}

bool FOGFoundationCharacterRuntime::Resolve(const FOGFoundationCharacterContext& Context,
    FOGFoundationCharacterContext& Out, int64 Tick, FString& Error) const
{
    Error.Reset();
    Out = Context;
    if (!Store.IsOpen() || !Context.EntityId.IsValid() || Tick < 0)
    { Error = TEXT("Character context requires an open canonical store, entity and valid world tick."); return false; }
    bool Found = false;
    FName Kind;
    FString State;
    int64 Revision = 0;
    if (!Store.TryReadEntity(Context.EntityId, Found, Kind, State, Revision, Error)) return false;
    if (!Found) { Error = TEXT("Character context references an unknown entity."); return false; }
    FOGCharacterManifestationRecord Manifestation;
    if (Context.ManifestationId.IsValid() && Context.ManifestationId != Context.EntityId)
    { Error = TEXT("Selected Manifestation must be the selected canonical entity."); return false; }
    if (!Store.TryReadCharacterManifestation(Context.EntityId, Found, Manifestation, Error)) return false;
    if (Context.ManifestationId.IsValid() && !Found)
    { Error = TEXT("Character context references an unknown Manifestation."); return false; }
    if (Found)
    {
        if (Context.OwnerEntityId.IsValid() && Context.OwnerEntityId != Manifestation.OwningRulerId)
        { Error = TEXT("Selected Manifestation does not belong to this owner."); return false; }
        Out.ManifestationId = Manifestation.ManifestationId;
        Out.OwnerEntityId = Manifestation.OwningRulerId;
    }
    else
    {
        if (Out.OwnerEntityId.IsValid() && Out.OwnerEntityId != Out.EntityId)
        { Error = TEXT("Non-Manifestation context must use its own canonical ownership."); return false; }
        Out.OwnerEntityId = Out.EntityId;
    }
    return true;
}

bool FOGFoundationCharacterRuntime::RequireManifestation(const FOGFoundationCharacterContext& Context,
    int64 Tick, FString& Error) const
{
    FOGFoundationCharacterContext Resolved;
    if (!Resolve(Context, Resolved, Tick, Error)) return false;
    if (!Resolved.ManifestationId.IsValid())
    { Error = TEXT("This command requires a canonical Manifestation."); return false; }
    return true;
}

bool FOGFoundationCharacterRuntime::Project(const FOGFoundationCharacterContext& Context,
    FOGFoundationCharacterProjection& Out, FString& Error) const
{
    Out = FOGFoundationCharacterProjection();
    FOGFoundationCharacterProjection P;
    if (!Resolve(Context, P.Context, 0, Error)) return false;
    bool Found = false;
    if (!Store.TryReadEntity(P.Context.EntityId, Found, P.EntityKind, P.EntityStateJson, P.EntityRevision, Error) ||
        !Store.TryReadCharacterManifestation(P.Context.EntityId, P.bHasManifestation, P.Manifestation, Error) ||
        !Store.TryReadWorldPresence(P.Context.EntityId, P.bHasPresence, P.Presence, Error) ||
        !Store.TryReadEntityRankState(P.Context.EntityId, P.bHasRank, P.Rank, Error) ||
        !FOGRankService(Store).ResolveEffectiveRank(P.Context.EntityId, Found, P.EffectiveRank, Error) ||
        !Store.ListFactorInstancesByOwner(P.Context.EntityId, P.Factors, Error) ||
        !Store.ListEntityClasses(P.Context.EntityId, P.Classes, Error) ||
        !Store.ListEntitySkills(P.Context.EntityId, P.Skills, Error) ||
        !Store.ListItemInstancesByOwner(P.Context.OwnerEntityId, P.Inventory, Error) ||
        !Store.ListEquipmentProficienciesByOwner(P.Context.EntityId, P.Proficiencies, Error) ||
        !Store.TryReadManifestationPresentationState(P.Context.EntityId, P.bHasPresentation, P.Presentation, Error) ||
        !Store.TryReadCharacterAdultRuntimeState(P.Context.EntityId, P.bHasAdultContext, P.AdultContext, Error) ||
        !Store.TryReadNpcPromotionState(P.Context.EntityId, P.bHasNpcPromotion, P.NpcPromotion, Error) ||
        !Store.ListKnowledgeFactsByOwner(P.Context.EntityId, P.Knowledge, Error) ||
        !Store.ListSemanticMemoriesByOwner(P.Context.EntityId, P.Memories, Error)) return false;
    for (const FOGEntitySkillRecord& Skill : P.Skills)
    {
        TArray<FOGSkillProvenanceRecord> Sources;
        if (!Store.ListSkillProvenance(P.Context.EntityId, Skill.SkillId, Sources, Error)) return false;
        P.SkillProvenance.Append(Sources);
    }
    if (P.bHasManifestation &&
        (!Store.ListManifestationRouteNodes(P.Context.EntityId, P.Routes, Error) ||
         !Store.ListManifestationForms(P.Context.EntityId, P.Forms, Error) ||
         !Store.TryReadManifestationReinforcement(P.Context.EntityId, P.bHasReinforcement, P.Reinforcement, Error) ||
         !Store.ListCharacterConvergencesByIdentity(P.Manifestation.IdentityId, P.Convergences, Error))) return false;
    if (!P.bHasManifestation)
    {
        TSharedPtr<FJsonObject> EntityState;
        if (!ReadObject(P.EntityStateJson, EntityState, Error)) return false;
        const TSharedPtr<FJsonObject>* Presentation = nullptr;
        if (EntityState->TryGetObjectField(TEXT("foundation_presentation"), Presentation))
        {
            P.bHasPresentation = true;
            P.Presentation.ManifestationId = P.Context.EntityId;
            FString Skin;
            (*Presentation)->TryGetStringField(TEXT("skin_id"), Skin);
            P.Presentation.SelectedSkinId = FOGContentId(Skin);
            (*Presentation)->TryGetStringField(TEXT("outfit_state_json"), P.Presentation.OutfitStateJson);
            (*Presentation)->TryGetStringField(TEXT("variant_state_json"), P.Presentation.PresentationVariantStateJson);
            FString Tick;
            if ((*Presentation)->TryGetStringField(TEXT("updated_world_tick"), Tick))
                LexTryParseString(P.Presentation.UpdatedWorldTick, *Tick);
        }
    }
    TArray<FOGEquipmentBindingRecord> Bindings;
    if (!Store.ListEquipmentBindings(P.Context.EntityId, Bindings, Error)) return false;
    for (const FOGEquipmentBindingRecord& Binding : Bindings)
    {
        FOGFoundationEquippedItemProjection Item;
        Item.Binding = Binding;
        if (!Store.TryReadItemInstance(Binding.ItemId, Found, Item.Item, Error)) return false;
        if (!Found) { Error = TEXT("Canonical equipment binding references a missing item."); return false; }
        if (!Store.ListItemModifiers(Binding.ItemId, Item.Modifiers, Error) ||
            !Store.TryReadItemOwnerAffinity(Binding.ItemId, P.Context.EntityId,
                Item.bHasAffinity, Item.Affinity, Error)) return false;
        Item.bBroken = IsBroken(Item.Item);
        P.Equipment.Add(Item);
    }
    Out = MoveTemp(P);
    return true;
}

bool FOGFoundationCharacterRuntime::ProjectEquipmentFunctions(const FOGFoundationCharacterContext& Context,
    const FOGFoundationEquipmentResolver& Resolver, FOGFoundationEquipmentFunctions& Out, FString& Error) const
{
    Out = FOGFoundationEquipmentFunctions();
    if (!Resolver) { Error = TEXT("Equipment mechanics require an authored resolver."); return false; }
    FOGFoundationCharacterProjection P;
    if (!Project(Context, P, Error)) return false;
    FOGFoundationEquipmentFunctions Result;
    for (const FOGFoundationEquippedItemProjection& Item : P.Equipment)
    {
        if (Item.bBroken) continue;
        FOGFoundationEquipmentFunctions Contribution;
        if (!Resolver(P, Item, Contribution, Error)) return false;
        for (const auto& Pair : Contribution.StatContributions)
        {
            if (Pair.Key.IsNone() || Pair.Value.Significand > FOGLargeNumber::MaxSignificand ||
                Pair.Value.Significand < -FOGLargeNumber::MaxSignificand)
            { Error = TEXT("Authored equipment resolver returned invalid stat data."); return false; }
            FOGLargeNumber& Total = Result.StatContributions.FindOrAdd(Pair.Key);
            Total = FOGLargeNumber::Add(Total, Pair.Value);
        }
        for (const FOGContentId& Function : Contribution.FunctionIds)
        {
            if (!Function.IsValid()) { Error = TEXT("Authored equipment resolver returned invalid function data."); return false; }
            Result.FunctionIds.AddUnique(Function);
        }
    }
    Out = MoveTemp(Result);
    return true;
}

bool FOGFoundationCharacterRuntime::SetRank(const FOGFoundationCharacterContext& C,
    FOGEntityRankStateRecord State, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    State.EntityId = R.EntityId; State.UpdatedWorldTick = Tick;
    return FOGRankService(Store).SetRankState(State, Error);
}
bool FOGFoundationCharacterRuntime::ProjectIntegratedKit(const FOGFoundationCharacterContext& C,
    const FOGFoundationKitResolver& Resolver, FOGResolvedSkillSet& Out, FString& Error) const
{
    Out = FOGResolvedSkillSet();
    if (!Resolver) { Error = TEXT("Integrated skills require an authored kit resolver."); return false; }
    FOGFoundationCharacterProjection P;
    if (!Project(C, P, Error)) return false;
    FOGResolvedSkillSet Kit;
    if (!Resolver(P, Kit, Error)) return false;
    if (Kit.HasDuplicateSkills()) { Error = TEXT("Integrated kit contains duplicate skills."); return false; }
    for (const auto& Skill : Kit.ActiveSkills)
        if (!Skill.IsValid()) { Error = TEXT("Integrated active skill is invalid."); return false; }
    for (const auto& Skill : Kit.PassiveSkills)
        if (!Skill.IsValid()) { Error = TEXT("Integrated passive skill is invalid."); return false; }
    if ((!Kit.UltimateSkill.IsEmpty() && !Kit.UltimateSkill.IsValid()) ||
        (!Kit.CurrentForm.IsEmpty() && !Kit.CurrentForm.IsValid()))
    { Error = TEXT("Integrated kit form/ultimate identity is invalid."); return false; }
    Out = MoveTemp(Kit);
    return true;
}
bool FOGFoundationCharacterRuntime::SetPresence(const FOGFoundationCharacterContext& C,
    FOGWorldPresenceRecord State, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    State.EntityId = R.EntityId; State.UpdatedWorldTick = Tick;
    return FOGSharedWorldStateService(Store).UpdatePhysicalPresence(State, Error);
}
bool FOGFoundationCharacterRuntime::AcquireFactor(const FOGFoundationCharacterContext& C,
    FOGFactorInstanceRecord State, const TArray<FOGFactorLineageRecord>& Lineage, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    State.OwnerEntityId = R.EntityId; State.AcquiredWorldTick = Tick;
    return FOGFactorService(Store).AcquireFactor(State, Lineage, Error);
}
bool FOGFoundationCharacterRuntime::RecognizeClass(const FOGFoundationCharacterContext& C,
    FOGEntityClassRecord State, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    State.OwnerEntityId = R.EntityId; State.UpdatedWorldTick = Tick;
    if (State.RecognizedWorldTick == 0) State.RecognizedWorldTick = Tick;
    return FOGClassRecognitionService(Store).RecognizeClass(State, Error);
}
bool FOGFoundationCharacterRuntime::SetFactorExpression(const FOGFoundationCharacterContext& C,
    const FOGEntityId& FactorId, int32 Weight, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    bool Found = false;
    FOGFactorInstanceRecord Factor;
    if (!Store.TryReadFactorInstance(FactorId, Found, Factor, Error)) return false;
    if (!Found || Factor.OwnerEntityId != R.EntityId)
    { Error = TEXT("Factor expression requires an owned causal Factor."); return false; }
    return FOGFactorService(Store).SetExpressionWeight(FactorId, Weight, Tick, Error);
}
bool FOGFoundationCharacterRuntime::LearnSkill(const FOGFoundationCharacterContext& C,
    FOGEntitySkillRecord State, TArray<FOGSkillProvenanceRecord> Sources, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    State.OwnerEntityId = R.EntityId;
    TArray<FOGEntitySkillRecord> Existing;
    if (!Store.ListEntitySkills(R.EntityId, Existing, Error)) return false;
    State.LearnedWorldTick = Tick;
    for (const auto& Skill : Existing)
        if (Skill.SkillId == State.SkillId) State.LearnedWorldTick = Skill.LearnedWorldTick;
    for (auto& Source : Sources)
    { Source.OwnerEntityId = R.EntityId; Source.SkillId = State.SkillId; Source.SourceWorldTick = Tick; }
    return FOGCharacterProgressionService(Store).LearnSkill(State, Sources, Error);
}
bool FOGFoundationCharacterRuntime::SetRouteNode(const FOGFoundationCharacterContext& C,
    FOGManifestationRouteNodeRecord State, int64 Tick, FString& Error)
{
    if (!RequireManifestation(C, Tick, Error)) return false;
    State.ManifestationId = C.EntityId;
    return FOGCharacterProgressionService(Store).SetRouteNode(State, Error);
}
bool FOGFoundationCharacterRuntime::SetForm(const FOGFoundationCharacterContext& C,
    FOGManifestationFormRecord State, int64 Tick, FString& Error)
{
    if (!RequireManifestation(C, Tick, Error)) return false;
    State.ManifestationId = C.EntityId;
    TArray<FOGManifestationFormRecord> Existing;
    if (!Store.ListManifestationForms(C.EntityId, Existing, Error)) return false;
    State.UnlockedWorldTick = Tick;
    for (const auto& Form : Existing)
        if (Form.FormId == State.FormId) State.UnlockedWorldTick = Form.UnlockedWorldTick;
    return FOGCharacterProgressionService(Store).UnlockOrUpdateForm(State, Error);
}
bool FOGFoundationCharacterRuntime::SetReinforcement(const FOGFoundationCharacterContext& C,
    FOGManifestationReinforcementRecord State, int64 Tick, FString& Error)
{
    if (!RequireManifestation(C, Tick, Error)) return false;
    State.ManifestationId = C.EntityId; State.UpdatedWorldTick = Tick;
    return FOGCharacterProgressionService(Store).SetReinforcement(State, Error);
}
bool FOGFoundationCharacterRuntime::Converge(const FOGFoundationCharacterContext& C,
    FOGCharacterManifestationRecord Result, const TArray<FOGEntityId>& Sources,
    const TArray<FOGContentId>& Lineage, const FOGContentId& Rule, int64 Tick,
    const FString& Json, bool Confirm, FOGEntityId& Out, FString& Error)
{
    Out = FOGEntityId();
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    if (!Sources.Contains(R.EntityId)) { Error = TEXT("Convergence must include the selected source."); return false; }
    Result.OwningRulerId = R.OwnerEntityId;
    return FOGGrandConvergenceService(Store).PerformConvergence(Result, Sources, Lineage, Rule,
        Tick, Json, Confirm, Out, Error);
}

bool FOGFoundationCharacterRuntime::AcquireItem(const FOGFoundationCharacterContext& C,
    const FOGContentId& Definition, const FOGContentId& Rank, const FOGContentId& Quality,
    int64 Tick, FOGEntityId& Out, FString& Error)
{
    Out = FOGEntityId();
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    return FOGInventoryEquipmentService(Store).CreateItem(R.OwnerEntityId, Definition, Rank, Quality, Tick, Out, Error);
}
bool FOGFoundationCharacterRuntime::Equip(const FOGFoundationCharacterContext& C,
    const FOGContentId& Slot, const FOGEntityId& ItemId,
    const FOGEquipmentSlotValidator& Validator, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, 0, Error)) return false;
    if (!Validator) { Error = TEXT("Equipment compatibility requires authored body/slot rules."); return false; }
    FOGEquipmentSlotValidator Guard = [R, Validator](const FOGEntityId& Wearer,
        const FOGContentId& SlotId, const FOGItemInstanceRecord& Item, FString& E)
    {
        if (Item.OwnerEntityId != R.OwnerEntityId && Item.OwnerEntityId != R.EntityId)
        { E = TEXT("Equipment is not owned by the selected character or its ruler."); return false; }
        if (IsBroken(Item)) { E = TEXT("Broken equipment must be restored before equipping."); return false; }
        return Validator(Wearer, SlotId, Item, E);
    };
    // Validate before modifying any existing bindings.
    bool Found = false;
    FOGItemInstanceRecord Item;
    if (!Store.TryReadItemInstance(ItemId, Found, Item, Error)) return false;
    if (!Found) { Error = TEXT("Equipment references an unknown item."); return false; }
    if (!Guard(R.EntityId, Slot, Item, Error)) return false;
    if (!Store.BeginTransaction(Error)) return false;
    return Finish(Store, Store.DeleteEquipmentBindingsForItem(ItemId, Error) &&
        FOGInventoryEquipmentService(Store).EquipItem(R.EntityId, Slot, ItemId, Guard, Error), Error);
}
bool FOGFoundationCharacterRuntime::Unequip(const FOGFoundationCharacterContext& C,
    const FOGContentId& Slot, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, 0, Error)) return false;
    bool Found = false;
    FOGEquipmentBindingRecord Binding;
    if (!Store.TryReadEquipmentBinding(R.EntityId, Slot, Found, Binding, Error)) return false;
    if (!Found) return true;
    return Store.DeleteEquipmentBindingsForItem(Binding.ItemId, Error);
}
bool FOGFoundationCharacterRuntime::SetAffinity(const FOGFoundationCharacterContext& C,
    FOGItemOwnerAffinityRecord State, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    bool Found = false;
    FOGItemInstanceRecord Item;
    if (!Store.TryReadItemInstance(State.ItemId, Found, Item, Error)) return false;
    if (!Found || (Item.OwnerEntityId != R.EntityId && Item.OwnerEntityId != R.OwnerEntityId))
    { Error = TEXT("Affinity command requires an owned item."); return false; }
    State.OwnerEntityId = R.EntityId; State.UpdatedWorldTick = Tick;
    return Store.UpsertItemOwnerAffinity(State, Error);
}
bool FOGFoundationCharacterRuntime::SetProficiency(const FOGFoundationCharacterContext& C,
    FOGEquipmentProficiencyRecord State, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    State.OwnerEntityId = R.EntityId; State.UpdatedWorldTick = Tick;
    return FOGInventoryEquipmentService(Store).SetProficiency(State, Error);
}
bool FOGFoundationCharacterRuntime::ChangeDurability(const FOGFoundationCharacterContext& C,
    const FOGEntityId& ItemId, int32 Amount, bool Restore, const FOGEntityId& Event, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    if (Amount <= 0 || Amount > 10000 || !Event.IsValid())
    { Error = TEXT("Equipment durability change requires positive basis points and a source event."); return false; }
    bool Found = false;
    FOGItemInstanceRecord Item;
    if (!Store.TryReadItemInstance(ItemId, Found, Item, Error)) return false;
    if (!Found || (Item.OwnerEntityId != R.EntityId && Item.OwnerEntityId != R.OwnerEntityId))
    { Error = TEXT("Equipment durability change requires an owned item."); return false; }
    TSharedPtr<FJsonObject> Durability, History;
    if (!ReadObject(Item.DurabilityStateJson, Durability, Error) || !ReadObject(Item.HistoryStateJson, History, Error)) return false;
    double Current = 10000;
    Durability->TryGetNumberField(TEXT("condition_bps"), Current);
    if (!FMath::IsFinite(Current) || Current < 0 || Current > 10000)
    { Error = TEXT("Existing durability condition is invalid."); return false; }
    const int32 Updated = FMath::Clamp(static_cast<int32>(Current) + (Restore ? Amount : -Amount), 0, 10000);
    Durability->SetNumberField(TEXT("condition_bps"), Updated);
    Durability->SetBoolField(TEXT("broken"), Updated == 0);
    AddHistory(History, Restore ? TEXT("restored") : TEXT("damaged"), Event, Tick);
    Item.DurabilityStateJson = WriteObject(Durability);
    Item.HistoryStateJson = WriteObject(History);
    if (!Store.BeginTransaction(Error)) return false;
    return Finish(Store, Store.UpsertItemInstance(Item, Tick, Error) &&
        (Updated != 0 || Store.DeleteEquipmentBindingsForItem(ItemId, Error)), Error);
}
bool FOGFoundationCharacterRuntime::DamageEquipment(const FOGFoundationCharacterContext& C,
    const FOGEntityId& Item, int32 Amount, const FOGEntityId& Event, int64 Tick, FString& Error)
{ return ChangeDurability(C, Item, Amount, false, Event, Tick, Error); }
bool FOGFoundationCharacterRuntime::RestoreEquipment(const FOGFoundationCharacterContext& C,
    const FOGEntityId& Item, int32 Amount, const FOGEntityId& Event, int64 Tick, FString& Error)
{ return ChangeDurability(C, Item, Amount, true, Event, Tick, Error); }
bool FOGFoundationCharacterRuntime::SetPresentation(const FOGFoundationCharacterContext& C,
    FOGManifestationPresentationStateRecord State, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    State.ManifestationId = R.EntityId; State.UpdatedWorldTick = Tick;
    // Outfit layers, damage and skin are presentation data; no Rank/form inference.
    if (R.ManifestationId.IsValid())
        return FOGInventoryEquipmentService(Store).SetPresentationState(State, Error);
    if (!State.SelectedSkinId.IsEmpty() && !State.SelectedSkinId.IsValid())
    { Error = TEXT("Selected skin must be an authored content identity."); return false; }
    TSharedPtr<FJsonObject> Outfit, Variant;
    if (!ReadObject(State.OutfitStateJson, Outfit, Error) ||
        !ReadObject(State.PresentationVariantStateJson, Variant, Error)) return false;
    bool Found = false;
    FName Kind;
    FString Json;
    int64 Revision = 0;
    if (!Store.TryReadEntity(R.EntityId, Found, Kind, Json, Revision, Error)) return false;
    TSharedPtr<FJsonObject> EntityState;
    if (!ReadObject(Json, EntityState, Error)) return false;
    TSharedPtr<FJsonObject> Presentation = MakeShared<FJsonObject>();
    Presentation->SetStringField(TEXT("skin_id"), State.SelectedSkinId.ToString());
    Presentation->SetStringField(TEXT("outfit_state_json"), State.OutfitStateJson);
    Presentation->SetStringField(TEXT("variant_state_json"), State.PresentationVariantStateJson);
    Presentation->SetStringField(TEXT("updated_world_tick"), LexToString(Tick));
    EntityState->SetObjectField(TEXT("foundation_presentation"), Presentation);
    return Store.UpsertEntity(R.EntityId, Kind, Tick, WriteObject(EntityState), Error);
}
bool FOGFoundationCharacterRuntime::SetInjury(const FOGFoundationCharacterContext& C,
    FName Injury, int32 Severity, const FOGEntityId& Event, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    if (Injury.IsNone() || Severity < 0 || Severity > 10000 || !Event.IsValid())
    { Error = TEXT("Injury requires authored state, basis-point severity and source event."); return false; }
    bool Found = false;
    FName Kind;
    FString Json;
    int64 Revision = 0;
    if (!Store.TryReadEntity(R.EntityId, Found, Kind, Json, Revision, Error)) return false;
    TSharedPtr<FJsonObject> State;
    if (!ReadObject(Json, State, Error)) return false;
    State->SetStringField(TEXT("injury_state"), Injury.ToString());
    State->SetNumberField(TEXT("injury_severity_bps"), Severity);
    AddHistory(State, Severity == 0 ? TEXT("injury_restored") : TEXT("injured"), Event, Tick);
    return Store.UpsertEntity(R.EntityId, Kind, Tick, WriteObject(State), Error);
}
bool FOGFoundationCharacterRuntime::RestoreInjury(const FOGFoundationCharacterContext& C,
    const FOGEntityId& Event, int64 Tick, FString& Error)
{ return SetInjury(C, FName(TEXT("restored")), 0, Event, Tick, Error); }

bool FOGFoundationCharacterRuntime::CraftKnown(const FOGFoundationCharacterContext& C,
    const FOGFoundationKnownRecipe& Recipe, int64 Tick, FOGEntityId& Out, FString& Error)
{
    Out = FOGEntityId();
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    if (!Recipe.RecipeId.IsValid() || Recipe.RequiredKnowledgeKey.IsNone() ||
        !Recipe.OutputDefinitionId.IsValid() || Recipe.MinimumConfidenceBps < 0 || Recipe.MinimumConfidenceBps > 10000)
    { Error = TEXT("Known crafting requires a valid authored recipe and knowledge requirement."); return false; }
    TArray<FOGKnowledgeFactRecord> Facts;
    if (!Store.ListKnowledgeFactsByOwner(R.EntityId, Facts, Error)) return false;
    const FOGKnowledgeFactRecord* Evidence = Facts.FindByPredicate([&Recipe](const FOGKnowledgeFactRecord& Fact)
    { return FactMatches(Fact, Recipe.RequiredKnowledgeKey, Recipe.KnowledgeSubjectId,
        Recipe.MinimumConfidenceBps, NAME_None) && Fact.BeliefState != FName(TEXT("refuted")); });
    if (!Evidence) { Error = TEXT("Selected character lacks sufficiently confident recipe knowledge."); return false; }
    TMap<FOGContentId, int64> Costs;
    for (const auto& Cost : Recipe.Costs)
    {
        if (!Cost.ResourceId.IsValid() || Cost.Amount <= 0 || Costs.Contains(Cost.ResourceId))
        { Error = TEXT("Craft recipe costs must be positive and have unique resource IDs."); return false; }
        Costs.Add(Cost.ResourceId, Cost.Amount);
    }
    if (!Store.BeginTransaction(Error)) return false;
    bool Success = true;
    for (const auto& Cost : Costs)
    {
        bool Found = false;
        int64 Balance = 0;
        if (!Store.TryReadResourceBalance(R.OwnerEntityId, Cost.Key, Found, Balance, Error)) { Success = false; break; }
        if (!Found || Balance < Cost.Value) { Error = TEXT("Insufficient resources for known crafting."); Success = false; break; }
        if (!Store.SetResourceBalance(R.OwnerEntityId, Cost.Key, Balance - Cost.Value, Error)) { Success = false; break; }
    }
    FOGEntityId Created;
    if (Success) Success = FOGInventoryEquipmentService(Store).CreateItem(R.OwnerEntityId,
        Recipe.OutputDefinitionId, Recipe.OutputRankId, Recipe.OutputQualityId, Tick, Created, Error);
    if (Success)
    {
        bool Found = false;
        FOGItemInstanceRecord Item;
        Success = Store.TryReadItemInstance(Created, Found, Item, Error) && Found;
        if (Success)
        {
            TSharedPtr<FJsonObject> History;
            Success = ReadObject(Item.HistoryStateJson, History, Error);
            if (Success)
            {
                History->SetStringField(TEXT("recipe_id"), Recipe.RecipeId.ToString());
                History->SetStringField(TEXT("crafter_entity_id"), R.EntityId.ToString());
                History->SetStringField(TEXT("knowledge_source_entity_id"), Evidence->SourceEntityId.ToString());
                AddHistory(History, TEXT("crafted_known"), Evidence->SourceEventId, Tick);
                Item.HistoryStateJson = WriteObject(History);
                Success = Store.UpsertItemInstance(Item, Tick, Error);
            }
        }
    }
    if (!Finish(Store, Success, Error)) return false;
    Out = Created;
    return true;
}
bool FOGFoundationCharacterRuntime::PromoteNpc(const FOGFoundationCharacterContext& C,
    const FOGContentId& Tier, const FOGEntityId& Event, const FString& Package, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    return FOGCharacterWorldStateService(Store).PromoteNpc(R.EntityId, Tier, Tick, Event, Package, Error);
}
bool FOGFoundationCharacterRuntime::RecordKnowledge(const FOGFoundationCharacterContext& C,
    FOGKnowledgeFactRecord Fact, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    if ((Fact.bHasEvidenceWorldTick && Fact.EvidenceWorldTick > Tick) ||
        (!Fact.SourceEntityId.IsValid() && !Fact.SourceEventId.IsValid()))
    { Error = TEXT("Knowledge requires received source provenance and nonfuture evidence."); return false; }
    Fact.OwnerEntityId = R.EntityId; Fact.UpdatedWorldTick = Tick;
    bool Found = false;
    FOGKnowledgeFactRecord Previous;
    if (!Store.TryReadKnowledgeFact(R.EntityId, Fact.FactKey, Fact.SubjectEntityId, Found, Previous, Error)) return false;
    Fact.LearnedWorldTick = Found ? Previous.LearnedWorldTick : Tick;
    return FOGCharacterWorldStateService(Store).RecordKnowledge(Fact, Error);
}
bool FOGFoundationCharacterRuntime::RecordMemory(const FOGFoundationCharacterContext& C,
    FOGSemanticMemoryRecord Memory, int64 Tick, FString& Error)
{
    FOGFoundationCharacterContext R;
    if (!Resolve(C, R, Tick, Error)) return false;
    Memory.OwnerEntityId = R.EntityId;
    if (!Memory.MemoryId.IsValid()) Memory.MemoryId = FOGEntityId::NewId();
    return FOGCharacterWorldStateService(Store).RecordSemanticMemory(Memory, Tick, Error);
}

bool FOGFoundationCharacterRuntime::FilterDialogue(const FOGFoundationCharacterContext& C,
    const TArray<FOGFoundationDialogueLine>& Pool, TArray<FOGFoundationDialogueLine>& Eligible,
    FOGFoundationDialogueProjection& Out, FString& Error) const
{
    Eligible.Reset(); Out = FOGFoundationDialogueProjection();
    FOGFoundationCharacterProjection P;
    if (!Project(C, P, Error)) return false;
    TSharedPtr<FJsonObject> State;
    if (!ReadObject(P.EntityStateJson, State, Error)) return false;
    FString Injury;
    State->TryGetStringField(TEXT("injury_state"), Injury);
    TSet<FOGContentId> Seen;
    for (const auto& Line : Pool)
    {
        if (!Line.LineId.IsValid() || Line.Text.IsEmpty() || Seen.Contains(Line.LineId) ||
            Line.MinimumConfidenceBps < 0 || Line.MinimumConfidenceBps > 10000 ||
            Line.MinimumMemorySalienceBps < 0 || Line.MinimumMemorySalienceBps > 10000)
        { Error = TEXT("Dialogue pool requires unique valid IDs, text and confidence bounds."); return false; }
        Seen.Add(Line.LineId);
        if (!Line.RequiredMovementContext.IsNone() &&
            (!P.bHasPresence || P.Presence.MovementContext != Line.RequiredMovementContext)) continue;
        if (Line.RequiredLocationId.IsValid() &&
            (!P.bHasPresence || P.Presence.LocationId != Line.RequiredLocationId)) continue;
        if (!Line.RequiredSimulationTier.IsEmpty() &&
            (!P.bHasNpcPromotion || P.NpcPromotion.SimulationTierId != Line.RequiredSimulationTier)) continue;
        if (!Line.RequiredInjuryState.IsNone() && Line.RequiredInjuryState.ToString() != Injury) continue;
        bool StateMatches = true;
        for (const auto& Requirement : Line.RequiredStateValues)
        {
            FString Value;
            if (!State->TryGetStringField(Requirement.Key.ToString(), Value) || Value != Requirement.Value)
            { StateMatches = false; break; }
        }
        if (!StateMatches) continue;
        if (!Line.RequiredMemoryTypeId.IsEmpty() && !P.Memories.ContainsByPredicate([&Line](const auto& Memory)
            { return Memory.MemoryTypeId == Line.RequiredMemoryTypeId &&
                (!Line.RequiredMemorySubjectId.IsValid() || Memory.SubjectEntityId == Line.RequiredMemorySubjectId) &&
                Memory.SalienceBps >= Line.MinimumMemorySalienceBps; })) continue;
        if (!Line.RequiredKnowledgeKey.IsNone() && !P.Knowledge.ContainsByPredicate([&Line](const FOGKnowledgeFactRecord& Fact)
            { return FactMatches(Fact, Line.RequiredKnowledgeKey, Line.KnowledgeSubjectId,
                Line.MinimumConfidenceBps, Line.RequiredBeliefState); })) continue;
        Eligible.Add(Line);
    }
    Eligible.Sort([](const auto& A, const auto& B) { return A.LineId.ToString() < B.LineId.ToString(); });
    P.Knowledge.Sort([](const auto& A, const auto& B)
    {
        if (A.ConfidenceBps != B.ConfidenceBps) return A.ConfidenceBps > B.ConfidenceBps;
        if (A.UpdatedWorldTick != B.UpdatedWorldTick) return A.UpdatedWorldTick > B.UpdatedWorldTick;
        if (A.FactKey != B.FactKey) return A.FactKey.ToString() < B.FactKey.ToString();
        return A.SubjectEntityId.ToString() < B.SubjectEntityId.ToString();
    });
    P.Memories.Sort([](const auto& A, const auto& B)
    {
        if (A.SalienceBps != B.SalienceBps) return A.SalienceBps > B.SalienceBps;
        return A.MemoryId.ToString() < B.MemoryId.ToString();
    });
    if (P.Knowledge.Num() > 8) P.Knowledge.SetNum(8);
    if (P.Memories.Num() > 8) P.Memories.SetNum(8);
    Out.RetrievedKnowledge = MoveTemp(P.Knowledge);
    Out.RetrievedMemories = MoveTemp(P.Memories);
    return true;
}
bool FOGFoundationCharacterRuntime::SelectDialogue(const FOGFoundationCharacterContext& C,
    const TArray<FOGFoundationDialogueLine>& Pool, uint32 Seed,
    FOGFoundationDialogueProjection& Out, FString& Error) const
{
    TArray<FOGFoundationDialogueLine> Eligible;
    if (!FilterDialogue(C, Pool, Eligible, Out, Error)) return false;
    if (Eligible.IsEmpty())
    {
        // Neutral fallback makes no factual claim or model call.
        Out.Text = TEXT("I have nothing further to add.");
        return true;
    }
    const auto& Line = Eligible[Seed % static_cast<uint32>(Eligible.Num())];
    Out.LineId = Line.LineId; Out.Text = Line.Text;
    return true;
}
bool FOGFoundationCharacterRuntime::SelectGeneratedDialogueLine(const FOGFoundationCharacterContext& C,
    const TArray<FOGFoundationDialogueLine>& Pool, const FOGContentId& Candidate,
    uint32 Seed, FOGFoundationDialogueProjection& Out, FString& Error) const
{
    TArray<FOGFoundationDialogueLine> Eligible;
    if (!FilterDialogue(C, Pool, Eligible, Out, Error)) return false;
    const auto* Line = Eligible.FindByPredicate([&Candidate](const auto& Entry) { return Entry.LineId == Candidate; });
    if (!Line) return SelectDialogue(C, Pool, Seed, Out, Error);
    Out.LineId = Line->LineId; Out.Text = Line->Text; Out.bUsedLocalFallback = false;
    return true;
}
