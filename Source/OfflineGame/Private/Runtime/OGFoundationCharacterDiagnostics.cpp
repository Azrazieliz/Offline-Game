#include "Runtime/OGFoundationCharacterDiagnostics.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
FOGEntityId ScopedEntity(const FOGEntityId& Scope, uint32 Salt)
{
    const FGuid& Id = Scope.Value;
    return FOGEntityId(FGuid(Id.A ^ 0x46444348u, Id.B ^ Salt, Id.C ^ 0x504F4F4Cu, Id.D ^ 0x53434F50u));
}
FOGFoundationCharacterDiagnosticDefinitions Definitions()
{
    FOGFoundationCharacterDiagnosticDefinitions D;
    D.FactorId = FOGContentId(TEXT("foundationdiag:factor.practice"));
    D.ClassId = FOGContentId(TEXT("foundationdiag:class.field_practice"));
    D.SkillId = FOGContentId(TEXT("foundationdiag:skill.field_practice"));
    D.RouteId = FOGContentId(TEXT("foundationdiag:route.practice"));
    D.NodeId = FOGContentId(TEXT("foundationdiag:node.first_session"));
    D.FormId = FOGContentId(TEXT("foundationdiag:form.practiced_stance"));
    D.ItemId = FOGContentId(TEXT("foundationdiag:item.practice_tool"));
    D.SlotId = FOGContentId(TEXT("foundationdiag:slot.practice_tool"));
    D.ProficiencyId = FOGContentId(TEXT("foundationdiag:proficiency.practice_tool"));
    D.ItemFunctionId = FOGContentId(TEXT("foundationdiag:function.practice_guard"));
    D.OuterLayerId = FOGContentId(TEXT("foundationdiag:outfit.outer_layer"));
    D.NpcTierId = FOGContentId(TEXT("foundationdiag:npc_tier.named_resident"));
    D.NpcIdentity.IdentityId = FOGContentId(TEXT("foundationdiag:identity.resident"));
    D.NpcIdentity.DisplayNameKey = TEXT("foundationdiag.resident");
    D.NpcIdentity.CanonicalMaturity = EOGCanonicalMaturity::Unknown;
    D.Recipe.RecipeId = FOGContentId(TEXT("foundationdiag:recipe.practice_tool"));
    D.Recipe.RequiredKnowledgeKey = FName(TEXT("foundationdiag_recipe_practice_tool"));
    D.Recipe.MinimumConfidenceBps = 10000;
    D.Recipe.OutputDefinitionId = D.ItemId;
    // This foundation specimen is an authored zero-material exercise recipe.
    // Real recipes pass their actual resource costs to the same CraftKnown API.
    return D;
}
bool Object(const FString& Json, TSharedPtr<FJsonObject>& Out, FString& Error)
{
    if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Out) && Out.IsValid()) return true;
    Error = TEXT("Existing diagnostic state is not an object; preserving it.");
    return false;
}
FString Json(const TSharedPtr<FJsonObject>& State)
{
    FString Result;
    FJsonSerializer::Serialize(State.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
    return Result;
}
const FOGItemInstanceRecord* Tool(const FOGFoundationCharacterProjection& P,
    const FOGFoundationCharacterDiagnosticDefinitions& D)
{
    return P.Inventory.FindByPredicate([&D](const auto& Item) { return Item.DefinitionId == D.ItemId; });
}
FOGEntityId ResidentId(const FOGFoundationCharacterContext& C)
{
    return ScopedEntity(C.OwnerEntityId.IsValid() ? C.OwnerEntityId : C.EntityId, 0x4E504301u);
}
}

bool FOGFoundationCharacterDiagnostics::EnsureDiagnosticDefinitions(const FOGFoundationCharacterContext& C,
    FOGFoundationCharacterDiagnosticDefinitions& D, FOGFoundationCharacterContext& Resolved, FString& Error) const
{
    FOGFoundationCharacterProjection P;
    if (!FOGFoundationCharacterRuntime(Store).Project(C, P, Error)) return false;
    D = Definitions(); Resolved = P.Context;
    return true;
}

bool FOGFoundationCharacterDiagnostics::BuildMenu(const FOGFoundationCharacterContext& C,
    TArray<FOGFoundationCharacterDiagnosticMenuEntry>& Out, FString& Error) const
{
    Out.Reset();
    FOGFoundationCharacterProjection P;
    if (!FOGFoundationCharacterRuntime(Store).Project(C, P, Error)) return false;
    const auto D = Definitions();
    const auto* Item = Tool(P, D);
    const bool Equipped = P.Equipment.ContainsByPredicate([&D](const auto& E) { return E.Binding.SlotId == D.SlotId; });
    const bool KnowsRecipe = P.Knowledge.ContainsByPredicate([&D](const auto& K)
        { return K.FactKey == D.Recipe.RequiredKnowledgeKey && K.ConfidenceBps == 10000; });
    auto Add = [&Out](EOGFoundationCharacterDiagnosticAction Action, const TCHAR* Label,
        bool Enabled = true, const TCHAR* Reason = TEXT(""))
    {
        FOGFoundationCharacterDiagnosticMenuEntry Row;
        Row.Action = Action; Row.Label = Label; Row.bEnabled = Enabled;
        if (!Enabled) Row.DisabledReason = Reason;
        Out.Add(MoveTemp(Row));
    };
    using A = EOGFoundationCharacterDiagnosticAction;
    Add(A::InspectRank, TEXT("View attained / effective / peak Rank"));
    Add(A::RecordPracticeRank, TEXT("Record current Rank through development service"));
    Add(A::PracticeFactor, TEXT("Practice Factor expression"));
    Add(A::RecognizePracticeClass, TEXT("Recognize field practice Class"));
    Add(A::LearnPracticeSkill, TEXT("Integrate practice skill"));
    Add(A::CompletePracticeNode, TEXT("Complete first practice node"), P.bHasManifestation, TEXT("Select a Manifestation."));
    Add(A::UnlockPracticeForm, TEXT("Unlock practiced stance"), P.bHasManifestation, TEXT("Select a Manifestation."));
    Add(A::PracticeReinforcement, TEXT("Record reinforcement practice"), P.bHasManifestation, TEXT("Select a Manifestation."));
    Add(A::AcquireTool, TEXT("Acquire practice tool"));
    Add(A::EquipTool, TEXT("Equip practice tool"), Item != nullptr, TEXT("Acquire or craft a practice tool."));
    Add(A::UnequipTool, TEXT("Unequip practice tool"), Equipped, TEXT("Equip a practice tool first."));
    Add(A::DamageTool, TEXT("Damage / break practice tool"), Item != nullptr, TEXT("Acquire or craft a practice tool."));
    Add(A::RestoreTool, TEXT("Restore practice tool"), Item != nullptr, TEXT("Acquire or craft a practice tool."));
    Add(A::LearnKnownRecipe, TEXT("Learn practice recipe"));
    Add(A::CraftKnownTool, TEXT("Craft known practice recipe"), KnowsRecipe, TEXT("Learn the recipe first."));
    Add(A::PracticeAffinity, TEXT("Record tool affinity practice"), Item != nullptr, TEXT("Acquire or craft a practice tool."));
    Add(A::PracticeProficiency, TEXT("Record tool proficiency practice"));
    Add(A::WearOuterLayer, TEXT("Wear authored outer outfit layer"));
    Add(A::RecordInjury, TEXT("Record minor training injury"));
    Add(A::RestoreInjury, TEXT("Restore training injury"));
    Add(A::RecordNpcObservation, TEXT("Let resident observe this character"));
    Add(A::RecordNpcMemory, TEXT("Consolidate resident's observation memory"));
    Add(A::PromoteNpc, TEXT("Promote resident while preserving history"));
    Add(A::SpeakToNpc, TEXT("Talk with resident (local dialogue)"));
    return true;
}

bool FOGFoundationCharacterDiagnostics::EnsureResident(const FOGFoundationCharacterProjection& P,
    const FOGFoundationCharacterDiagnosticDefinitions& D, int64 Tick,
    FOGFoundationCharacterContext& Out, FString& Error)
{
    Out.EntityId = ResidentId(P.Context);
    bool Found = false;
    FName Kind;
    FString State;
    int64 Revision = 0;
    if (!Store.TryReadEntity(Out.EntityId, Found, Kind, State, Revision, Error)) return false;
    if (Found)
    {
        TSharedPtr<FJsonObject> Existing;
        if (!Object(State, Existing, Error)) return false;
        FString Identity;
        if (!Existing->TryGetStringField(TEXT("identity_content_id"), Identity) || Identity != D.NpcIdentity.IdentityId.ToString())
        { Error = TEXT("Scoped resident ID already belongs to a different canonical entity."); return false; }
        return true;
    }
    auto Identity = MakeShared<FJsonObject>();
    Identity->SetStringField(TEXT("identity_content_id"), D.NpcIdentity.IdentityId.ToString());
    Identity->SetStringField(TEXT("display_name_key"), D.NpcIdentity.DisplayNameKey);
    Identity->SetStringField(TEXT("history_origin"), TEXT("local_resident"));
    if (!Store.BeginTransaction(Error)) return false;
    bool Success = Store.UpsertEntity(Out.EntityId, FName(TEXT("npc")), Tick, Json(Identity), Error);
    if (Success && P.bHasPresence)
    {
        FOGWorldPresenceRecord Presence = P.Presence;
        Presence.EntityId = Out.EntityId; Presence.UpdatedWorldTick = Tick;
        Presence.LocalPosition += FVector(150.0, 0.0, 0.0);
        Success = Store.UpsertWorldPresence(Presence, Error);
    }
    if (Success && Store.CommitTransaction(Error)) return true;
    FString RollbackError; Store.RollbackTransaction(RollbackError);
    return false;
}

bool FOGFoundationCharacterDiagnostics::ResolveEquipmentFunctions(const FOGFoundationCharacterProjection&,
    const FOGFoundationEquippedItemProjection& Item, FOGFoundationEquipmentFunctions& Out, FString& Error)
{
    Out = FOGFoundationEquipmentFunctions(); Error.Reset();
    const auto D = Definitions();
    if (Item.Item.DefinitionId != D.ItemId || Item.bBroken) return true;
    Out.StatContributions.Add(FName(TEXT("guard")), FOGLargeNumber::FromInt64(2));
    Out.FunctionIds.Add(D.ItemFunctionId);
    return true;
}

bool FOGFoundationCharacterDiagnostics::ExecuteDiagnosticAction(const FOGFoundationCharacterContext& C,
    EOGFoundationCharacterDiagnosticAction Action, int64 Tick,
    FOGFoundationCharacterDiagnosticResult& Out, FString& Error)
{
    Out = FOGFoundationCharacterDiagnosticResult();
    if (!Store.BeginTransaction(Error)) return false;
    if (ExecuteInternal(C, Action, Tick, Out, Error) && Store.CommitTransaction(Error)) return true;
    FString RollbackError;
    Store.RollbackTransaction(RollbackError);
    Out = FOGFoundationCharacterDiagnosticResult();
    return false;
}

bool FOGFoundationCharacterDiagnostics::ExecuteInternal(const FOGFoundationCharacterContext& C,
    EOGFoundationCharacterDiagnosticAction Action, int64 Tick,
    FOGFoundationCharacterDiagnosticResult& Out, FString& Error)
{
    Out = FOGFoundationCharacterDiagnosticResult();
    if (Tick < 0) { Error = TEXT("Diagnostic action requires the current canonical world tick."); return false; }
    FOGFoundationCharacterRuntime Runtime(Store);
    FOGFoundationCharacterProjection P;
    if (!Runtime.Project(C, P, Error)) return false;
    const auto D = Definitions();
    const auto R = P.Context;
    Out.ResolvedContext = R;
    const auto* Item = Tool(P, D);
    using A = EOGFoundationCharacterDiagnosticAction;
    auto Result = [&Out](bool Success, const TCHAR* Message)
    { if (Success) Out.Message = Message; return Success; };
    // Source events are authored only on requested actions, never while building menus.
    auto Event = [&](FOGEntityId& EventId, FName Kind)
    {
        FOGWorldEvent E; E.EventId = FOGEntityId::NewId(); E.EventType = Kind;
        E.WorldTick = Tick; E.PrimaryEntity = R.EntityId;
        E.PayloadJson = TEXT("{\"content_scope\":\"foundationdiag\"}");
        if (!Store.AppendWorldEvent(E, Error)) return false;
        EventId = E.EventId; return true;
    };
    switch (Action)
    {
    case A::InspectRank:
        Out.Message = P.bHasRank ? FString::Printf(TEXT("Attained %s L%d | Effective %s L%d | Peak %s L%d"),
            *P.Rank.AttainedRankId.ToString(), P.Rank.AttainedLevel,
            *P.EffectiveRank.RankId.ToString(), P.EffectiveRank.Level,
            *P.Rank.PeakRankId.ToString(), P.Rank.PeakLevel) : TEXT("No Rank state has been recorded for this entity.");
        return true;
    case A::PracticeFactor:
    {
        const auto* Existing = P.Factors.FindByPredicate([&D](const auto& F) { return F.FactorId == D.FactorId; });
        if (Existing) return Result(Runtime.SetFactorExpression(R, Existing->FactorInstanceId,
            Existing->ExpressionWeightBps >= 5000 ? 1000 : 5000, Tick, Error), TEXT("Owned Factor expression updated; other Factors remain causal."));
        FOGFactorInstanceRecord F; F.FactorInstanceId = FOGEntityId::NewId(); F.FactorId = D.FactorId;
        F.SourceEntityId = R.EntityId; F.ExpressionWeightBps = 1000; F.MaturityBps = 1000;
        return Result(Runtime.AcquireFactor(R, F, {}, Tick, Error), TEXT("Practice Factor acquired in canonical development."));
    }
    case A::RecordPracticeRank:
    {
        auto Rank = P.bHasRank ? P.Rank : FOGEntityRankStateRecord();
        if (!P.bHasRank)
        {
            Rank.AttainedRankId = FOGContentId(TEXT("foundationdiag:rank.practice"));
            Rank.PeakRankId = Rank.AttainedRankId;
        }
        return Result(Runtime.SetRank(R, Rank, Tick, Error), TEXT("Rank recorded through canonical development; existing attained, peak and effective state retained."));
    }
    case A::RecognizePracticeClass:
    {
        FOGEntityClassRecord Class;
        const auto* Existing = P.Classes.FindByPredicate([&D](const auto& V) { return V.ClassId == D.ClassId; });
        if (Existing) Class = *Existing;
        Class.ClassId = D.ClassId;
        return Result(Runtime.RecognizeClass(R, Class, Tick, Error), TEXT("Field practice Class recognized."));
    }
    case A::LearnPracticeSkill:
    {
        FOGEntitySkillRecord Skill;
        const auto* Existing = P.Skills.FindByPredicate([&D](const auto& V) { return V.SkillId == D.SkillId; });
        if (Existing) Skill = *Existing;
        Skill.SkillId = D.SkillId; Skill.CurrentState = FName(TEXT("integrated"));
        FOGSkillProvenanceRecord Source; Source.SourceKind = FName(TEXT("practice"));
        Source.SourceContentOrEntityId = D.NodeId.ToString();
        return Result(Runtime.LearnSkill(R, Skill, { Source }, Tick, Error), TEXT("Practice skill integrated with source provenance."));
    }
    case A::CompletePracticeNode:
    {
        FOGManifestationRouteNodeRecord Node;
        const auto* Existing = P.Routes.FindByPredicate([&D](const auto& V) { return V.RouteId == D.RouteId && V.NodeId == D.NodeId; });
        if (Existing) Node = *Existing;
        Node.RouteId = D.RouteId; Node.NodeId = D.NodeId; Node.State = FName(TEXT("completed"));
        if (!Node.bHasEnteredWorldTick) { Node.bHasEnteredWorldTick = true; Node.EnteredWorldTick = Tick; }
        if (!Node.bHasCompletedWorldTick) { Node.bHasCompletedWorldTick = true; Node.CompletedWorldTick = Tick; }
        return Result(Runtime.SetRouteNode(R, Node, Tick, Error), TEXT("Practice route node completed; other routes remain available."));
    }
    case A::UnlockPracticeForm:
    {
        FOGManifestationFormRecord Form;
        const auto* Existing = P.Forms.FindByPredicate([&D](const auto& V) { return V.FormId == D.FormId; });
        if (Existing) Form = *Existing;
        Form.FormId = D.FormId;
        return Result(Runtime.SetForm(R, Form, Tick, Error), TEXT("Practiced stance unlocked independently of presentation skins."));
    }
    case A::PracticeReinforcement:
    {
        auto Reinforcement = P.bHasReinforcement ? P.Reinforcement : FOGManifestationReinforcementRecord();
        return Result(Runtime.SetReinforcement(R, Reinforcement, Tick, Error), TEXT("Reinforcement practice recorded; attained reinforcement retained."));
    }
    case A::AcquireTool:
        return Result(Runtime.AcquireItem(R, D.ItemId, FOGContentId(), FOGContentId(), Tick, Out.ItemId, Error), TEXT("Practice tool acquired."));
    case A::EquipTool:
    {
        if (!Item) { Error = TEXT("Acquire or craft a practice tool first."); return false; }
        Out.ItemId = Item->ItemId;
        FOGEquipmentSlotValidator Slot = [D](const FOGEntityId&, const FOGContentId& SlotId,
            const FOGItemInstanceRecord& V, FString& E)
        {
            if (SlotId == D.SlotId && V.DefinitionId == D.ItemId) return true;
            E = TEXT("Only the authored practice tool fits this diagnostic slot."); return false;
        };
        return Result(Runtime.Equip(R, D.SlotId, Item->ItemId, Slot, Error), TEXT("Practice tool equipped: guard +2 and practice guard function."));
    }
    case A::UnequipTool:
        return Result(Runtime.Unequip(R, D.SlotId, Error), TEXT("Practice tool unequipped; item and history retained."));
    case A::DamageTool:
    case A::RestoreTool:
    {
        if (!Item) { Error = TEXT("Acquire or craft a practice tool first."); return false; }
        FOGEntityId SourceEvent;
        if (!Event(SourceEvent, FName(TEXT("foundationdiag_equipment_change")))) return false;
        Out.ItemId = Item->ItemId;
        return Action == A::DamageTool ? Result(Runtime.DamageEquipment(R, Item->ItemId, 10000, SourceEvent, Tick, Error),
            TEXT("Practice tool broken; functional binding removed, history retained.")) :
            Result(Runtime.RestoreEquipment(R, Item->ItemId, 10000, SourceEvent, Tick, Error), TEXT("Practice tool restored; equip it again to use its function."));
    }
    case A::LearnKnownRecipe:
    {
        FOGKnowledgeFactRecord Fact; Fact.FactKey = D.Recipe.RequiredKnowledgeKey;
        Fact.SourceEntityId = R.EntityId; Fact.ConfidenceBps = 10000;
        Fact.ValueJson = TEXT("{\"recipe_id\":\"foundationdiag:recipe.practice_tool\"}");
        return Result(Runtime.RecordKnowledge(R, Fact, Tick, Error), TEXT("Known practice recipe recorded with source and confidence."));
    }
    case A::CraftKnownTool:
        return Result(Runtime.CraftKnown(R, D.Recipe, Tick, Out.ItemId, Error), TEXT("Known exercise recipe crafted with recipe provenance."));
    case A::PracticeAffinity:
    {
        if (!Item) { Error = TEXT("Acquire or craft a practice tool first."); return false; }
        FOGItemOwnerAffinityRecord Affinity;
        const auto* EquippedItem = P.Equipment.FindByPredicate([Item](const auto& E) { return E.Item.ItemId == Item->ItemId; });
        if (EquippedItem && EquippedItem->bHasAffinity) Affinity = EquippedItem->Affinity;
        else
        {
            bool Found = false;
            if (!Store.TryReadItemOwnerAffinity(Item->ItemId, R.EntityId, Found, Affinity, Error)) return false;
        }
        Affinity.ItemId = Item->ItemId; Affinity.OwnerEntityId = R.EntityId;
        if (Affinity.AffinityValue < MAX_int64) ++Affinity.AffinityValue;
        return Result(Runtime.SetAffinity(R, Affinity, Tick, Error), TEXT("Tool affinity practice recorded without a universal stat multiplier."));
    }
    case A::PracticeProficiency:
    {
        FOGEquipmentProficiencyRecord Proficiency;
        const auto* Existing = P.Proficiencies.FindByPredicate([&D](const auto& V) { return V.ProficiencyId == D.ProficiencyId; });
        if (Existing) Proficiency = *Existing;
        Proficiency.ProficiencyId = D.ProficiencyId;
        if (Proficiency.ProficiencyValue < MAX_int64) ++Proficiency.ProficiencyValue;
        return Result(Runtime.SetProficiency(R, Proficiency, Tick, Error), TEXT("Authored tool proficiency practice recorded."));
    }
    case A::WearOuterLayer:
    {
        auto Presentation = P.bHasPresentation ? P.Presentation : FOGManifestationPresentationStateRecord();
        TSharedPtr<FJsonObject> Outfit;
        if (!Object(Presentation.OutfitStateJson, Outfit, Error)) return false;
        const TArray<TSharedPtr<FJsonValue>>* Previous = nullptr;
        TArray<TSharedPtr<FJsonValue>> Layers;
        if (Outfit->TryGetArrayField(TEXT("layers"), Previous)) Layers = *Previous;
        bool Exists = false;
        for (const auto& Value : Layers)
        {
            const TSharedPtr<FJsonObject>* Layer = nullptr;
            FString Id;
            if (Value->TryGetObject(Layer) && (*Layer)->TryGetStringField(TEXT("id"), Id) && Id == D.OuterLayerId.ToString()) Exists = true;
        }
        if (!Exists)
        {
            auto Layer = MakeShared<FJsonObject>(); Layer->SetStringField(TEXT("id"), D.OuterLayerId.ToString());
            Layer->SetNumberField(TEXT("condition_bps"), 10000); Layer->SetBoolField(TEXT("visible"), true);
            Layers.Add(MakeShared<FJsonValueObject>(Layer));
        }
        Outfit->SetArrayField(TEXT("layers"), Layers); Presentation.OutfitStateJson = Json(Outfit);
        return Result(Runtime.SetPresentation(R, Presentation, Tick, Error), TEXT("Outer layer added; existing outfit, skin and hairstyle retained."));
    }
    case A::RecordInjury:
    case A::RestoreInjury:
    {
        FOGEntityId SourceEvent;
        if (!Event(SourceEvent, FName(TEXT("foundationdiag_injury_change")))) return false;
        return Action == A::RecordInjury ? Result(Runtime.SetInjury(R, FName(TEXT("training_injury")), 1000, SourceEvent, Tick, Error),
            TEXT("Minor training injury recorded with history.")) :
            Result(Runtime.RestoreInjury(R, SourceEvent, Tick, Error), TEXT("Training injury restored; history retained."));
    }
    case A::RecordNpcObservation:
    case A::RecordNpcMemory:
    case A::PromoteNpc:
    case A::SpeakToNpc:
    {
        FOGFoundationCharacterContext Npc;
        if (!EnsureResident(P, D, Tick, Npc, Error)) return false;
        Out.NpcEntityId = Npc.EntityId;
        if (Action == A::RecordNpcObservation)
        {
            FOGKnowledgeFactRecord Fact; Fact.SubjectEntityId = R.EntityId;
            Fact.FactKey = FName(TEXT("foundationdiag_observed_visitor")); Fact.SourceEntityId = Npc.EntityId;
            Fact.BeliefState = FName(TEXT("observed")); Fact.ConfidenceBps = 8000;
            Fact.bHasEvidenceWorldTick = true; Fact.EvidenceWorldTick = Tick;
            Fact.ValueJson = TEXT("{\"observation\":\"visitor_present\"}");
            return Result(Runtime.RecordKnowledge(Npc, Fact, Tick, Error), TEXT("Resident learned a local observation, with confidence and witness provenance."));
        }
        if (Action == A::RecordNpcMemory)
        {
            FOGSemanticMemoryRecord Memory;
            Memory.MemoryId = ScopedEntity(Npc.EntityId, GetTypeHash(R.EntityId) ^ 0x4D454D01u);
            Memory.SubjectEntityId = R.EntityId;
            Memory.MemoryTypeId = FOGContentId(TEXT("foundationdiag:memory.visitor"));
            Memory.SalienceBps = 6000;
            Memory.StateJson = TEXT("{\"summary\":\"remembered_local_visitor\"}");
            return Result(Runtime.RecordMemory(Npc, Memory, Tick, Error), TEXT("Resident memory consolidated into a structured record."));
        }
        if (Action == A::PromoteNpc)
            return Result(Runtime.PromoteNpc(Npc, D.NpcTierId, FOGEntityId(), TEXT("{}"), Tick, Error), TEXT("Resident promoted using the same canonical ID and retained history."));
        FOGFoundationDialogueLine Greeting;
        Greeting.LineId = FOGContentId(TEXT("foundationdiag:dialogue.greeting"));
        Greeting.Text = TEXT("Good day, visitor.");
        FOGFoundationDialogueLine Observed;
        Observed.LineId = FOGContentId(TEXT("foundationdiag:dialogue.observed"));
        Observed.Text = TEXT("I remember seeing you nearby.");
        Observed.RequiredKnowledgeKey = FName(TEXT("foundationdiag_observed_visitor"));
        Observed.KnowledgeSubjectId = R.EntityId; Observed.RequiredBeliefState = FName(TEXT("observed"));
        Observed.MinimumConfidenceBps = 8000;
        Observed.RequiredMemoryTypeId = FOGContentId(TEXT("foundationdiag:memory.visitor"));
        Observed.RequiredMemorySubjectId = R.EntityId; Observed.MinimumMemorySalienceBps = 6000;
        FOGFoundationCharacterProjection NpcP;
        if (!Runtime.Project(Npc, NpcP, Error)) return false;
        const bool Remembers = NpcP.Memories.ContainsByPredicate([&R](const auto& M)
            { return M.SubjectEntityId == R.EntityId && M.MemoryTypeId == FOGContentId(TEXT("foundationdiag:memory.visitor")) && M.SalienceBps >= 6000; });
        const bool Knows = NpcP.Knowledge.ContainsByPredicate([&R](const auto& K)
            { return K.SubjectEntityId == R.EntityId && K.FactKey == FName(TEXT("foundationdiag_observed_visitor")) && K.BeliefState == FName(TEXT("observed")) && K.ConfidenceBps >= 8000; });
        const TArray<FOGFoundationDialogueLine> Pool = Remembers && Knows ?
            TArray<FOGFoundationDialogueLine>{ Observed } : TArray<FOGFoundationDialogueLine>{ Greeting };
        if (!Runtime.SelectDialogue(Npc, Pool, 0, Out.Dialogue, Error)) return false;
        Out.Message = Out.Dialogue.Text; return true;
    }
    default: Error = TEXT("Unknown character diagnostic action."); return false;
    }
}
