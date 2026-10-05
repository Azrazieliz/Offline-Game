#include "UI/OGUiViewModelService.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
int32 ContentOrderIndex(
    const TArray<FOGContentId>& Order,
    const FOGContentId& Value)
{
    const int32 Found =
        Order.IndexOfByPredicate(
            [&Value](const FOGContentId& Candidate)
            {
                return Candidate == Value;
            });
    return Found == INDEX_NONE
        ? MAX_int32
        : Found;
}

int32 NameOrderIndex(
    const TArray<FName>& Order,
    FName Value)
{
    const int32 Found =
        Order.IndexOfByKey(Value);
    return Found == INDEX_NONE
        ? MAX_int32
        : Found;
}

bool MatchesSearch(
    const FString& Needle,
    const FString& Value)
{
    return Needle.IsEmpty() ||
        Value.Contains(
            Needle,
            ESearchCase::IgnoreCase);
}

FName ProjectStatusName(EOGProjectStatus Status)
{
    switch (Status)
    {
        case EOGProjectStatus::Planned: return FName(TEXT("planned"));
        case EOGProjectStatus::Active: return FName(TEXT("active"));
        case EOGProjectStatus::Completed: return FName(TEXT("completed"));
        case EOGProjectStatus::Failed: return FName(TEXT("failed"));
        case EOGProjectStatus::Cancelled: return FName(TEXT("cancelled"));
        default: return NAME_None;
    }
}

FName DispatchStatusName(EOGDispatchStatus Status)
{
    switch (Status)
    {
        case EOGDispatchStatus::Planned: return FName(TEXT("planned"));
        case EOGDispatchStatus::Active: return FName(TEXT("active"));
        case EOGDispatchStatus::Succeeded: return FName(TEXT("succeeded"));
        case EOGDispatchStatus::Failed: return FName(TEXT("failed"));
        case EOGDispatchStatus::Cancelled: return FName(TEXT("cancelled"));
        default: return NAME_None;
    }
}

FName WarStatusName(EOGWarStatus Status)
{
    switch (Status)
    {
        case EOGWarStatus::Active: return FName(TEXT("active"));
        case EOGWarStatus::Resolved: return FName(TEXT("resolved"));
        case EOGWarStatus::Withdrawn: return FName(TEXT("withdrawn"));
        case EOGWarStatus::Collapsed: return FName(TEXT("collapsed"));
        default: return NAME_None;
    }
}

bool BuildEquipmentRows(
    IOGWorldStore& Store,
    const FOGEntityId& WearerId,
    TArray<FOGEquipmentSlotViewModel>& OutRows,
    FString& OutError)
{
    OutRows.Reset();

    TArray<FOGEquipmentBindingRecord> Bindings;
    if (!Store.ListEquipmentBindings(
            WearerId,
            Bindings,
            OutError))
    {
        return false;
    }

    for (const FOGEquipmentBindingRecord& Binding :
         Bindings)
    {
        bool bItemFound = false;
        FOGItemInstanceRecord Item;
        if (!Store.TryReadItemInstance(
                Binding.ItemId,
                bItemFound,
                Item,
                OutError))
        {
            return false;
        }

        if (!bItemFound)
        {
            OutError =
                TEXT("Equipment binding references a missing item instance.");
            OutRows.Reset();
            return false;
        }

        FOGEquipmentSlotViewModel Row;
        Row.SlotId =
            Binding.SlotId;
        Row.ItemId =
            Item.ItemId;
        Row.DefinitionId =
            Item.DefinitionId;
        Row.ItemRankId =
            Item.CurrentRankId;
        Row.QualityId =
            Item.QualityId;
        Row.EvolutionStateJson =
            Item.EvolutionStateJson;

        bool bAffinityFound = false;
        FOGItemOwnerAffinityRecord Affinity;
        if (!Store.TryReadItemOwnerAffinity(
                Item.ItemId,
                WearerId,
                bAffinityFound,
                Affinity,
                OutError))
        {
            return false;
        }

        if (bAffinityFound)
        {
            Row.bAffinityKnown =
                true;
            Row.AffinityMilestoneId =
                Affinity.MilestoneId;
        }

        OutRows.Add(
            MoveTemp(Row));
    }

    OutRows.Sort(
        [](const FOGEquipmentSlotViewModel& A,
           const FOGEquipmentSlotViewModel& B)
        {
            return A.SlotId.ToString().Compare(
                B.SlotId.ToString(),
                ESearchCase::CaseSensitive) < 0;
        });

    return true;
}

FName ExtractChronicleImportance(
    const FString& PayloadJson)
{
    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(
            PayloadJson);

    if (!FJsonSerializer::Deserialize(
            Reader,
            Json) ||
        !Json.IsValid())
    {
        return FName(TEXT("significant"));
    }

    FString Importance;
    if (Json->TryGetStringField(
            TEXT("importance"),
            Importance) &&
        !Importance.IsEmpty())
    {
        return FName(*Importance);
    }

    return FName(TEXT("significant"));
}
}

bool FOGUiViewModelService::BuildRosterWithQuery(
    const FOGEntityId& RulerId,
    const TArray<FOGCharacterIdentityDefinition>& IdentityDefinitions,
    const FOGRosterQuery& Query,
    TArray<FOGRosterIdentityViewModel>& OutRoster,
    FString& OutError) const
{
    OutRoster.Reset();
    OutError.Reset();

    if (Query.Columns != 2 &&
        Query.Columns != 3)
    {
        OutError =
            TEXT("Roster density must use the frozen 2- or 3-column choices.");
        return false;
    }

    TArray<FOGRosterIdentityViewModel> BaseRoster;
    if (!BuildRoster(
            RulerId,
            BaseRoster,
            OutError))
    {
        return false;
    }

    TMap<FOGContentId, FString> DisplayNames;
    for (const FOGCharacterIdentityDefinition& Identity :
         IdentityDefinitions)
    {
        if (Identity.IdentityId.IsValid())
        {
            DisplayNames.Add(
                Identity.IdentityId,
                Identity.DisplayNameKey);
        }
    }

    for (FOGRosterIdentityViewModel& IdentityView :
         BaseRoster)
    {
        if (const FString* DisplayName =
                DisplayNames.Find(
                    IdentityView.IdentityId))
        {
            IdentityView.DisplayNameKey =
                *DisplayName;
        }

        bool bSearchMatch =
            MatchesSearch(
                Query.SearchText,
                IdentityView.IdentityId.ToString()) ||
            MatchesSearch(
                Query.SearchText,
                IdentityView.DisplayNameKey);

        if (!bSearchMatch)
        {
            for (const FOGRosterManifestationViewModel& Manifestation :
                 IdentityView.Manifestations)
            {
                if (MatchesSearch(
                        Query.SearchText,
                        Manifestation.BuildLabel))
                {
                    bSearchMatch =
                        true;
                    break;
                }
            }
        }

        if (!bSearchMatch)
        {
            continue;
        }

        bool bAnyManifestationMatches =
            false;

        for (const FOGRosterManifestationViewModel& Manifestation :
             IdentityView.Manifestations)
        {
            if (!Query.RankIds.IsEmpty() &&
                !Query.RankIds.ContainsByPredicate(
                    [&Manifestation](const FOGContentId& RankId)
                    {
                        return RankId ==
                            Manifestation.RankId;
                    }))
            {
                continue;
            }

            if (!Query.Rarities.IsEmpty() &&
                !Query.Rarities.Contains(
                    Manifestation.CurrentRarity))
            {
                continue;
            }

            if (!Query.WorldFantasmGradeIds.IsEmpty() &&
                !Query.WorldFantasmGradeIds.ContainsByPredicate(
                    [&Manifestation](const FOGContentId& GradeId)
                    {
                        return GradeId ==
                            Manifestation.WorldFantasmGradeId;
                    }))
            {
                continue;
            }

            if (!Query.ClassIds.IsEmpty())
            {
                TArray<FOGEntityClassRecord> Classes;
                if (!Store.ListEntityClasses(
                        Manifestation.ManifestationId,
                        Classes,
                        OutError))
                {
                    OutRoster.Reset();
                    return false;
                }

                const bool bClassMatch =
                    Classes.ContainsByPredicate(
                        [&Query](const FOGEntityClassRecord& Class)
                        {
                            return Query.ClassIds.ContainsByPredicate(
                                [&Class](const FOGContentId& Required)
                                {
                                    return Required ==
                                        Class.ClassId;
                                });
                        });

                if (!bClassMatch)
                {
                    continue;
                }
            }

            bAnyManifestationMatches =
                true;
            break;
        }

        const bool bHasAnyFilter =
            !Query.RankIds.IsEmpty() ||
            !Query.ClassIds.IsEmpty() ||
            !Query.Rarities.IsEmpty() ||
            !Query.WorldFantasmGradeIds.IsEmpty();

        if (bHasAnyFilter &&
            !bAnyManifestationMatches)
        {
            continue;
        }

        OutRoster.Add(
            MoveTemp(IdentityView));
    }

    auto SelectedFor =
        [](const FOGRosterIdentityViewModel& Identity)
            -> const FOGRosterManifestationViewModel*
        {
            return Identity.Manifestations.FindByPredicate(
                [&Identity](const FOGRosterManifestationViewModel& Manifestation)
                {
                    return Manifestation.ManifestationId ==
                        Identity.SelectedManifestationId;
                });
        };

    OutRoster.Sort(
        [&Query, &SelectedFor](
            const FOGRosterIdentityViewModel& A,
            const FOGRosterIdentityViewModel& B)
        {
            const FOGRosterManifestationViewModel* AM =
                SelectedFor(A);
            const FOGRosterManifestationViewModel* BM =
                SelectedFor(B);

            int32 Compare = 0;
            switch (Query.SortDimension)
            {
                case EOGRosterSortDimension::Rank:
                {
                    const int32 AI =
                        AM
                            ? ContentOrderIndex(
                                Query.RankOrder,
                                AM->RankId)
                            : MAX_int32;
                    const int32 BI =
                        BM
                            ? ContentOrderIndex(
                                Query.RankOrder,
                                BM->RankId)
                            : MAX_int32;
                    Compare =
                        AI == BI
                            ? 0
                            : (AI < BI ? -1 : 1);
                    break;
                }
                case EOGRosterSortDimension::Rarity:
                {
                    const int32 AI =
                        AM
                            ? NameOrderIndex(
                                Query.RarityOrder,
                                AM->CurrentRarity)
                            : MAX_int32;
                    const int32 BI =
                        BM
                            ? NameOrderIndex(
                                Query.RarityOrder,
                                BM->CurrentRarity)
                            : MAX_int32;
                    Compare =
                        AI == BI
                            ? 0
                            : (AI < BI ? -1 : 1);
                    break;
                }
                case EOGRosterSortDimension::WorldFantasm:
                {
                    const int32 AI =
                        AM
                            ? ContentOrderIndex(
                                Query.WorldFantasmOrder,
                                AM->WorldFantasmGradeId)
                            : MAX_int32;
                    const int32 BI =
                        BM
                            ? ContentOrderIndex(
                                Query.WorldFantasmOrder,
                                BM->WorldFantasmGradeId)
                            : MAX_int32;
                    Compare =
                        AI == BI
                            ? 0
                            : (AI < BI ? -1 : 1);
                    break;
                }
                case EOGRosterSortDimension::Class:
                {
                    // Classes are content-defined and may coexist. Without an
                    // authored class ordering, stable Identity ordering is the
                    // only non-fabricated fallback.
                    Compare =
                        0;
                    break;
                }
                case EOGRosterSortDimension::Identity:
                default:
                    break;
            }

            if (Compare == 0)
            {
                const FString AText =
                    A.DisplayNameKey.IsEmpty()
                        ? A.IdentityId.ToString()
                        : A.DisplayNameKey;
                const FString BText =
                    B.DisplayNameKey.IsEmpty()
                        ? B.IdentityId.ToString()
                        : B.DisplayNameKey;
                Compare =
                    AText.Compare(
                        BText,
                        ESearchCase::IgnoreCase);
            }

            return Query.bDescending
                ? Compare > 0
                : Compare < 0;
        });

    return true;
}

bool FOGUiViewModelService::BuildManifestationDetail(
    const FOGEntityId& RulerId,
    const FOGEntityId& ManifestationId,
    const FString& SkillSearchText,
    int32 MaxVisibleSkills,
    const FOGResolvedStatsProjectionResolver& StatsResolver,
    FOGManifestationDetailViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGManifestationDetailViewModel();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        !ManifestationId.IsValid() ||
        MaxVisibleSkills < 0)
    {
        OutError =
            TEXT("Manifestation-detail projection request is invalid.");
        return false;
    }

    bool bFound = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Store.TryReadCharacterManifestation(
            ManifestationId,
            bFound,
            Manifestation,
            OutError))
    {
        return false;
    }

    if (!bFound ||
        Manifestation.OwningRulerId !=
            RulerId)
    {
        OutError =
            TEXT("Manifestation detail requires an owned Manifestation.");
        return false;
    }

    OutViewModel.ManifestationId =
        ManifestationId;
    OutViewModel.IdentityId =
        Manifestation.IdentityId;
    OutViewModel.BuildLabel =
        Manifestation.BuildLabel;
    OutViewModel.WorldModeAnchorTerritoryId =
        Manifestation.WorldModeAnchorTerritoryId;
    OutViewModel.DeploymentState =
        Manifestation.WorldModeAnchorTerritoryId.IsValid()
            ? FName(TEXT("anchored"))
            : FName(TEXT("unanchored"));
    OutViewModel.Level =
        Manifestation.Level;
    OutViewModel.CurrentRarity =
        Manifestation.CurrentRarity;

    FOGEntityRankStateRecord Rank;
    if (!Store.TryReadEntityRankState(
            ManifestationId,
            bFound,
            Rank,
            OutError))
    {
        return false;
    }
    if (bFound)
    {
        OutViewModel.RankId =
            Rank.EffectiveRankId.IsValid()
                ? Rank.EffectiveRankId
                : Rank.AttainedRankId;
    }

    FOGManifestationWorldFantasmStateRecord Fantasm;
    if (!Store.TryReadManifestationWorldFantasmState(
            ManifestationId,
            bFound,
            Fantasm,
            OutError))
    {
        return false;
    }
    if (bFound)
    {
        OutViewModel.WorldFantasmGradeId =
            Fantasm.GradeId;
    }

    TArray<FOGEntityClassRecord> Classes;
    if (!Store.ListEntityClasses(
            ManifestationId,
            Classes,
            OutError))
    {
        return false;
    }
    for (const FOGEntityClassRecord& Class :
         Classes)
    {
        OutViewModel.ClassIds.Add(
            Class.ClassId);
    }

    TArray<FOGEntitySkillRecord> Skills;
    if (!Store.ListEntitySkills(
            ManifestationId,
            Skills,
            OutError))
    {
        return false;
    }
    OutViewModel.TotalLearnedSkillCount =
        Skills.Num();

    const int32 VisibleLimit =
        MaxVisibleSkills == 0
            ? 64
            : MaxVisibleSkills;

    for (const FOGEntitySkillRecord& Skill :
         Skills)
    {
        if (!MatchesSearch(
                SkillSearchText,
                Skill.SkillId.ToString()))
        {
            continue;
        }

        if (OutViewModel.Skills.Num() >=
            VisibleLimit)
        {
            break;
        }

        FOGSkillViewModel View;
        View.SkillId =
            Skill.SkillId;
        View.CurrentState =
            Skill.CurrentState;
        View.LearnedWorldTick =
            Skill.LearnedWorldTick;

        TArray<FOGSkillProvenanceRecord> Provenance;
        if (!Store.ListSkillProvenance(
                ManifestationId,
                Skill.SkillId,
                Provenance,
                OutError))
        {
            return false;
        }

        for (const FOGSkillProvenanceRecord& Source :
             Provenance)
        {
            if (!Source.SourceKind.IsNone() &&
                !View.SourceKinds.Contains(
                    Source.SourceKind))
            {
                View.SourceKinds.Add(
                    Source.SourceKind);
            }
        }

        OutViewModel.Skills.Add(
            MoveTemp(View));
    }

    TArray<FOGManifestationRouteNodeRecord> Routes;
    if (!Store.ListManifestationRouteNodes(
            ManifestationId,
            Routes,
            OutError))
    {
        return false;
    }

    for (const FOGManifestationRouteNodeRecord& Route :
         Routes)
    {
        FOGRouteNodeViewModel View;
        View.RouteId =
            Route.RouteId;
        View.NodeId =
            Route.NodeId;
        View.State =
            Route.State;
        View.bEntered =
            Route.bHasEnteredWorldTick;
        View.bCompleted =
            Route.bHasCompletedWorldTick;
        OutViewModel.RouteNodes.Add(
            MoveTemp(View));
    }

    TArray<FOGManifestationFormRecord> Forms;
    if (!Store.ListManifestationForms(
            ManifestationId,
            Forms,
            OutError))
    {
        return false;
    }
    for (const FOGManifestationFormRecord& Form :
         Forms)
    {
        FOGFormViewModel View;
        View.FormId =
            Form.FormId;
        View.State =
            Form.State;
        View.UnlockedWorldTick =
            Form.UnlockedWorldTick;
        OutViewModel.Forms.Add(
            MoveTemp(View));
    }

    if (!BuildEquipmentRows(
            Store,
            ManifestationId,
            OutViewModel.Equipment,
            OutError))
    {
        return false;
    }

    TArray<FOGEquipmentProficiencyRecord> Proficiencies;
    if (!Store.ListEquipmentProficienciesByOwner(
            ManifestationId,
            Proficiencies,
            OutError))
    {
        return false;
    }
    for (const FOGEquipmentProficiencyRecord& Proficiency :
         Proficiencies)
    {
        FOGEquipmentProficiencySummaryViewModel View;
        View.ProficiencyId =
            Proficiency.ProficiencyId;
        View.GradeId =
            Proficiency.GradeId;
        // Grade-first is the frozen default. Numeric values remain available
        // to a detailed inspection surface without becoming mandatory.
        View.bNumericDetailAvailable =
            true;
        View.ProficiencyValue =
            Proficiency.ProficiencyValue;
        OutViewModel.Proficiencies.Add(
            MoveTemp(View));
    }

    FOGManifestationPresentationStateRecord Presentation;
    if (!Store.TryReadManifestationPresentationState(
            ManifestationId,
            bFound,
            Presentation,
            OutError))
    {
        return false;
    }
    if (bFound)
    {
        OutViewModel.SelectedSkinId =
            Presentation.SelectedSkinId;
        OutViewModel.OutfitStateJson =
            Presentation.OutfitStateJson;
    }

    if (StatsResolver)
    {
        if (!StatsResolver(
                ManifestationId,
                OutViewModel.ResolvedStatsJson,
                OutError))
        {
            return false;
        }
        if (OutViewModel.ResolvedStatsJson.IsEmpty())
        {
            OutViewModel.ResolvedStatsJson =
                TEXT("{}");
        }
    }

    return true;
}

bool FOGUiViewModelService::BuildManifestationComparison(
    const FOGEntityId& RulerId,
    const FOGEntityId& LeftManifestationId,
    const FOGEntityId& RightManifestationId,
    const FOGResolvedStatsProjectionResolver& StatsResolver,
    FOGManifestationComparisonViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGManifestationComparisonViewModel();
    OutError.Reset();

    if (!LeftManifestationId.IsValid() ||
        !RightManifestationId.IsValid() ||
        LeftManifestationId ==
            RightManifestationId)
    {
        OutError =
            TEXT("Manifestation comparison requires two distinct Manifestations.");
        return false;
    }

    if (!BuildManifestationDetail(
            RulerId,
            LeftManifestationId,
            FString(),
            64,
            StatsResolver,
            OutViewModel.Left,
            OutError) ||
        !BuildManifestationDetail(
            RulerId,
            RightManifestationId,
            FString(),
            64,
            StatsResolver,
            OutViewModel.Right,
            OutError))
    {
        return false;
    }

    if (OutViewModel.Left.IdentityId !=
        OutViewModel.Right.IdentityId)
    {
        OutError =
            TEXT("Character Identity comparison surface compares Manifestations of the same Identity.");
        OutViewModel =
            FOGManifestationComparisonViewModel();
        return false;
    }

    OutViewModel.IdentityId =
        OutViewModel.Left.IdentityId;
    return true;
}

bool FOGUiViewModelService::BuildWardrobe(
    const FOGEntityId& RulerId,
    const FOGEntityId& ManifestationId,
    const FOGPresentationCompatibilityResolver& CompatibilityResolver,
    FOGWardrobeViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGWardrobeViewModel();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        !ManifestationId.IsValid() ||
        !CompatibilityResolver)
    {
        OutError =
            TEXT("Wardrobe projection requires owned Manifestation and compatibility resolver.");
        return false;
    }

    bool bFound = false;
    FOGCharacterManifestationRecord Manifestation;
    if (!Store.TryReadCharacterManifestation(
            ManifestationId,
            bFound,
            Manifestation,
            OutError))
    {
        return false;
    }
    if (!bFound ||
        Manifestation.OwningRulerId !=
            RulerId)
    {
        OutError =
            TEXT("Wardrobe projection references a non-owned Manifestation.");
        return false;
    }

    OutViewModel.ManifestationId =
        ManifestationId;
    OutViewModel.ArchiveReplayFilters =
    {
        FName(TEXT("participant")),
        FName(TEXT("version_form")),
        FName(TEXT("outfit_skin")),
        FName(TEXT("location"))
    };

    FOGManifestationPresentationStateRecord Presentation;
    if (!Store.TryReadManifestationPresentationState(
            ManifestationId,
            bFound,
            Presentation,
            OutError))
    {
        return false;
    }
    if (bFound)
    {
        OutViewModel.SelectedSkinId =
            Presentation.SelectedSkinId;
        OutViewModel.OutfitStateJson =
            Presentation.OutfitStateJson;
    }

    TArray<FOGOwnedPresentationUnlockRecord> Unlocks;
    if (!Store.ListOwnedPresentationUnlocks(
            RulerId,
            Unlocks,
            OutError))
    {
        return false;
    }

    for (const FOGOwnedPresentationUnlockRecord& Unlock :
         Unlocks)
    {
        bool bCompatible = false;
        if (!CompatibilityResolver(
                ManifestationId,
                Unlock.PresentationId,
                bCompatible,
                OutError))
        {
            return false;
        }

        FOGWardrobeUnlockViewModel View;
        View.PresentationId =
            Unlock.PresentationId;
        View.bCompatible =
            bCompatible;
        View.State =
            Unlock.State;
        OutViewModel.OwnedPresentations.Add(
            MoveTemp(View));
    }

    return BuildEquipmentRows(
        Store,
        ManifestationId,
        OutViewModel.VisibleEquipment,
        OutError);
}

bool FOGUiViewModelService::BuildAdultUtility(
    const FOGCharacterIdentityDefinition& Identity,
    const FOGEntityId& CharacterEntityId,
    bool bSfwPresentationEnabled,
    const TArray<FOGContentId>& AvailableSystemicInteractionIds,
    FOGAdultUtilityViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGAdultUtilityViewModel();
    OutError.Reset();

    if (!Identity.IdentityId.IsValid() ||
        !CharacterEntityId.IsValid())
    {
        OutError =
            TEXT("Adult utility projection requires valid Identity and character entity.");
        return false;
    }

    OutViewModel.bSfwPresentationEnabled =
        bSfwPresentationEnabled;

    if (Identity.CanonicalMaturity !=
        EOGCanonicalMaturity::Adult)
    {
        return true;
    }

    OutViewModel.bVisible =
        true;
    OutViewModel.Sections =
    {
        FName(TEXT("profile")),
        FName(TEXT("direct_interaction")),
        FName(TEXT("systemic_interactions")),
        FName(TEXT("archive_replay"))
    };
    OutViewModel.ReplayFilters =
    {
        FName(TEXT("participant")),
        FName(TEXT("version_form")),
        FName(TEXT("outfit_skin")),
        FName(TEXT("location"))
    };

    for (const FOGContentId& InteractionId :
         AvailableSystemicInteractionIds)
    {
        if (!InteractionId.IsValid())
        {
            OutError =
                TEXT("Adult utility received an invalid authored systemic interaction ID.");
            return false;
        }
        OutViewModel.AvailableSystemicInteractionIds.Add(
            InteractionId);
    }

    bool bRuntimeFound = false;
    FOGCharacterAdultRuntimeStateRecord Runtime;
    if (!Store.TryReadCharacterAdultRuntimeState(
            CharacterEntityId,
            bRuntimeFound,
            Runtime,
            OutError))
    {
        return false;
    }
    if (bRuntimeFound)
    {
        OutViewModel.CurrentProfileVariantId =
            Runtime.CurrentProfileVariantId;
        OutViewModel.MutableContextStateJson =
            Runtime.MutableContextStateJson;
    }

    TArray<FOGWorldEvent> Events;
    if (!Store.ListWorldEvents(
            CharacterEntityId,
            NAME_None,
            true,
            250,
            Events,
            OutError))
    {
        return false;
    }

    for (const FOGWorldEvent& Event :
         Events)
    {
        FOGChronicleEntryViewModel View;
        View.EventId =
            Event.EventId;
        View.EventType =
            Event.EventType;
        View.WorldTick =
            Event.WorldTick;
        View.PrimaryEntityId =
            Event.PrimaryEntity;
        View.RelatedEntityIds =
            Event.RelatedEntities;
        View.PayloadJson =
            Event.PayloadJson;
        View.Importance =
            ExtractChronicleImportance(
                Event.PayloadJson);
        OutViewModel.ArchiveEvents.Add(
            MoveTemp(View));
    }

    return true;
}

FOGCraftingViewModel FOGUiViewModelService::BuildCraftingShell()
{
    FOGCraftingViewModel View;
    View.Modes =
    {
        FName(TEXT("known")),
        FName(TEXT("experiment"))
    };
    View.ExperimentFields =
    {
        FName(TEXT("materials")),
        FName(TEXT("facility_technique")),
        FName(TEXT("intended_function")),
        FName(TEXT("appearance")),
        FName(TEXT("desired_properties"))
    };
    return View;
}

bool FOGUiViewModelService::BuildEquipmentComparison(
    const FOGEntityId& CurrentItemId,
    const FOGEntityId& CandidateItemId,
    const FString& ResolvedStatDeltaJson,
    const TArray<FOGContentId>& GainedSkillIds,
    const TArray<FOGContentId>& LostSkillIds,
    const TArray<FString>& CompatibilityWarnings,
    const FString& AffinityImplicationTextKey,
    const FString& ProficiencyImplicationTextKey,
    FOGEquipmentComparisonViewModel& OutViewModel,
    FString& OutError)
{
    OutViewModel =
        FOGEquipmentComparisonViewModel();
    OutError.Reset();

    if (!CandidateItemId.IsValid() ||
        (CurrentItemId.IsValid() &&
         CurrentItemId ==
            CandidateItemId))
    {
        OutError =
            TEXT("Equipment comparison requires a distinct valid candidate item.");
        return false;
    }

    for (const FOGContentId& SkillId :
         GainedSkillIds)
    {
        if (!SkillId.IsValid())
        {
            OutError =
                TEXT("Equipment comparison contains an invalid gained-skill ID.");
            return false;
        }
    }
    for (const FOGContentId& SkillId :
         LostSkillIds)
    {
        if (!SkillId.IsValid())
        {
            OutError =
                TEXT("Equipment comparison contains an invalid lost-skill ID.");
            return false;
        }
    }

    OutViewModel.CurrentItemId =
        CurrentItemId;
    OutViewModel.CandidateItemId =
        CandidateItemId;
    OutViewModel.ResolvedStatDeltaJson =
        ResolvedStatDeltaJson.IsEmpty()
            ? TEXT("{}")
            : ResolvedStatDeltaJson;
    OutViewModel.GainedSkillIds =
        GainedSkillIds;
    OutViewModel.LostSkillIds =
        LostSkillIds;
    OutViewModel.CompatibilityWarnings =
        CompatibilityWarnings;
    OutViewModel.AffinityImplicationTextKey =
        AffinityImplicationTextKey;
    OutViewModel.ProficiencyImplicationTextKey =
        ProficiencyImplicationTextKey;
    return true;
}

bool FOGUiViewModelService::BuildGachaDetails(
    const FOGGachaBannerDefinition& Banner,
    const TArray<FOGContentId>& AuthoredDesignationOptions,
    const FString& SpecialRulesJson,
    FOGGachaDetailsViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGGachaDetailsViewModel();
    OutError.Reset();

    if (!FOGGachaService::ValidateBanner(
            Banner,
            OutError))
    {
        return false;
    }

    OutViewModel.CarryCategory =
        Banner.PityCategory;
    OutViewModel.SoftPityStart =
        Banner.SoftPityStart;
    OutViewModel.SoftPityBonusPerPullBps =
        Banner.SoftPityBonusPerPullBps;
    OutViewModel.HardPity =
        Banner.HardPity;
    OutViewModel.bFeaturedGuaranteeAfterMiss =
        Banner.bFeaturedGuaranteeAfterMiss;
    OutViewModel.CompatibleTicketIds =
        Banner.CompatibleTicketIds;
    OutViewModel.SpecialRulesJson =
        SpecialRulesJson.IsEmpty()
            ? TEXT("{}")
            : SpecialRulesJson;

    int64 WeightTotal = 0;
    for (const FOGGachaPoolEntry& Entry :
         Banner.Entries)
    {
        WeightTotal +=
            Entry.Weight;
    }

    for (const FOGGachaPoolEntry& Entry :
         Banner.Entries)
    {
        FOGGachaProbabilityViewModel Probability;
        Probability.IdentityId =
            Entry.IdentityId;
        Probability.VersionId =
            Entry.VersionId;
        Probability.Rarity =
            Entry.Rarity;
        Probability.bFeatured =
            Entry.bFeatured;
        Probability.BaseProbabilityBps =
            WeightTotal > 0
                ? static_cast<int32>(
                    static_cast<int64>(
                        Entry.Weight) *
                    10000 /
                    WeightTotal)
                : 0;
        OutViewModel.DeclaredProbabilities.Add(
            MoveTemp(Probability));
    }

    for (const FOGContentId& IdentityId :
         AuthoredDesignationOptions)
    {
        if (!IdentityId.IsValid() ||
            !Banner.Entries.ContainsByPredicate(
                [&IdentityId](const FOGGachaPoolEntry& Entry)
                {
                    return Entry.IdentityId ==
                        IdentityId;
                }))
        {
            OutError =
                TEXT("Gacha designation option must reference an eligible banner Identity.");
            OutViewModel =
                FOGGachaDetailsViewModel();
            return false;
        }

        if (!OutViewModel.AuthoredDesignationOptions.ContainsByPredicate(
                [&IdentityId](const FOGContentId& Existing)
                {
                    return Existing ==
                        IdentityId;
                }))
        {
            OutViewModel.AuthoredDesignationOptions.Add(
                IdentityId);
        }
    }

    return true;
}

bool FOGUiViewModelService::BuildGachaHistoryFiltered(
    const FOGEntityId& RulerId,
    const FOGGachaHistoryFilter& Filter,
    int32 Limit,
    TArray<FOGGachaHistoryEntryViewModel>& OutHistory,
    FString& OutError) const
{
    OutHistory.Reset();
    OutError.Reset();

    if (Limit <= 0 ||
        Limit > 1000 ||
        (Filter.bUseWorldTickRange &&
         Filter.MinWorldTick >
            Filter.MaxWorldTick))
    {
        OutError =
            TEXT("Gacha history filter/limit is invalid.");
        return false;
    }

    TArray<FOGGachaHistoryEntryViewModel> Raw;
    if (!BuildGachaHistory(
            RulerId,
            1000,
            Raw,
            OutError))
    {
        return false;
    }

    for (const FOGGachaHistoryEntryViewModel& Entry :
         Raw)
    {
        if (Filter.BannerId.IsValid() &&
            Entry.BannerId !=
                Filter.BannerId)
        {
            continue;
        }
        if (Filter.IdentityId.IsValid() &&
            Entry.IdentityId !=
                Filter.IdentityId)
        {
            continue;
        }
        if (!Filter.Rarity.IsNone() &&
            Entry.Rarity !=
                Filter.Rarity)
        {
            continue;
        }
        if (Filter.bUseWorldTickRange &&
            (Entry.WorldTick <
                 Filter.MinWorldTick ||
             Entry.WorldTick >
                 Filter.MaxWorldTick))
        {
            continue;
        }

        OutHistory.Add(
            Entry);
        if (OutHistory.Num() >=
            Limit)
        {
            break;
        }
    }

    return true;
}

bool FOGUiViewModelService::SetActiveTerritoryOverlay(
    FOGTerritoryViewModel& InOutViewModel,
    FName Overlay,
    FString& OutError)
{
    OutError.Reset();

    if (Overlay.IsNone())
    {
        InOutViewModel.ActiveOverlay =
            NAME_None;
        return true;
    }

    if (!InOutViewModel.AnalyticalOverlays.Contains(
            Overlay))
    {
        OutError =
            TEXT("Territory overlay is not one of the authored analytical layers.");
        return false;
    }

    // One slot by construction means enabling a new major overlay replaces the
    // previous one instead of stacking unreadable strategic layers.
    InOutViewModel.ActiveOverlay =
        Overlay;
    return true;
}

bool FOGUiViewModelService::BuildChronicleFiltered(
    const FOGEntityId& RulerId,
    const FOGChronicleFilter& Filter,
    TArray<FOGChronicleEntryViewModel>& OutEntries,
    FString& OutError) const
{
    OutEntries.Reset();
    OutError.Reset();

    if (!RulerId.IsValid() ||
        Filter.Limit <= 0 ||
        Filter.Limit > 1000)
    {
        OutError =
            TEXT("Chronicle filter request is invalid.");
        return false;
    }

    const FOGEntityId QueryEntity =
        Filter.EntityId.IsValid()
            ? Filter.EntityId
            : RulerId;

    TArray<FOGWorldEvent> Events;
    if (!Store.ListWorldEvents(
            QueryEntity,
            NAME_None,
            true,
            1000,
            Events,
            OutError))
    {
        return false;
    }

    for (const FOGWorldEvent& Event :
         Events)
    {
        if (!Filter.EventTypes.IsEmpty() &&
            !Filter.EventTypes.Contains(
                Event.EventType))
        {
            continue;
        }

        const FName Importance =
            ExtractChronicleImportance(
                Event.PayloadJson);
        if (!Filter.ImportanceLevels.IsEmpty() &&
            !Filter.ImportanceLevels.Contains(
                Importance))
        {
            continue;
        }

        FOGChronicleEntryViewModel View;
        View.EventId =
            Event.EventId;
        View.EventType =
            Event.EventType;
        View.WorldTick =
            Event.WorldTick;
        View.PrimaryEntityId =
            Event.PrimaryEntity;
        View.RelatedEntityIds =
            Event.RelatedEntities;
        View.PayloadJson =
            Event.PayloadJson;
        View.Importance =
            Importance;
        OutEntries.Add(
            MoveTemp(View));

        if (OutEntries.Num() >=
            Filter.Limit)
        {
            break;
        }
    }

    return true;
}

bool FOGUiViewModelService::BuildIntelligenceFiltered(
    const FOGEntityId& RulerId,
    const FOGIntelligenceFilter& Filter,
    TArray<FOGIntelligenceEntryViewModel>& OutEntries,
    FString& OutError) const
{
    TArray<FOGIntelligenceEntryViewModel> All;
    if (!BuildIntelligence(
            RulerId,
            All,
            OutError))
    {
        return false;
    }

    OutEntries.Reset();
    for (const FOGIntelligenceEntryViewModel& Entry :
         All)
    {
        if (Filter.SubjectEntityId.IsValid() &&
            Entry.SubjectEntityId !=
                Filter.SubjectEntityId)
        {
            continue;
        }

        if (!Filter.States.IsEmpty() &&
            !Filter.States.Contains(
                Entry.Knowledge.KnowledgeState))
        {
            continue;
        }

        OutEntries.Add(
            Entry);
    }

    return true;
}

FOGCodexViewModel FOGUiViewModelService::BuildCodex(
    const TArray<FOGCodexEntryViewModel>& KnownEntries,
    const FString& SearchText,
    FName CategoryFilter)
{
    FOGCodexViewModel View;
    View.Categories =
    {
        FName(TEXT("character_identities")),
        FName(TEXT("observed_manifestations")),
        FName(TEXT("creatures_species")),
        FName(TEXT("worlds_locations")),
        FName(TEXT("powers_effects")),
        FName(TEXT("items")),
        FName(TEXT("factions")),
        FName(TEXT("other"))
    };

    for (const FOGCodexEntryViewModel& Entry :
         KnownEntries)
    {
        if (!Entry.EntryId.IsValid() ||
            Entry.Category.IsNone())
        {
            continue;
        }

        if (!CategoryFilter.IsNone() &&
            Entry.Category !=
                CategoryFilter)
        {
            continue;
        }

        if (!MatchesSearch(
                SearchText,
                Entry.DisplayNameKey) &&
            !MatchesSearch(
                SearchText,
                Entry.EntryId.ToString()))
        {
            continue;
        }

        View.VisibleEntries.Add(
            Entry);
    }

    return View;
}

bool FOGUiViewModelService::BuildWorldTarget(
    const FOGEntityId& TargetEntityId,
    const FOGLargeNumber& CurrentHp,
    const FOGKnowledgeFactRecord* HpKnowledge,
    const FOGKnowledgeFactRecord* PhaseKnowledge,
    const FOGKnowledgeFactRecord* ResourceKnowledge,
    const TArray<FOGHudStatusEffectInput>& StatusEffects,
    FOGWorldTargetViewModel& OutViewModel,
    FString& OutError)
{
    OutViewModel =
        FOGWorldTargetViewModel();
    OutError.Reset();

    if (!TargetEntityId.IsValid())
    {
        OutError =
            TEXT("World target projection requires a valid target entity.");
        return false;
    }

    OutViewModel.TargetEntityId =
        TargetEntityId;

    const FOGKnowledgeFactViewModel Hp =
        ProjectKnowledgeFact(
            HpKnowledge,
            false);
    OutViewModel.HpDisplay =
        FOGUiNumberFormatter::Format(
            CurrentHp,
            Hp.bExactValueVisible,
            true);
    OutViewModel.Phase =
        ProjectKnowledgeFact(
            PhaseKnowledge,
            false);
    OutViewModel.ResourceState =
        ProjectKnowledgeFact(
            ResourceKnowledge,
            false);

    for (const FOGHudStatusEffectInput& Status :
         StatusEffects)
    {
        if (!Status.EffectId.IsValid() ||
            Status.Stacks <= 0 ||
            Status.RemainingTurns < 0)
        {
            OutError =
                TEXT("World target status-effect input is invalid.");
            OutViewModel =
                FOGWorldTargetViewModel();
            return false;
        }

        FOGHudStatusEffectViewModel View;
        View.EffectId =
            Status.EffectId;
        View.Stacks =
            Status.Stacks;
        View.RemainingTurns =
            Status.RemainingTurns;
        View.bPreciseDetailsVisible =
            Status.bPreciseKnowledge;
        if (Status.bPreciseKnowledge)
        {
            View.PreciseEffectTextKey =
                Status.PreciseEffectTextKey;
        }
        OutViewModel.StatusEffects.Add(
            MoveTemp(View));
    }

    return true;
}

bool FOGUiViewModelService::BuildTurnBattlePresentation(
    const FOGTurnBattleState& BattleState,
    float SelectedSpeed,
    bool bUltimateCinematicsEnabled,
    const TArray<FOGTurnBattleRecapEntryViewModel>& AuthoritativeRecap,
    FOGTurnBattlePresentationViewModel& OutViewModel,
    FString& OutError)
{
    OutViewModel =
        FOGTurnBattlePresentationViewModel();
    OutError.Reset();

    const bool bValidSpeed =
        FMath::IsNearlyEqual(
            SelectedSpeed,
            1.0f) ||
        FMath::IsNearlyEqual(
            SelectedSpeed,
            2.0f) ||
        FMath::IsNearlyEqual(
            SelectedSpeed,
            3.0f);
    if (!bValidSpeed)
    {
        OutError =
            TEXT("Turn-battle presentation speed must be 1x, 2x or 3x.");
        return false;
    }

    OutViewModel.SpeedOptions =
    {
        1.0f,
        2.0f,
        3.0f
    };
    OutViewModel.SelectedSpeed =
        SelectedSpeed;
    OutViewModel.bUltimateCinematicsEnabled =
        bUltimateCinematicsEnabled;
    OutViewModel.QuickAutoPresets =
    {
        FName(TEXT("balanced")),
        FName(TEXT("aggressive")),
        FName(TEXT("conservative"))
    };

    for (const FOGCombatUnitState& Unit :
         BattleState.Units)
    {
        if (!Unit.UnitEntityId.IsValid() ||
            Unit.Presence ==
                EOGCombatPresence::Defeated)
        {
            continue;
        }

        FOGTurnTimelineEntryViewModel Entry;
        Entry.EntityId =
            Unit.UnitEntityId;
        Entry.ActionValue =
            Unit.NextActionValue;
        OutViewModel.Timeline.Add(
            MoveTemp(Entry));
    }

    OutViewModel.Timeline.Sort(
        [](const FOGTurnTimelineEntryViewModel& A,
           const FOGTurnTimelineEntryViewModel& B)
        {
            if (A.ActionValue !=
                B.ActionValue)
            {
                return A.ActionValue <
                    B.ActionValue;
            }
            return A.EntityId.ToString().Compare(
                B.EntityId.ToString(),
                ESearchCase::CaseSensitive) < 0;
        });

    TSet<FOGEntityId> SeenRecapEntities;
    for (const FOGTurnBattleRecapEntryViewModel& Recap :
         AuthoritativeRecap)
    {
        if (!Recap.EntityId.IsValid() ||
            SeenRecapEntities.Contains(
                Recap.EntityId))
        {
            OutError =
                TEXT("Turn-battle recap requires unique valid character IDs.");
            OutViewModel =
                FOGTurnBattlePresentationViewModel();
            return false;
        }

        SeenRecapEntities.Add(
            Recap.EntityId);
        OutViewModel.Recap.Add(
            Recap);
    }

    return true;
}

FOGBackupManagerViewModel FOGUiViewModelService::BuildBackupManager(
    const TArray<FOGBackupCatalogEntry>& Entries)
{
    FOGBackupManagerViewModel View;
    View.GlobalActions =
    {
        FName(TEXT("create_manual_snapshot")),
        FName(TEXT("import_backup"))
    };

    for (const FOGBackupCatalogEntry& Entry :
         Entries)
    {
        if (Entry.BackupId.IsEmpty() ||
            Entry.SchemaVersion < 0)
        {
            continue;
        }

        FOGBackupEntryViewModel Backup;
        Backup.BackupId =
            Entry.BackupId;
        Backup.SchemaVersion =
            Entry.SchemaVersion;
        Backup.CreatedUtc =
            Entry.CreatedUtc;
        Backup.SourceBuildVersion =
            Entry.SourceBuildVersion;
        Backup.ValidationState =
            Entry.ValidationState;
        Backup.Actions =
        {
            FName(TEXT("export")),
            FName(TEXT("restore"))
        };
        View.Backups.Add(
            MoveTemp(Backup));
    }

    return View;
}

bool FOGUiViewModelService::BuildPackageStorage(
    const FOGPackageSizeResolver& SizeResolver,
    FOGPackageStorageViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGPackageStorageViewModel();
    OutError.Reset();

    TArray<FOGContentPackageRecord> Packages;
    if (!Store.ListContentPackageRecords(
            Packages,
            OutError))
    {
        return false;
    }

    for (const FOGContentPackageRecord& Package :
         Packages)
    {
        FOGPackageStorageEntryViewModel View;
        View.PackageId =
            Package.PackageId;
        View.Version =
            Package.Version;
        View.Category =
            Package.Category;
        View.StorageClass =
            Package.StorageClass;
        View.DownloadState =
            Package.DownloadState;

        if (SizeResolver)
        {
            if (!SizeResolver(
                    Package,
                    View.bSizeKnown,
                    View.SizeBytes,
                    OutError))
            {
                return false;
            }
            if (View.bSizeKnown &&
                View.SizeBytes < 0)
            {
                OutError =
                    TEXT("Package size resolver returned a negative size.");
                return false;
            }
        }

        View.Actions.Add(
            FName(TEXT("inspect_update")));
        if (Package.bInstalled)
        {
            View.Actions.Add(
                FName(TEXT("move")));
            View.Actions.Add(
                FName(TEXT("archive")));
        }

        OutViewModel.Packages.Add(
            MoveTemp(View));
    }

    return true;
}

FOGQualitativeRiskViewModel FOGUiViewModelService::ProjectRisk(
    int32 RiskBps,
    EOGUiKnowledgeState KnowledgeState,
    bool bExactProbabilityAuthorized)
{
    FOGQualitativeRiskViewModel View;
    View.KnowledgeState =
        KnowledgeState;

    if (KnowledgeState ==
            EOGUiKnowledgeState::Unknown ||
        KnowledgeState ==
            EOGUiKnowledgeState::Contradicted ||
        KnowledgeState ==
            EOGUiKnowledgeState::Outdated)
    {
        return View;
    }

    const int32 Clamped =
        FMath::Clamp(
            RiskBps,
            0,
            10000);

    if (Clamped < 2500)
    {
        View.Band =
            EOGUiRiskBand::Low;
    }
    else if (Clamped < 5000)
    {
        View.Band =
            EOGUiRiskBand::Moderate;
    }
    else if (Clamped < 7500)
    {
        View.Band =
            EOGUiRiskBand::High;
    }
    else
    {
        View.Band =
            EOGUiRiskBand::Critical;
    }

    View.bExactProbabilityVisible =
        bExactProbabilityAuthorized &&
        KnowledgeState ==
            EOGUiKnowledgeState::Confirmed;
    if (View.bExactProbabilityVisible)
    {
        View.ExactProbabilityBps =
            Clamped;
    }

    return View;
}

bool FOGUiViewModelService::BuildProject(
    const FOGEntityId& ProjectId,
    FOGProjectViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGProjectViewModel();
    OutError.Reset();

    bool bFound = false;
    FOGProjectRecord Project;
    if (!Store.TryReadProject(
            ProjectId,
            bFound,
            Project,
            OutError))
    {
        return false;
    }
    if (!bFound)
    {
        OutError =
            TEXT("Project UI projection references an unknown Project.");
        return false;
    }

    OutViewModel.ProjectId =
        ProjectId;
    OutViewModel.Status =
        ProjectStatusName(
            Project.Status);
    OutViewModel.ProgressBps =
        Project.ProgressBps;

    TArray<FOGProjectPhaseRecord> Phases;
    if (!Store.ListProjectPhases(
            ProjectId,
            Phases,
            OutError))
    {
        return false;
    }
    Phases.Sort(
        [](const FOGProjectPhaseRecord& A,
           const FOGProjectPhaseRecord& B)
        {
            return A.Sequence <
                B.Sequence;
        });
    for (const FOGProjectPhaseRecord& Phase :
         Phases)
    {
        OutViewModel.PhaseIds.Add(
            Phase.PhaseId);
    }

    TArray<FOGProjectAssignmentRecord> Assignments;
    if (!Store.ListProjectAssignments(
            ProjectId,
            Assignments,
            OutError))
    {
        return false;
    }
    for (const FOGProjectAssignmentRecord& Assignment :
         Assignments)
    {
        if (!OutViewModel.NamedAssigneeIds.Contains(
                Assignment.AssigneeEntityId))
        {
            OutViewModel.NamedAssigneeIds.Add(
                Assignment.AssigneeEntityId);
        }
    }

    return true;
}

bool FOGUiViewModelService::BuildDispatch(
    const FOGEntityId& DispatchId,
    EOGUiKnowledgeState RiskKnowledgeState,
    bool bExactRiskAuthorized,
    FOGDispatchViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGDispatchViewModel();
    OutError.Reset();

    bool bFound = false;
    FOGDispatchRecord Dispatch;
    if (!Store.TryReadDispatch(
            DispatchId,
            bFound,
            Dispatch,
            OutError))
    {
        return false;
    }
    if (!bFound)
    {
        OutError =
            TEXT("Dispatch UI projection references an unknown Dispatch.");
        return false;
    }

    OutViewModel.DispatchId =
        DispatchId;
    OutViewModel.Status =
        DispatchStatusName(
            Dispatch.Status);
    OutViewModel.Risk =
        ProjectRisk(
            Dispatch.RiskBps,
            RiskKnowledgeState,
            bExactRiskAuthorized);

    TArray<FOGDispatchObjectiveRecord> Objectives;
    if (!Store.ListDispatchObjectives(
            DispatchId,
            Objectives,
            OutError))
    {
        return false;
    }
    for (const FOGDispatchObjectiveRecord& Objective :
         Objectives)
    {
        if (Objective.bMandatory)
        {
            OutViewModel.MandatoryObjectiveIds.Add(
                Objective.ObjectiveId);
        }
        else
        {
            OutViewModel.SecondaryObjectiveIds.Add(
                Objective.ObjectiveId);
        }
    }

    TArray<FOGDispatchConstraintRecord> Constraints;
    if (!Store.ListDispatchConstraints(
            DispatchId,
            Constraints,
            OutError))
    {
        return false;
    }
    for (const FOGDispatchConstraintRecord& Constraint :
         Constraints)
    {
        OutViewModel.ConstraintIds.Add(
            Constraint.ConstraintId);
    }

    return true;
}

bool FOGUiViewModelService::BuildWar(
    const FOGEntityId& WarId,
    EOGUiKnowledgeState OutcomeKnowledgeState,
    int32 OutcomeRiskBps,
    bool bExactOutcomeAuthorized,
    FOGWarViewModel& OutViewModel,
    FString& OutError) const
{
    OutViewModel =
        FOGWarViewModel();
    OutError.Reset();

    bool bFound = false;
    FOGWarRecord War;
    if (!Store.TryReadWar(
            WarId,
            bFound,
            War,
            OutError))
    {
        return false;
    }
    if (!bFound)
    {
        OutError =
            TEXT("War UI projection references an unknown War.");
        return false;
    }

    OutViewModel.WarId =
        WarId;
    OutViewModel.Status =
        WarStatusName(
            War.Status);
    OutViewModel.OutcomeConfidence =
        ProjectRisk(
            OutcomeRiskBps,
            OutcomeKnowledgeState,
            bExactOutcomeAuthorized);

    TArray<FOGWarFrontRecord> Fronts;
    if (!Store.ListWarFronts(
            WarId,
            Fronts,
            OutError))
    {
        return false;
    }
    for (const FOGWarFrontRecord& Front :
         Fronts)
    {
        OutViewModel.FrontIds.Add(
            Front.FrontId);
    }

    TArray<FOGWarObjectiveRecord> Objectives;
    if (!Store.ListWarObjectives(
            WarId,
            Objectives,
            OutError))
    {
        return false;
    }
    for (const FOGWarObjectiveRecord& Objective :
         Objectives)
    {
        OutViewModel.ObjectiveIds.Add(
            Objective.ObjectiveId);
    }

    TArray<FOGWarOrderRecord> Orders;
    if (!Store.ListWarOrders(
            WarId,
            Orders,
            OutError))
    {
        return false;
    }
    for (const FOGWarOrderRecord& Order :
         Orders)
    {
        OutViewModel.IssuedIntentIds.Add(
            Order.IntentId);
    }

    return true;
}
