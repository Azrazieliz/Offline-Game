#include "Math/OGLargeNumber.h"

namespace
{
int64 Pow10Int(int32 Power)
{
    static constexpr int64 Powers[] =
    {
        1LL,
        10LL,
        100LL,
        1000LL,
        10000LL,
        100000LL,
        1000000LL,
        10000000LL,
        100000000LL,
        1000000000LL
    };

    return Power >= 0 && Power <= 9 ? Powers[Power] : 0;
}

uint64 AbsMagnitude(int64 Value)
{
    return Value < 0
        ? static_cast<uint64>(-(Value + 1)) + 1ull
        : static_cast<uint64>(Value);
}

int64 DivideRounded(int64 Value, int64 Divisor)
{
    if (Divisor <= 1)
    {
        return Value;
    }

    const int64 Quotient = Value / Divisor;
    const int64 Remainder = Value % Divisor;
    const uint64 TwiceRemainder = AbsMagnitude(Remainder) * 2ull;

    if (TwiceRemainder < static_cast<uint64>(Divisor))
    {
        return Quotient;
    }

    return Quotient + (Value >= 0 ? 1 : -1);
}
}

FOGLargeNumber::FOGLargeNumber(int64 InSignificand, int32 InExponent10)
    : Significand(InSignificand)
    , Exponent10(InExponent10)
{
    Normalize();
}

FOGLargeNumber FOGLargeNumber::FromInt64(int64 Value)
{
    return FOGLargeNumber(Value, 0);
}

void FOGLargeNumber::Normalize()
{
    if (Significand == 0)
    {
        Exponent10 = 0;
        return;
    }

    uint64 Absolute = AbsMagnitude(Significand);

    while (Absolute > static_cast<uint64>(MaxSignificand))
    {
        Significand = DivideRounded(Significand, 10);
        ++Exponent10;
        Absolute = AbsMagnitude(Significand);
    }

    while (Absolute < static_cast<uint64>(MinNormalizedAbs) &&
           Exponent10 > MIN_int32)
    {
        if (Absolute > static_cast<uint64>(MaxSignificand / 10))
        {
            break;
        }

        Significand *= 10;
        --Exponent10;
        Absolute = AbsMagnitude(Significand);
    }
}

int32 FOGLargeNumber::Compare(const FOGLargeNumber& A, const FOGLargeNumber& B)
{
    const int32 SignA = A.GetSign();
    const int32 SignB = B.GetSign();

    if (SignA != SignB)
    {
        return SignA < SignB ? -1 : 1;
    }

    if (SignA == 0)
    {
        return 0;
    }

    FOGLargeNumber Left = A;
    FOGLargeNumber Right = B;
    Left.Normalize();
    Right.Normalize();

    if (Left.Exponent10 != Right.Exponent10)
    {
        const bool bLeftGreaterMagnitude = Left.Exponent10 > Right.Exponent10;
        if (SignA > 0)
        {
            return bLeftGreaterMagnitude ? 1 : -1;
        }
        return bLeftGreaterMagnitude ? -1 : 1;
    }

    if (Left.Significand == Right.Significand)
    {
        return 0;
    }

    return Left.Significand < Right.Significand ? -1 : 1;
}

FOGLargeNumber FOGLargeNumber::Add(
    const FOGLargeNumber& A,
    const FOGLargeNumber& B)
{
    if (A.IsZero())
    {
        return B;
    }
    if (B.IsZero())
    {
        return A;
    }

    FOGLargeNumber Left = A;
    FOGLargeNumber Right = B;
    Left.Normalize();
    Right.Normalize();

    if (Left.Exponent10 < Right.Exponent10)
    {
        Swap(Left, Right);
    }

    const int32 Delta = Left.Exponent10 - Right.Exponent10;
    if (Delta > 9)
    {
        return Left;
    }

    const int64 Scale = Pow10Int(Delta);
    const int64 AdjustedRight = DivideRounded(Right.Significand, Scale);

    return FOGLargeNumber(
        Left.Significand + AdjustedRight,
        Left.Exponent10);
}

FOGLargeNumber FOGLargeNumber::Multiply(
    const FOGLargeNumber& A,
    const FOGLargeNumber& B)
{
    if (A.IsZero() || B.IsZero())
    {
        return FOGLargeNumber();
    }

    FOGLargeNumber Left = A;
    FOGLargeNumber Right = B;
    Left.Normalize();
    Right.Normalize();

    // Each normalized significand is <= 9 digits, so the product fits int64.
    const int64 Product = Left.Significand * Right.Significand;

    return FOGLargeNumber(
        Product,
        Left.Exponent10 + Right.Exponent10);
}

FString FOGLargeNumber::ToDebugString() const
{
    return FString::Printf(
        TEXT("%llde%d"),
        Significand,
        Exponent10);
}
