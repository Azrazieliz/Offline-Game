#pragma once

#include "CoreMinimal.h"
#include "OGEntityId.generated.h"

/**
 * Stable identity used by the authoritative world state.
 *
 * Unreal Actor lifetime must never be used as persistent entity identity.
 * The same ID survives unloading, World Mode/Ruler Mode transitions, process
 * restarts, and content-package eviction.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGEntityId
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGuid Value;

    FOGEntityId() = default;
    explicit FOGEntityId(const FGuid& InValue) : Value(InValue) {}

    static FOGEntityId NewId()
    {
        return FOGEntityId(FGuid::NewGuid());
    }

    bool IsValid() const
    {
        return Value.IsValid();
    }

    FString ToString() const
    {
        return Value.ToString(EGuidFormats::DigitsWithHyphensLower);
    }

    bool operator==(const FOGEntityId& Other) const
    {
        return Value == Other.Value;
    }

    bool operator!=(const FOGEntityId& Other) const
    {
        return !(*this == Other);
    }
};

FORCEINLINE uint32 GetTypeHash(const FOGEntityId& Id)
{
    return GetTypeHash(Id.Value);
}
