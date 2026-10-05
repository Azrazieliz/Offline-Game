#pragma once

#include "CoreMinimal.h"
#include "Combat/OGTurnBattle.h"
#include "Persistence/OGRecoveryCatalogService.h"
#include "Gacha/OGGachaDefinitions.h"
#include "Persistence/OGWorldStore.h"
#include "UI/OGUiViewModels.h"
#include "World/OGWorldStateRecords.h"

/**
 * Read-only player-facing projections over canonical world state.
 *
 * These view models never become a second source of truth. Unknown information
 * stays Unknown, formulas stay internal, and presentation preferences remain
 * outside world causality.
 */
using FOGResolvedStatsProjectionResolver =
    TFunction<bool(
        const FOGEntityId& ManifestationId,
        FString& OutResolvedStatsJson,
        FString& OutError)>;

using FOGPresentationCompatibilityResolver =
    TFunction<bool(
        const FOGEntityId& ManifestationId,
        const FOGContentId& PresentationId,
        bool& bOutCompatible,
        FString& OutError)>;

using FOGPackageSizeResolver =
    TFunction<bool(
        const FOGContentPackageRecord& Package,
        bool& bOutKnown,
        int64& OutSizeBytes,
        FString& OutError)>;

using FOGPackageStorageActionResolver =
    TFunction<bool(
        const FOGContentPackageRecord& Package,
        bool& bOutMoveAllowed,
        bool& bOutArchiveAllowed,
        FString& OutError)>;

class OFFLINEGAME_API FOGUiViewModelService
{
public:
    explicit FOGUiViewModelService(IOGWorldStore& InStore)
        : Store(InStore)
    {
    }

    bool BuildRulerShell(
        const FOGEntityId& RulerId,
        FOGRulerShellViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildRoster(
        const FOGEntityId& RulerId,
        TArray<FOGRosterIdentityViewModel>& OutRoster,
        FString& OutError) const;

    bool BuildCharacterIdentity(
        const FOGEntityId& RulerId,
        const FOGCharacterIdentityDefinition& Identity,
        FOGCharacterIdentityViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildGacha(
        const FOGEntityId& RulerId,
        const FOGGachaBannerDefinition& Banner,
        FOGGachaViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildGachaHistory(
        const FOGEntityId& RulerId,
        int32 Limit,
        TArray<FOGGachaHistoryEntryViewModel>& OutHistory,
        FString& OutError) const;

    bool BuildTerritory(
        const FOGEntityId& RulerId,
        const FOGEntityId& SelectedTerritoryId,
        FOGTerritoryViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildRecordsHub(
        const FOGEntityId& RulerId,
        FOGRecordsHubViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildChronicle(
        const FOGEntityId& RulerId,
        int32 Limit,
        TArray<FOGChronicleEntryViewModel>& OutEntries,
        FString& OutError) const;

    bool BuildIntelligence(
        const FOGEntityId& RulerId,
        TArray<FOGIntelligenceEntryViewModel>& OutEntries,
        FString& OutError) const;

    bool BuildWorldHud(
        const FOGEntityId& KnowledgeOwnerId,
        const FOGEntityId& ControlledEntityId,
        const TArray<FOGEntityId>& CompanionIds,
        const TSet<FOGEntityId>& QteReadyIds,
        const FOGKnowledgeFactRecord* TargetConditionFact,
        bool bTargetConditionOutdated,
        FOGWorldHudViewModel& OutViewModel,
        FString& OutError) const;

    static FOGKnowledgeFactViewModel ProjectKnowledgeFact(
        const FOGKnowledgeFactRecord* Fact,
        bool bOutdated);

    static FOGOpeningViewModel BuildOpening(
        bool bWorldExists,
        bool bRecoverableWorldExists,
        bool bImportBackupAvailable,
        bool bReducedMotionPresentation);

    static FOGContentId CharacterLastUsedContextId(
        const FOGContentId& IdentityId);

    bool BuildRosterWithQuery(
        const FOGEntityId& RulerId,
        const TArray<FOGCharacterIdentityDefinition>& IdentityDefinitions,
        const FOGRosterQuery& Query,
        TArray<FOGRosterIdentityViewModel>& OutRoster,
        FString& OutError) const;

    bool BuildManifestationDetail(
        const FOGEntityId& RulerId,
        const FOGEntityId& ManifestationId,
        const FString& SkillSearchText,
        int32 MaxVisibleSkills,
        const FOGResolvedStatsProjectionResolver& StatsResolver,
        FOGManifestationDetailViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildManifestationComparison(
        const FOGEntityId& RulerId,
        const FOGEntityId& LeftManifestationId,
        const FOGEntityId& RightManifestationId,
        const FOGResolvedStatsProjectionResolver& StatsResolver,
        FOGManifestationComparisonViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildWardrobe(
        const FOGEntityId& RulerId,
        const FOGEntityId& ManifestationId,
        const FOGPresentationCompatibilityResolver& CompatibilityResolver,
        FOGWardrobeViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildAdultUtility(
        const FOGCharacterIdentityDefinition& Identity,
        const FOGEntityId& CharacterEntityId,
        bool bSfwPresentationEnabled,
        const TArray<FOGContentId>& AvailableSystemicInteractionIds,
        FOGAdultUtilityViewModel& OutViewModel,
        FString& OutError) const;

    static FOGCraftingViewModel BuildCraftingShell();

    static bool BuildEquipmentComparison(
        const FOGEntityId& CurrentItemId,
        const FOGEntityId& CandidateItemId,
        const FString& ResolvedStatDeltaJson,
        const TArray<FOGContentId>& GainedSkillIds,
        const TArray<FOGContentId>& LostSkillIds,
        const TArray<FString>& CompatibilityWarnings,
        const FString& AffinityImplicationTextKey,
        const FString& ProficiencyImplicationTextKey,
        FOGEquipmentComparisonViewModel& OutViewModel,
        FString& OutError);

    bool BuildGachaDetails(
        const FOGGachaBannerDefinition& Banner,
        const TArray<FOGContentId>& AuthoredDesignationOptions,
        const FString& SpecialRulesJson,
        FOGGachaDetailsViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildGachaHistoryFiltered(
        const FOGEntityId& RulerId,
        const FOGGachaHistoryFilter& Filter,
        int32 Limit,
        TArray<FOGGachaHistoryEntryViewModel>& OutHistory,
        FString& OutError) const;

    static bool SetActiveTerritoryOverlay(
        FOGTerritoryViewModel& InOutViewModel,
        FName Overlay,
        FString& OutError);

    bool BuildChronicleFiltered(
        const FOGEntityId& RulerId,
        const FOGChronicleFilter& Filter,
        TArray<FOGChronicleEntryViewModel>& OutEntries,
        FString& OutError) const;

    bool BuildIntelligenceFiltered(
        const FOGEntityId& RulerId,
        const FOGIntelligenceFilter& Filter,
        TArray<FOGIntelligenceEntryViewModel>& OutEntries,
        FString& OutError) const;

    static FOGCodexViewModel BuildCodex(
        const TArray<FOGCodexEntryViewModel>& KnownEntries,
        const FString& SearchText,
        FName CategoryFilter);

    static bool BuildWorldTarget(
        const FOGEntityId& TargetEntityId,
        const FOGLargeNumber& CurrentHp,
        const FOGKnowledgeFactRecord* HpKnowledge,
        const FOGKnowledgeFactRecord* PhaseKnowledge,
        const FOGKnowledgeFactRecord* ResourceKnowledge,
        const TArray<FOGHudStatusEffectInput>& StatusEffects,
        FOGWorldTargetViewModel& OutViewModel,
        FString& OutError);

    static bool BuildTurnBattlePresentation(
        const FOGTurnBattleState& BattleState,
        float SelectedSpeed,
        bool bUltimateCinematicsEnabled,
        const TArray<FOGTurnTimelineEntryViewModel>& RuntimeTimelineEvents,
        const TArray<FOGTurnBattleRecapEntryViewModel>& AuthoritativeRecap,
        FOGTurnBattlePresentationViewModel& OutViewModel,
        FString& OutError);

    static FOGBackupManagerViewModel BuildBackupManager(
        const TArray<FOGBackupCatalogEntry>& Entries);

    bool BuildPackageStorage(
        const FOGPackageSizeResolver& SizeResolver,
        const FOGPackageStorageActionResolver& ActionResolver,
        FOGPackageStorageViewModel& OutViewModel,
        FString& OutError) const;

    static bool ProjectRisk(
        int32 RiskBps,
        EOGUiKnowledgeState KnowledgeState,
        bool bExactProbabilityAuthorized,
        const TArray<int32>& AuthoredBandThresholdBps,
        FOGQualitativeRiskViewModel& OutViewModel,
        FString& OutError);

    bool BuildProject(
        const FOGEntityId& ProjectId,
        FOGProjectViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildDispatch(
        const FOGEntityId& DispatchId,
        EOGUiKnowledgeState RiskKnowledgeState,
        bool bExactRiskAuthorized,
        const TArray<int32>& AuthoredRiskBandThresholdBps,
        FOGDispatchViewModel& OutViewModel,
        FString& OutError) const;

    bool BuildWar(
        const FOGEntityId& WarId,
        EOGUiKnowledgeState OutcomeKnowledgeState,
        int32 OutcomeRiskBps,
        bool bExactOutcomeAuthorized,
        const TArray<int32>& AuthoredRiskBandThresholdBps,
        FOGWarViewModel& OutViewModel,
        FString& OutError) const;

private:
    IOGWorldStore& Store;
};
