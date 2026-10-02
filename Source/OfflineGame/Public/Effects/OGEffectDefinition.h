#pragma once

#include "CoreMinimal.h"
#include "Content/OGContentId.h"
#include "Rules/OGRulePriority.h"
#include "OGEffectDefinition.generated.h"

UENUM(BlueprintType)
enum class EOGEffectLifetimeKind : uint8
{
    Instant,
    Turns,
    RealTimeMilliseconds,
    Encounter,
    PersistentUntilRemoved
};

USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEffectLifetimeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EOGEffectLifetimeKind Kind = EOGEffectLifetimeKind::Instant;

    /**
     * Turns or milliseconds depending on Kind.
     * Ignored for Instant / Encounter / PersistentUntilRemoved.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Magnitude = 0;
};

/**
 * One mechanically meaningful effect definition.
 *
 * MechanicKind selects registered runtime behavior. FamilyId groups related
 * effects such as Burn -> Ignite -> Calcination without forcing a universal
 * elemental chart.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEffectDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId EffectId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGContentId FamilyId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName MechanicKind = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 PotencyTier = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxStacks = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEffectLifetimeDefinition Lifetime;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGRulePriority RulePriority;

    /**
     * Search/provenance/UI metadata only unless a specific mechanic explicitly
     * reads a tag. Tags are not automatic combat behavior.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FName> MetadataTags;
};
