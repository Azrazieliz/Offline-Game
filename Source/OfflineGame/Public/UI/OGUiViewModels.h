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

class OFFLINEGAME_API FOGUiNumberFormatter
{
public:
    static FString Format(
        const FOGLargeNumber& Value,
        bool bPrecisionKnown,
        bool bCombatCompact);
};
