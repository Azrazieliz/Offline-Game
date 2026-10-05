#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Core/OGEntityId.h"
#include "Math/OGLargeNumber.h"
#include "OGTerritoryStateRecords.generated.h"

UENUM(BlueprintType)
enum class EOGDomainCoreLifecycle : uint8
{
    Dormant,
    Awakened,
    Broken,
    Absorbed
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTerritoryRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TerritoryId;

    /** Invalid means currently neutral/unclaimed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RulerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RootLocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bMainTerritory = false;

    /** Aggregate only. No individual demographic simulation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Population = 0;

    /** Controlled / Disputed / Neutral / etc.; data-level state, not politics. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ControlState = FName(TEXT("controlled"));
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGLocationTerritoryRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId LocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TerritoryId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName RelationKind = FName(TEXT("contained"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 CoverageBps = 10000;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTerritoryClaimRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ClaimId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TerritoryId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RulerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ClaimKind = FName(TEXT("control"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName ControlState = FName(TEXT("controlled"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ControlStrengthBps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ClaimStartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasEffectiveControlStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 EffectiveControlStartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasDisplacedWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 DisplacedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasReclaimDeadline = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ReclaimDeadlineWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRulerSovereigntyStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RulerId;

    /** 'none' is a persistence sentinel, not a universal sovereignty title. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CurrentTitle = FName(TEXT("none"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName HistoricalPeakTitle = FName(TEXT("none"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasContinuousControlStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ContinuousControlStartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasLastEffectiveControlTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 LastEffectiveControlWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString ScopeStateJson = TEXT("{}");

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGRulerGachaAccessRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId RulerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasQualificationStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 QualificationStartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasQualificationSuspendedTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 QualificationSuspendedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasUnlockedWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UnlockedWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bPermanentlyUnlocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 UpdatedWorldTick = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTerritoryEffectiveControlResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FOGEntityId TerritoryId;

    /** Multiple entries represent genuine effective co-sovereignty/contestation. */
    UPROPERTY(BlueprintReadOnly)
    TArray<FOGEntityId> EffectiveRulerIds;

    UPROPERTY(BlueprintReadOnly)
    TArray<FOGEntityId> EffectiveClaimIds;

    UPROPERTY(BlueprintReadOnly)
    bool bContested = false;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDomainCoreAspect
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId AspectId;

    /** Optional authored progression grade for this Aspect. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Grade = 0;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDomainCoreRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId CoreId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TerritoryId;

    /** Invalid when uncontrolled/neutral/broken as appropriate. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ControllerRulerId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGDomainCoreLifecycle Lifecycle = EOGDomainCoreLifecycle::Dormant;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber CurrentDurability;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGLargeNumber MaxDurability;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGDomainCoreAspect> Aspects;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGTerritoryDomainStateRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId TerritoryId;

    /** Current metaphysical heart. Invalid when the Domain has no active heart. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ActiveCoreId;

    /**
     * none / functional / damaged / heart_lost_ruining / ruined /
     * reconstituting. Physical Territory damage is intentionally separate.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName DomainState = FName(TEXT("none"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasHeartLostWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 HeartLostWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bHasRuinStartedWorldTick = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 RuinStartedWorldTick = 0;

    /** Exceptional high-order recovery Project, never ordinary repair. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ReconstitutionProjectId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDomainCoreConceptRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId CoreId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ConceptId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Grade = 0;

    /** Original Core that contributed this Concept, when provenance is known. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OriginSourceCoreId;

    /** Content-authored synthesis rule that produced/transformed this Concept. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SynthesisRuleId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDomainCoreSynthesisConcept
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ConceptId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Grade = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDomainCoreFusionRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FusionId;

    /** Baseline fusion keeps the absorber as the surviving/result Core. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ResultCoreId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId AbsorberCoreId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId AbsorbedCoreId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 FusionWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SequenceOrdinal = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SynthesisRuleId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName OutcomeKind = FName(TEXT("preserved"));

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ResolutionSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString InstabilityStateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDomainCoreLineageRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ResultCoreId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId SourceCoreId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId FusionId;

    /** absorber / absorbed. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName LineageRole = NAME_None;
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGDomainCoreFusionRequest
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId AbsorberCoreId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId AbsorbedCoreId;

    /**
     * Required content-authored synthesis rule. The runtime never invents
     * arbitrary executable abilities from concept names.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId SynthesisRuleId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ResolutionSeed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName OutcomeKind = FName(TEXT("preserved"));

    /**
     * Optional bounded, content-resolved synthetic Concepts. Source Concepts
     * are preserved automatically; this array only adds authored/systemic
     * synthesis outputs.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGDomainCoreSynthesisConcept> SynthesizedConcepts;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString InstabilityStateJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGResourceBalance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ResourceId;

    /** Resources are ordinary aggregate counters unless proven otherwise. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Amount = 0;
};

UENUM(BlueprintType)
enum class EOGProjectStatus : uint8
{
    Planned,
    Active,
    Completed,
    Failed,
    Cancelled
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGProjectRecord
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId ProjectId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId OwnerEntityId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId LocationId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ProjectTypeId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGProjectStatus Status = EOGProjectStatus::Planned;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 StartWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 ResolveWorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 ProgressBps = 0;

    /** Sparse project-specific state; searchable fields remain normalized. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PayloadJson = TEXT("{}");
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGProjectResourceCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId ResourceId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Amount = 0;
};
