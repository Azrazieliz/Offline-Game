#pragma once

#include "CoreMinimal.h"
#include "OGLargeNumber.generated.h"

/**
 * Deterministic scientific-decimal style number for very large combat values.
 *
 * Value = Significand * 10^Exponent10
 *
 * Significand is normalized to at most 9 decimal digits of precision so
 * Significand * Significand always fits signed int64 during multiplication.
 * This avoids floating-point overflow and keeps serialization simple.
 */
USTRUCT(BlueprintType)
struct OFFLINEGAME_API FOGLargeNumber
{
    GENERATED_BODY()

    static constexpr int64 MaxSignificand = 999999999;
    static constexpr int64 MinNormalizedAbs = 100000000;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int64 Significand = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 Exponent10 = 0;

    FOGLargeNumber() = default;
    FOGLargeNumber(int64 InSignificand, int32 InExponent10);

    static FOGLargeNumber FromInt64(int64 Value);

    bool IsZero() const { return Significand == 0; }
    int32 GetSign() const { return Significand > 0 ? 1 : (Significand < 0 ? -1 : 0); }

    void Normalize();

    static int32 Compare(const FOGLargeNumber& A, const FOGLargeNumber& B);
    static FOGLargeNumber Add(const FOGLargeNumber& A, const FOGLargeNumber& B);
    static FOGLargeNumber Multiply(const FOGLargeNumber& A, const FOGLargeNumber& B);

    FString ToDebugString() const;

    bool operator==(const FOGLargeNumber& Other) const
    {
        return Significand == Other.Significand &&
               Exponent10 == Other.Exponent10;
    }
};
