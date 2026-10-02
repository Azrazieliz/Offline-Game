#pragma once

#include "CoreMinimal.h"
#include "OGContentId.generated.h"

/**
 * Stable identifier for immutable/content-defined objects.
 *
 * Unlike FOGEntityId (runtime/persistent entity UUID), a content ID names a
 * definition shipped by a validated content package. Display names may change;
 * content IDs must not.
 *
 * Format: namespace:key
 * Example: core:character.test_001
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGContentId
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Value;

    FOGContentId() = default;
    explicit FOGContentId(FString InValue)
        : Value(MoveTemp(InValue))
    {
    }

    bool IsValid() const
    {
        int32 SeparatorIndex = INDEX_NONE;
        if (!Value.FindChar(TEXT(':'), SeparatorIndex) ||
            SeparatorIndex <= 0 ||
            SeparatorIndex >= Value.Len() - 1)
        {
            return false;
        }

        auto IsAllowed = [](TCHAR Character)
        {
            return FChar::IsAlnum(Character) ||
                   Character == TEXT('_') ||
                   Character == TEXT('-') ||
                   Character == TEXT('.') ||
                   Character == TEXT('/');
        };

        for (int32 Index = 0; Index < Value.Len(); ++Index)
        {
            if (Index == SeparatorIndex)
            {
                continue;
            }

            const TCHAR Character = Value[Index];
            if (!IsAllowed(Character) || FChar::IsUpper(Character))
            {
                return false;
            }
        }

        return true;
    }

    bool IsEmpty() const { return Value.IsEmpty(); }

    FString ToString() const { return Value; }

    bool operator==(const FOGContentId& Other) const
    {
        return Value == Other.Value;
    }

    bool operator!=(const FOGContentId& Other) const
    {
        return !(*this == Other);
    }
};

FORCEINLINE uint32 GetTypeHash(const FOGContentId& Id)
{
    return GetTypeHash(Id.Value);
}
