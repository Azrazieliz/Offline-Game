#pragma once

#include "CoreMinimal.h"
#include "Characters/OGCharacterDefinitions.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Math/OGLargeNumber.h"
#include "World/OGWorldStateRecords.h"
#include "OGUiViewModels.generated.h"

UENUM(BlueprintType)
enum class EOGRulerPrimaryDestination : uint8
{
    Home,
    Characters,
    Gacha,
    Territory,
    Records
};

UENUM(BlueprintType)
enum class EOGUiKnowledgeState : uint8
{
    Unknown,
    Confirmed,
    Estimated,
    Rumor,
    Contradicted,
    Outdated
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRulerShellViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<EOGRulerPrimaryDestination> PrimaryDestinations;

    UPROPERTY(BlueprintReadOnly)
    bool bGachaUnlocked = false;

    UPROPERTY(BlueprintReadOnly)
    int32 UnacknowledgedReportCount = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bHomeUsesContextualNotices = true;

    UPROPERTY(BlueprintReadOnly)
    bool bRecordsOpensHub = true;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRosterManifestationViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(BlueprintReadOnly)
    FString BuildLabel;

    UPROPERTY(BlueprintReadOnly)
    int32 Level = 1;

    UPROPERTY(BlueprintReadOnly)
    FName CurrentRarity = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId RankId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId WorldFantasmGradeId;

    UPROPERTY(BlueprintReadOnly)
    bool bWorldModeAnchored = false;

    UPROPERTY(BlueprintReadOnly)
    bool bFavorite = false;

    UPROPERTY(BlueprintReadOnly)
    bool bProtected = false;

    UPROPERTY(BlueprintReadOnly)
    bool bLocked = false;

    UPROPERTY(BlueprintReadOnly)
    bool bLastUsed = false;

    UPROPERTY(BlueprintReadOnly)
    int64 AcquisitionWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRosterIdentityViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId SelectedManifestationId;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGRosterManifestationViewModel> Manifestations;

    UPROPERTY(BlueprintReadOnly)
    int32 ManifestationCount = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bFavorite = false;

    UPROPERTY(BlueprintReadOnly)
    bool bProtected = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCharacterIdentityViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayNameKey;

    UPROPERTY(BlueprintReadOnly)
    EOGCanonicalMaturity CanonicalMaturity = EOGCanonicalMaturity::Unknown;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId SelectedManifestationId;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGRosterManifestationViewModel> Manifestations;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> BottomTabs;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> UtilityDestinations;

    UPROPERTY(BlueprintReadOnly)
    bool bAdultUtilityVisible = false;

    UPROPERTY(BlueprintReadOnly)
    bool bCharacterArtDominant = true;

    UPROPERTY(BlueprintReadOnly)
    bool bInternalFormulaBreakdownVisible = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaProbabilityViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId VersionId;

    UPROPERTY(BlueprintReadOnly)
    FName Rarity = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    int32 BaseProbabilityBps = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bFeatured = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaTicketViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId TicketId;

    UPROPERTY(BlueprintReadOnly)
    int64 Balance = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bWillConsumeBeforeCurrency = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bUnlocked = false;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId BannerId;

    UPROPERTY(BlueprintReadOnly)
    int64 CurrencyBalance = 0;

    UPROPERTY(BlueprintReadOnly)
    int64 PullCost = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGGachaTicketViewModel> CompatibleTickets;

    UPROPERTY(BlueprintReadOnly)
    bool bWillUseTicketFirst = false;

    UPROPERTY(BlueprintReadOnly)
    int32 PityCount = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bFeaturedGuarantee = false;

    UPROPERTY(BlueprintReadOnly)
    int32 SoftPityStart = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 HardPity = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGGachaProbabilityViewModel> BasePool;

    UPROPERTY(BlueprintReadOnly)
    bool bDetailsAvailable = true;

    UPROPERTY(BlueprintReadOnly)
    bool bHistoryAvailable = true;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaHistoryEntryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId EventId;

    UPROPERTY(BlueprintReadOnly)
    int64 WorldTick = 0;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId BannerId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId VersionId;

    UPROPERTY(BlueprintReadOnly)
    FName Rarity = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    bool bFeatured = false;

    UPROPERTY(BlueprintReadOnly)
    bool bDuplicateIdentity = false;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId PaymentResourceId;

    UPROPERTY(BlueprintReadOnly)
    bool bUsedTicket = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTerritorySummaryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId TerritoryId;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId RootLocationId;

    UPROPERTY(BlueprintReadOnly)
    bool bMainTerritory = false;

    UPROPERTY(BlueprintReadOnly)
    FName ControlState = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    bool bContested = false;

    UPROPERTY(BlueprintReadOnly)
    FName DomainState = FName(TEXT("none"));
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTerritoryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bMapFirst = true;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> QuickSections;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> AnalyticalOverlays;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGTerritorySummaryViewModel> Territories;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId SelectedTerritoryId;

    UPROPERTY(BlueprintReadOnly)
    FName ActiveOverlay = NAME_None;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRecordsHubViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bOpenAtGeneralHub = true;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> Destinations;

    UPROPERTY(BlueprintReadOnly)
    int32 ReportCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 UnacknowledgedReportCount = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGChronicleEntryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId EventId;

    UPROPERTY(BlueprintReadOnly)
    FName EventType = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    int64 WorldTick = 0;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId PrimaryEntityId;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGEntityId> RelatedEntityIds;

    UPROPERTY(BlueprintReadOnly)
    FString PayloadJson;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGKnowledgeFactViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EOGUiKnowledgeState KnowledgeState = EOGUiKnowledgeState::Unknown;

    UPROPERTY(BlueprintReadOnly)
    bool bExactValueVisible = false;

    UPROPERTY(BlueprintReadOnly)
    FString ValueJson;

    UPROPERTY(BlueprintReadOnly)
    int32 ConfidenceBps = 0;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId SourceEntityId;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId SourceEventId;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGIntelligenceEntryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName FactKey = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId SubjectEntityId;

    UPROPERTY(BlueprintReadOnly)
    FOGKnowledgeFactViewModel Knowledge;

    UPROPERTY(BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId LanguageContextId;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWorldHudCompanionViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId EntityId;

    UPROPERTY(BlueprintReadOnly)
    bool bSwitchAvailable = true;

    UPROPERTY(BlueprintReadOnly)
    bool bQteReady = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWorldHudViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bCompactContextualHud = true;

    UPROPERTY(BlueprintReadOnly)
    bool bPersistentMmoOverlay = false;

    UPROPERTY(BlueprintReadOnly)
    FName CompanionPlacement = FName(TEXT("upper_right"));

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId ControlledEntityId;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId CurrentLocationId;

    UPROPERTY(BlueprintReadOnly)
    bool bShowMinimap = false;

    UPROPERTY(BlueprintReadOnly)
    bool bCurrentLocationMapped = false;

    UPROPERTY(BlueprintReadOnly)
    EOGLocationKnowledgeLevel LocationKnowledge = EOGLocationKnowledgeLevel::Rumored;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGWorldHudCompanionViewModel> Companions;

    UPROPERTY(BlueprintReadOnly)
    FOGKnowledgeFactViewModel TargetCondition;

    UPROPERTY(BlueprintReadOnly)
    bool bShowExactEnemyState = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGOpeningViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bContinueAvailable = false;

    UPROPERTY(BlueprintReadOnly)
    bool bRecoverExistingWorldAvailable = false;

    UPROPERTY(BlueprintReadOnly)
    bool bImportBackupAvailable = true;

    UPROPERTY(BlueprintReadOnly)
    bool bReducedMotionPresentation = false;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> PrimaryActions;

    UPROPERTY(BlueprintReadOnly)
    FName SecondaryDestructiveAction = FName(TEXT("clear_world"));

    UPROPERTY(BlueprintReadOnly)
    bool bClearWorldRequiresConfirmation = true;
};


UENUM(BlueprintType)
enum class EOGRosterSortDimension : uint8
{
    Identity,
    Rank,
    Class,
    Rarity,
    WorldFantasm
};

UENUM(BlueprintType)
enum class EOGUiRiskBand : uint8
{
    Unknown,
    Low,
    Moderate,
    High,
    Critical
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRosterQuery
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SearchText;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FOGContentId> RankIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FOGContentId> ClassIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> Rarities;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FOGContentId> WorldFantasmGradeIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EOGRosterSortDimension SortDimension = EOGRosterSortDimension::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bDescending = false;

    /** Frozen compact-density choices are 2 or 3 columns; default is 3. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Columns = 3;

    /** Content-authored ordering; no Rank/Rarity/Fantasm order is hard-coded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FOGContentId> RankOrder;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> RarityOrder;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FOGContentId> WorldFantasmOrder;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGSkillViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId SkillId;

    UPROPERTY(BlueprintReadOnly)
    FName CurrentState = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> SourceKinds;

    UPROPERTY(BlueprintReadOnly)
    int64 LearnedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRouteNodeViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId RouteId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId NodeId;

    UPROPERTY(BlueprintReadOnly)
    FName State = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    bool bEntered = false;

    UPROPERTY(BlueprintReadOnly)
    bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGFormViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId FormId;

    UPROPERTY(BlueprintReadOnly)
    FName State = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    int64 UnlockedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEquipmentSlotViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId SlotId;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId ItemId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId DefinitionId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId ItemRankId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId QualityId;

    UPROPERTY(BlueprintReadOnly)
    FString EvolutionStateJson;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId AffinityMilestoneId;

    UPROPERTY(BlueprintReadOnly)
    bool bAffinityKnown = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEquipmentProficiencySummaryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId ProficiencyId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId GradeId;

    UPROPERTY(BlueprintReadOnly)
    bool bNumericDetailAvailable = false;

    UPROPERTY(BlueprintReadOnly)
    int64 ProficiencyValue = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGManifestationDetailViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(BlueprintReadOnly)
    FString BuildLabel;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId RankId;

    UPROPERTY(BlueprintReadOnly)
    int32 Level = 1;

    UPROPERTY(BlueprintReadOnly)
    FName CurrentRarity = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId WorldFantasmGradeId;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> ClassIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGSkillViewModel> Skills;

    UPROPERTY(BlueprintReadOnly)
    int32 TotalLearnedSkillCount = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bSkillSearchAvailable = true;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGRouteNodeViewModel> RouteNodes;

    UPROPERTY(BlueprintReadOnly)
    bool bRouteGraphZoomable = true;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGFormViewModel> Forms;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGEquipmentSlotViewModel> Equipment;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGEquipmentProficiencySummaryViewModel> Proficiencies;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId SelectedSkinId;

    UPROPERTY(BlueprintReadOnly)
    FString OutfitStateJson;

    /** Resolved player-facing values only; never internal formulas/decomposition. */
    UPROPERTY(BlueprintReadOnly)
    FString ResolvedStatsJson = TEXT("{}");

    UPROPERTY(BlueprintReadOnly)
    bool bInternalFormulaBreakdownVisible = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGManifestationComparisonViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId IdentityId;

    UPROPERTY(BlueprintReadOnly)
    FOGManifestationDetailViewModel Left;

    UPROPERTY(BlueprintReadOnly)
    FOGManifestationDetailViewModel Right;

    UPROPERTY(BlueprintReadOnly)
    bool bSideBySide = true;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWardrobeUnlockViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId PresentationId;

    UPROPERTY(BlueprintReadOnly)
    bool bCompatible = false;

    UPROPERTY(BlueprintReadOnly)
    FName State = NAME_None;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWardrobeViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId ManifestationId;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId SelectedSkinId;

    UPROPERTY(BlueprintReadOnly)
    FString OutfitStateJson = TEXT("{}");

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGWardrobeUnlockViewModel> OwnedPresentations;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGEquipmentSlotViewModel> VisibleEquipment;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> ArchiveReplayFilters;

    UPROPERTY(BlueprintReadOnly)
    bool bWardrobeIsUtilityNotBottomTab = true;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGAdultUtilityViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    bool bVisible = false;

    UPROPERTY(BlueprintReadOnly)
    bool bRelationshipGatePresent = false;

    UPROPERTY(BlueprintReadOnly)
    bool bFastPrivacySfwToggleAvailable = true;

    UPROPERTY(BlueprintReadOnly)
    bool bSfwPresentationEnabled = false;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> Sections;

    UPROPERTY(BlueprintReadOnly)
    FOGContentId CurrentProfileVariantId;

    UPROPERTY(BlueprintReadOnly)
    FString MutableContextStateJson = TEXT("{}");

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> AvailableSystemicInteractionIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGChronicleEntryViewModel> ArchiveEvents;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> ReplayFilters;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEquipmentComparisonViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId CurrentItemId;

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId CandidateItemId;

    UPROPERTY(BlueprintReadOnly)
    FString ResolvedStatDeltaJson = TEXT("{}");

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> GainedSkillIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> LostSkillIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FString> CompatibilityWarnings;

    UPROPERTY(BlueprintReadOnly)
    FString AffinityImplicationTextKey;

    UPROPERTY(BlueprintReadOnly)
    FString ProficiencyImplicationTextKey;

    UPROPERTY(BlueprintReadOnly)
    bool bInternalFormulaBreakdownVisible = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCraftingViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> Modes;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> ExperimentFields;

    UPROPERTY(BlueprintReadOnly)
    bool bKnownRecipesSearchable = true;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaHistoryFilter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FOGContentId BannerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FOGContentId IdentityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName Rarity = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bUseWorldTickRange = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 MinWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int64 MaxWorldTick = MAX_int64;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGGachaDetailsViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FName CarryCategory = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    int32 SoftPityStart = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 SoftPityBonusPerPullBps = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 HardPity = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bFeaturedGuaranteeAfterMiss = true;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGGachaProbabilityViewModel> DeclaredProbabilities;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> CompatibleTicketIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> AuthoredDesignationOptions;

    UPROPERTY(BlueprintReadOnly)
    FString SpecialRulesJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGChronicleFilter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FOGEntityId EntityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> EventTypes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> ImportanceLevels;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Limit = 100;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGIntelligenceFilter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FOGEntityId SubjectEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EOGUiKnowledgeState> States;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCodexEntryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId EntryId;

    UPROPERTY(BlueprintReadOnly)
    FName Category = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    FString DisplayNameKey;

    UPROPERTY(BlueprintReadOnly)
    EOGUiKnowledgeState KnowledgeState = EOGUiKnowledgeState::Unknown;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGCodexViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> Categories;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGCodexEntryViewModel> VisibleEntries;

    UPROPERTY(BlueprintReadOnly)
    bool bSearchAvailable = true;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGHudStatusEffectInput
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FOGContentId EffectId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Stacks = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 RemainingTurns = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bPreciseKnowledge = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PreciseEffectTextKey;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGHudStatusEffectViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId EffectId;

    UPROPERTY(BlueprintReadOnly)
    int32 Stacks = 1;

    UPROPERTY(BlueprintReadOnly)
    int32 RemainingTurns = 0;

    UPROPERTY(BlueprintReadOnly)
    bool bTapForDetails = true;

    UPROPERTY(BlueprintReadOnly)
    bool bPreciseDetailsVisible = false;

    UPROPERTY(BlueprintReadOnly)
    FString PreciseEffectTextKey;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWorldTargetViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId TargetEntityId;

    UPROPERTY(BlueprintReadOnly)
    bool bPrincipalTarget = true;

    UPROPERTY(BlueprintReadOnly)
    FString HpDisplay = TEXT("Unknown");

    UPROPERTY(BlueprintReadOnly)
    FOGKnowledgeFactViewModel Phase;

    UPROPERTY(BlueprintReadOnly)
    FOGKnowledgeFactViewModel ResourceState;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGHudStatusEffectViewModel> StatusEffects;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTurnTimelineEntryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId EntityId;

    UPROPERTY(BlueprintReadOnly)
    int64 ActionValue = 0;

    UPROPERTY(BlueprintReadOnly)
    FName Marker = FName(TEXT("actor"));
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTurnBattleRecapEntryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId EntityId;

    UPROPERTY(BlueprintReadOnly)
    FOGLargeNumber DirectDamage;

    UPROPERTY(BlueprintReadOnly)
    FOGLargeNumber DotDamage;

    UPROPERTY(BlueprintReadOnly)
    FOGLargeNumber TotalHealing;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTurnBattlePresentationViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<float> SpeedOptions;

    UPROPERTY(BlueprintReadOnly)
    float SelectedSpeed = 1.0f;

    UPROPERTY(BlueprintReadOnly)
    bool bUltimateCinematicsEnabled = true;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGTurnTimelineEntryViewModel> Timeline;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> QuickAutoPresets;

    UPROPERTY(BlueprintReadOnly)
    bool bAdvancedConditionalRuleEditorAvailable = true;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGTurnBattleRecapEntryViewModel> Recap;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGBackupEntryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString BackupId;

    UPROPERTY(BlueprintReadOnly)
    int32 SchemaVersion = 0;

    UPROPERTY(BlueprintReadOnly)
    FString CreatedUtc;

    UPROPERTY(BlueprintReadOnly)
    FString SourceBuildVersion;

    UPROPERTY(BlueprintReadOnly)
    FName ValidationState = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> Actions;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGBackupManagerViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGBackupEntryViewModel> Backups;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> GlobalActions;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGPackageStorageEntryViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGContentId PackageId;

    UPROPERTY(BlueprintReadOnly)
    int32 Version = 0;

    UPROPERTY(BlueprintReadOnly)
    FName Category = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    FName StorageClass = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    FName DownloadState = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    bool bSizeKnown = false;

    UPROPERTY(BlueprintReadOnly)
    int64 SizeBytes = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FName> Actions;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGPackageStorageViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGPackageStorageEntryViewModel> Packages;

    UPROPERTY(BlueprintReadOnly)
    bool bAutomaticMetadataChecksAllowed = true;

    UPROPERTY(BlueprintReadOnly)
    bool bLargeDownloadsDefaultUnmeteredOnly = true;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGQualitativeRiskViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    EOGUiRiskBand Band = EOGUiRiskBand::Unknown;

    UPROPERTY(BlueprintReadOnly)
    bool bExactProbabilityVisible = false;

    UPROPERTY(BlueprintReadOnly)
    int32 ExactProbabilityBps = 0;

    UPROPERTY(BlueprintReadOnly)
    EOGUiKnowledgeState KnowledgeState = EOGUiKnowledgeState::Unknown;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGProjectViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId ProjectId;

    UPROPERTY(BlueprintReadOnly)
    FName Status = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    int32 ProgressBps = 0;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> PhaseIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGEntityId> NamedAssigneeIds;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDispatchViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId DispatchId;

    UPROPERTY(BlueprintReadOnly)
    FName Status = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> MandatoryObjectiveIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> SecondaryObjectiveIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> ConstraintIds;

    UPROPERTY(BlueprintReadOnly)
    FOGQualitativeRiskViewModel Risk;

    /** Deliberately no flat success-percent field. */
    UPROPERTY(BlueprintReadOnly)
    bool bUsesCapabilityKnowledgeResolution = true;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWarViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId WarId;

    UPROPERTY(BlueprintReadOnly)
    FName Status = NAME_None;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGEntityId> FrontIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> ObjectiveIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGContentId> IssuedIntentIds;

    UPROPERTY(BlueprintReadOnly)
    FOGQualitativeRiskViewModel OutcomeConfidence;
};

class OFFLINEGAME_API FOGUiNumberFormatter
{
public:
    static FString Format(
        const FOGLargeNumber& Value,
        bool bPrecisionKnown,
        bool bCombatCompact);
};
