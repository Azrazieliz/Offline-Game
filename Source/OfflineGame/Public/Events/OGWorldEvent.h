#pragma once

#include "CoreMinimal.h"
#include "Core/OGEntityId.h"
#include "OGWorldEvent.generated.h"

/**
 * Append-only meaningful world event.
 *
 * The event ledger is not a transcript of simulation noise. It records events
 * that can affect persistence, knowledge, Chronicle output, debugging, or later
 * causal decisions.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGWorldEvent
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId EventId;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName EventType = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 WorldTick = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FOGEntityId PrimaryEntity;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FOGEntityId> RelatedEntities;

    /** Extensible payload. Core indexed facts should still receive real columns. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PayloadJson;

    /** Whether the Chronicle projector may surface this event to the player. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bChronicleEligible = false;
};
