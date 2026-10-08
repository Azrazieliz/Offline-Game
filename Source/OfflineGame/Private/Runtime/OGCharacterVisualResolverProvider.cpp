#include "Runtime/OGCharacterVisualResolverProvider.h"
#include <cfloat>

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FObject = TSharedPtr<FJsonObject>;
FObject Read(const FString& Json)
{
    FObject Object;
    FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Object);
    return Object;
}
FString String(const FObject& Object, const TCHAR* Key)
{
    FString Value;
    if (Object) Object->TryGetStringField(Key, Value);
    return Value;
}
FObject Child(const FObject& Object, const TCHAR* Key)
{
    const FObject* Value = nullptr;
    return Object && Object->TryGetObjectField(Key, Value) ? *Value : nullptr;
}
TSharedPtr<FJsonValue> At(const FObject& Object, const FString& Path)
{
    TArray<FString> Keys;
    Path.ParseIntoArray(Keys, TEXT("."), true);
    FObject Current = Object;
    for (int32 Index = 0; Current && Index < Keys.Num(); ++Index)
    {
        const TSharedPtr<FJsonValue> Value = Current->TryGetField(Keys[Index]);
        if (!Value.IsValid()) return nullptr;
        if (Index == Keys.Num() - 1) return Value;
        if (Value->Type != EJson::Object) return nullptr;
        Current = Value->AsObject();
    }
    return nullptr;
}
bool Equal(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    if (!A || !B || A->Type != B->Type) return false;
    switch (A->Type)
    {
    case EJson::String: return A->AsString() == B->AsString();
    case EJson::Number: return FMath::IsFinite(A->AsNumber()) && A->AsNumber() == B->AsNumber();
    case EJson::Boolean: return A->AsBool() == B->AsBool();
    case EJson::Null: return true;
    default: return false; // Use leaf predicates for extensible structured state.
    }
}
FObject ConditionSource(const FObject& Condition, const FOGFoundationCharacterProjection& P)
{
    const FString Source = String(Condition, TEXT("source"));
    if (Source == TEXT("entity")) return Read(P.EntityStateJson);
    if (Source == TEXT("progression") && P.bHasManifestation) return Read(P.Manifestation.ProgressionStateJson);
    if (Source == TEXT("rank") && P.bHasRank) return Read(P.Rank.StateJson);
    if (Source == TEXT("reinforcement") && P.bHasReinforcement) return Read(P.Reinforcement.StateJson);
    if (Source == TEXT("route"))
        for (const auto& Route : P.Routes)
            if (Route.RouteId.ToString() == String(Condition, TEXT("route_id")) &&
                Route.NodeId.ToString() == String(Condition, TEXT("node_id"))) return Read(Route.StateJson);
    if (Source == TEXT("convergence") && P.bHasManifestation)
    {
        // The projection includes Identity-wide history; another copy's result is not this body's state.
        const FString Id = String(Condition, TEXT("convergence_id")), Rule = String(Condition, TEXT("rule_id"));
        if (Id.IsEmpty() && Rule.IsEmpty()) return nullptr;
        const FOGCharacterConvergenceRecord* Selected = nullptr;
        for (const auto& Convergence : P.Convergences)
            if (Convergence.ResultManifestationId == P.Context.ManifestationId &&
                (Id.IsEmpty() || Convergence.ConvergenceId.ToString() == Id) &&
                (Rule.IsEmpty() || Convergence.RuleId.ToString() == Rule) &&
                (!Selected || Convergence.WorldTick > Selected->WorldTick)) Selected = &Convergence;
        return Selected ? Read(Selected->StateJson) : nullptr;
    }
    if (Source == TEXT("outfit") && P.bHasPresentation) return Read(P.Presentation.OutfitStateJson);
    if (Source == TEXT("variant") && P.bHasPresentation) return Read(P.Presentation.PresentationVariantStateJson);
    if (Source == TEXT("form"))
        for (const auto& Form : P.Forms)
            if (Form.FormId.ToString() == String(Condition, TEXT("form_id")) && Form.State == FName(TEXT("active")))
                return Read(Form.StateJson);
    if (Source == TEXT("factor"))
    {
        double Minimum = 0;
        Condition->TryGetNumberField(TEXT("minimum_expression_bps"), Minimum);
        if (!FMath::IsFinite(Minimum)) return nullptr;
        for (const auto& Factor : P.Factors)
            if (Factor.FactorId.ToString() == String(Condition, TEXT("factor_id")) && Factor.ExpressionWeightBps >= Minimum)
                return Read(Factor.StateJson);
    }
    if (Source.StartsWith(TEXT("equipment_")))
        for (const auto& Equipment : P.Equipment)
            if (Equipment.Binding.SlotId.ToString() == String(Condition, TEXT("slot_id")))
            {
                const FString Definition = String(Condition, TEXT("definition_id"));
                if (!Definition.IsEmpty() && Definition != Equipment.Item.DefinitionId.ToString()) continue;
                if (Source == TEXT("equipment_binding")) return Read(Equipment.Binding.StateJson);
                if (Source == TEXT("equipment_durability"))
                {
                    FObject Durability = Read(Equipment.Item.DurabilityStateJson);
                    if (!Durability) return nullptr;
                    if (!Durability->HasField(TEXT("condition_bps")))
                        Durability->SetNumberField(TEXT("condition_bps"), 10000.0);
                    if (!Durability->HasField(TEXT("broken")))
                        Durability->SetBoolField(TEXT("broken"), false);
                    return Durability;
                }
                if (Source == TEXT("equipment_evolution")) return Read(Equipment.Item.EvolutionStateJson);
            }
    return nullptr;
}
TSet<FString> ActiveForms(const FOGFoundationCharacterProjection& P)
{
    TSet<FString> Forms;
    for (const auto& Form : P.Forms)
        if (Form.State == FName(TEXT("active"))) Forms.Add(Form.FormId.ToString());
    return Forms;
}
void SetExactSelection(const FObject& Selector, const FOGFoundationCharacterProjection& P)
{
    Selector->SetStringField(TEXT("identity_id"), P.bHasManifestation ? P.Manifestation.IdentityId.ToString() : FString());
    Selector->SetStringField(TEXT("version_id"), P.bHasManifestation ? P.Manifestation.ActiveVersionId.ToString() : FString());
    Selector->SetStringField(TEXT("entity_kind"), P.EntityKind.ToString());
    TArray<TSharedPtr<FJsonValue>> Forms;
    for (const auto& Form : ActiveForms(P)) Forms.Add(MakeShared<FJsonValueString>(Form));
    Selector->SetArrayField(TEXT("form_ids"), Forms);
}
bool Matches(const FObject& Selector, const FOGFoundationCharacterProjection& P)
{
    if (!Selector) return false;
    // Empty Identity/Version explicitly select an ordinary non-Manifestation entity.
    // They never act as wildcards for a canonical roster character.
    FString Identity, Version;
    if (!Selector->TryGetStringField(TEXT("identity_id"), Identity) ||
        !Selector->TryGetStringField(TEXT("version_id"), Version) ||
        Identity != (P.bHasManifestation ? P.Manifestation.IdentityId.ToString() : FString()) ||
        Version != (P.bHasManifestation ? P.Manifestation.ActiveVersionId.ToString() : FString())) return false;
    const FString Entity = String(Selector, TEXT("entity_id"));
    if (!Entity.IsEmpty() && Entity != P.Context.EntityId.ToString()) return false;
    const FString Kind = String(Selector, TEXT("entity_kind"));
    if ((!P.bHasManifestation && Kind.IsEmpty()) || (!Kind.IsEmpty() && Kind != P.EntityKind.ToString())) return false;
    const TArray<TSharedPtr<FJsonValue>>* Forms = nullptr;
    if (!Selector->TryGetArrayField(TEXT("form_ids"), Forms)) return false;
    const TSet<FString> SelectedForms = ActiveForms(P);
    if (Forms->Num() != SelectedForms.Num()) return false;
    TSet<FString> AuthoredForms;
    for (const auto& Form : *Forms)
    {
        if (!Form || Form->Type != EJson::String || !SelectedForms.Contains(Form->AsString()) ||
            AuthoredForms.Contains(Form->AsString())) return false;
        AuthoredForms.Add(Form->AsString());
    }
    FString Skin;
    if (Selector->TryGetStringField(TEXT("skin_id"), Skin) &&
        Skin != (P.bHasPresentation ? P.Presentation.SelectedSkinId.ToString() : FString())) return false;
    const TArray<TSharedPtr<FJsonValue>>* Conditions = nullptr;
    if (Selector->HasField(TEXT("conditions")) && !Selector->TryGetArrayField(TEXT("conditions"), Conditions)) return false;
    if (Conditions) for (const auto& Value : *Conditions)
    {
        if (!Value || Value->Type != EJson::Object) return false;
        const auto Condition = Value->AsObject();
        const auto Actual = At(ConditionSource(Condition, P), String(Condition, TEXT("key")));
        if (!Actual) return false;
        bool Compared = false;
        if (const auto* Expected = Condition->Values.Find(TEXT("equals")))
        { Compared = true; if (!Equal(Actual, *Expected)) return false; }
        for (const auto* LimitKey : { TEXT("minimum"), TEXT("maximum") })
        {
            if (!Condition->HasField(LimitKey)) continue;
            double Limit = 0;
            if (!Condition->TryGetNumberField(LimitKey, Limit) || !FMath::IsFinite(Limit) ||
                Actual->Type != EJson::Number || !FMath::IsFinite(Actual->AsNumber())) return false;
            Compared = true;
            if ((FCString::Strcmp(LimitKey, TEXT("minimum")) == 0 && Actual->AsNumber() < Limit) ||
                (FCString::Strcmp(LimitKey, TEXT("maximum")) == 0 && Actual->AsNumber() > Limit)) return false;
        }
        if (!Compared) return false;
    }
    return true;
}
bool Reference(const FObject& Object, const FOGContentId& Owner, FOGOptionalAssetReference& Out)
{
    if (!Object) return false;
    const FString Package = String(Object, TEXT("package_id"));
    Out.PackageId = Package.IsEmpty() ? Owner : FOGContentId(Package);
    Out.AssetPath = FSoftObjectPath(String(Object, TEXT("asset_path")));
    return Out.PackageId.IsValid() && Out.AssetPath.IsValid();
}
bool OptionalReference(const FObject& Object, const TCHAR* Key, const FOGContentId& Owner, FOGOptionalAssetReference& Out)
{
    return !Object->HasField(Key) || Reference(Child(Object, Key), Owner, Out);
}
bool Materials(const FObject& Object, const TCHAR* Key, const FOGContentId& Owner,
    TMap<int32, FOGOptionalAssetReference>& Out)
{
    if (!Object->HasField(Key)) return true;
    const FObject Slots = Child(Object, Key);
    if (!Slots) return false;
    for (const auto& Slot : Slots->Values)
    {
        int32 Index = -1;
        FOGOptionalAssetReference Ref;
        if (!LexTryParseString(Index, *Slot.Key) || Index < 0 || !Slot.Value || Slot.Value->Type != EJson::Object ||
            !Reference(Slot.Value->AsObject(), Owner, Ref)) return false;
        Out.Add(Index, Ref);
    }
    return true;
}
bool Vector(const FObject& Object, const TCHAR* Key, FVector& Out)
{
    if (!Object->HasField(Key)) return true;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object->TryGetArrayField(Key, Values) || Values->Num() != 3) return false;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        if (!(*Values)[Index] || (*Values)[Index]->Type != EJson::Number ||
            !FMath::IsFinite((*Values)[Index]->AsNumber())) return false;
        Out[Index] = (*Values)[Index]->AsNumber();
    }
    return true;
}
bool Binding(const FObject& Object, const FOGContentId& Owner, FOGCharacterVisualBinding& Out)
{
    if (!Object) return false;
    Out.RigFamily = FName(*String(Object, TEXT("rig_family")));
    if (!OptionalReference(Object, TEXT("body"), Owner, Out.Body) ||
        !OptionalReference(Object, TEXT("privacy_body"), Owner, Out.PrivacyBody) ||
        !OptionalReference(Object, TEXT("animation_class"), Owner, Out.AnimationClass) ||
        !Materials(Object, TEXT("materials"), Owner, Out.Materials) ||
        !Materials(Object, TEXT("privacy_materials"), Owner, Out.PrivacyMaterials)) return false;
    Object->TryGetBoolField(TEXT("substitute_body_for_privacy"), Out.bSubstituteBodyForPrivacy);
    if ((Out.Body.AssetPath.IsValid() || Out.PrivacyBody.AssetPath.IsValid()) && Out.RigFamily.IsNone()) return false;
    const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
    if (Object->HasField(TEXT("parts")) && !Object->TryGetArrayField(TEXT("parts"), Parts)) return false;
    TSet<FName> Seen;
    if (Parts) for (const auto& Value : *Parts)
    {
        if (!Value || Value->Type != EJson::Object) return false;
        const auto PartObject = Value->AsObject();
        FOGCharacterVisualPart Part;
        Part.PartId = FName(*String(PartObject, TEXT("part_id")));
        Part.RigFamily = FName(*String(PartObject, TEXT("rig_family")));
        Part.SocketName = FName(*String(PartObject, TEXT("socket")));
        if (Part.PartId.IsNone() || Seen.Contains(Part.PartId) ||
            !OptionalReference(PartObject, TEXT("asset"), Owner, Part.Asset) ||
            !OptionalReference(PartObject, TEXT("privacy_asset"), Owner, Part.PrivacyAsset)) return false;
        Seen.Add(Part.PartId);
        PartObject->TryGetBoolField(TEXT("substitute_for_privacy"), Part.bSubstituteForPrivacy);
        PartObject->TryGetBoolField(TEXT("use_leader_pose"), Part.bUseLeaderPose);
        FVector Translation = FVector::ZeroVector, Rotation = FVector::ZeroVector, Scale = FVector::OneVector;
        if (!Vector(PartObject, TEXT("translation"), Translation) ||
            !Vector(PartObject, TEXT("rotation"), Rotation) || !Vector(PartObject, TEXT("scale"), Scale)) return false;
        Part.RelativeTransform = FTransform(FRotator(Rotation.X, Rotation.Y, Rotation.Z), Translation, Scale);
        Out.Parts.Add(MoveTemp(Part));
    }
    return true;
}
}

bool FOGCharacterVisualResolverProvider::Resolve(const IOGWorldStore& Store,
    const FOGFoundationCharacterProjection& Projection, bool bPrivacy,
    FOGCharacterVisualBinding& Out, FString& Reason)
{
    Out = FOGCharacterVisualBinding();
    Reason.Reset();
    // Privacy references stay in the same binding; the presenter applies them without changing selection/state.
    (void)bPrivacy;
    TArray<FOGContentPackageRecord> Packages;
    if (!Store.ListContentPackageRecords(Packages, Reason)) return false;
    FObject Selected;
    FOGContentId Owner;
    double BestPriority = -DBL_MAX;
    bool Ambiguous = false, Found = false;
    for (const auto& Package : Packages)
    {
        if (!Package.bValidated) continue;
        const auto Manifest = Read(Package.ManifestJson);
        const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
        if (!Manifest || !Manifest->TryGetArrayField(TEXT("character_visual_bindings"), Rows)) continue;
        for (const auto& Value : *Rows)
        {
            if (!Value || Value->Type != EJson::Object) continue;
            const auto Row = Value->AsObject();
            if (!Matches(Child(Row, TEXT("selector")), Projection)) continue;
            double Priority = 0;
            if (Row->HasField(TEXT("priority")) && !Row->TryGetNumberField(TEXT("priority"), Priority)) continue;
            if (!FMath::IsFinite(Priority) || Priority < BestPriority) continue;
            if (Found && Priority == BestPriority) { Ambiguous = true; continue; }
            Found = true;
            Selected = Child(Row, TEXT("binding"));
            Owner = Package.PackageId;
            BestPriority = Priority;
            Ambiguous = false;
        }
    }
    if (Ambiguous) { Reason = TEXT("Ambiguous exact canonical visual bindings; current body retained."); return false; }
    if (!Found) { Reason = TEXT("No exact Identity/Version/active-form visual metadata; current body retained."); return false; }
    FOGCharacterVisualBinding Result;
    if (!Binding(Selected, Owner, Result))
    { Reason = TEXT("Malformed exact visual metadata; current body retained."); return false; }
    Out = MoveTemp(Result);
    return true;
}

bool FOGCharacterVisualResolverProvider::MakeDiagnosticRow(const FOGFoundationCharacterProjection& P,
    const FString& AuthoredTemplate, FString& OutRow, FString& Reason)
{
    const auto Row = Read(AuthoredTemplate);
    if (!Row || String(Row, TEXT("binding_id")).IsEmpty() || !Child(Row, TEXT("binding")))
    { Reason = TEXT("Diagnostic template requires binding_id and authored binding."); return false; }
    const auto Selector = MakeShared<FJsonObject>();
    SetExactSelection(Selector, P);
    Row->SetObjectField(TEXT("selector"), Selector);
    Row->SetStringField(TEXT("binding_id"), String(Row, TEXT("binding_id")) + TEXT("|") + P.Context.EntityId.ToString());
    // Diagnostic examples intentionally address the actual diagnostic individual, not other roster instances.
    Selector->SetStringField(TEXT("entity_id"), P.Context.EntityId.ToString());
    if (!Matches(Selector, P)) { Reason = TEXT("Diagnostic selection does not match canonical projection."); return false; }
    OutRow.Reset();
    FJsonSerializer::Serialize(Row.ToSharedRef(), TJsonWriterFactory<>::Create(&OutRow));
    Reason.Reset();
    return true;
}

bool FOGCharacterVisualResolverProvider::MergeAuthoredMetadata(FOGContentPackageRecord& Package,
    const FString& RowJson, FString& Reason)
{
    const auto Manifest = Read(Package.ManifestJson), Row = Read(RowJson);
    const FString Id = String(Row, TEXT("binding_id"));
    if (!Manifest || !Row || Id.IsEmpty() || !Child(Row, TEXT("selector")) || !Child(Row, TEXT("binding")))
    { Reason = TEXT("Authored visual metadata must be an object with binding_id, selector and binding."); return false; }
    TArray<TSharedPtr<FJsonValue>> Rows;
    const TArray<TSharedPtr<FJsonValue>>* Existing = nullptr;
    if (Manifest->HasField(TEXT("character_visual_bindings")))
    {
        if (!Manifest->TryGetArrayField(TEXT("character_visual_bindings"), Existing))
        { Reason = TEXT("Existing character visual metadata malformed; preserving manifest."); return false; }
        Rows = *Existing;
    }
    for (const auto& Value : Rows)
        if (Value && Value->Type == EJson::Object && String(Value->AsObject(), TEXT("binding_id")) == Id)
        { Reason.Reset(); return true; } // Never replace authored rows during diagnostic reseeding.
    Rows.Add(MakeShared<FJsonValueObject>(Row));
    Manifest->SetArrayField(TEXT("character_visual_bindings"), Rows);
    FString Json;
    FJsonSerializer::Serialize(Manifest.ToSharedRef(), TJsonWriterFactory<>::Create(&Json));
    Package.ManifestJson = MoveTemp(Json);
    Reason.Reset();
    return true;
}
