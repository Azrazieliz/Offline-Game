#include "Misc/AutomationTest.h"
#include "Runtime/OGCanonicalClockState.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGCanonicalClockPartitionTest,
    "OfflineGame.Core.Clock.PartitionPauseAndOverflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOGCanonicalClockPartitionTest::RunTest(const FString& Parameters)
{
    // Unitless arithmetic inputs; these do not author a production calendar/rate.
    FOGCanonicalClockState Initial;
    Initial.CurrentWorldTick = 100;
    Initial.RateDenominator = 7;
    Initial.FractionalNumerator = 2;
    FString Error;
    FOGCanonicalClockState Combined;
    if (!TestTrue(TEXT("Combined elapsed transition"),
        FOGCanonicalClockAccumulator::Advance(Initial, 11, 3, Combined, Error))) return false;
    FOGCanonicalClockState Split = Initial;
    for (int32 Index = 0; Index < 11; ++Index)
    {
        FOGCanonicalClockState Candidate;
        if (!TestTrue(TEXT("Partition transition"),
            FOGCanonicalClockAccumulator::Advance(Split, 1, 3, Candidate, Error))) return false;
        Split = Candidate;
    }
    TestEqual(TEXT("Partition-independent tick"), Split.CurrentWorldTick, Combined.CurrentWorldTick);
    TestEqual(TEXT("Partition-independent residue"), Split.FractionalNumerator, Combined.FractionalNumerator);

    FOGCanonicalClockState Paused;
    TestTrue(TEXT("Explicit core pause"), FOGCanonicalClockAccumulator::Advance(Split, 900, 0, Paused, Error));
    TestEqual(TEXT("Pause retains tick"), Paused.CurrentWorldTick, Split.CurrentWorldTick);
    TestEqual(TEXT("Pause retains residue"), Paused.FractionalNumerator, Split.FractionalNumerator);
    FOGCanonicalClockState Rejected;
    TestFalse(TEXT("Negative elapsed rejected"), FOGCanonicalClockAccumulator::Advance(Split, -1, 3, Rejected, Error));
    TestEqual(TEXT("Rejection retains tick"), Rejected.CurrentWorldTick, Split.CurrentWorldTick);

    FOGCanonicalClockState Limit;
    Limit.CurrentWorldTick = MAX_int64;
    Limit.RateDenominator = 1;
    TestFalse(TEXT("Tick overflow rejected"), FOGCanonicalClockAccumulator::Advance(Limit, 1, 1, Rejected, Error));
    TestEqual(TEXT("Overflow retains committed tick"), Rejected.CurrentWorldTick, MAX_int64);
    Limit.CurrentWorldTick = 0;
    TestFalse(TEXT("Elapsed product overflow rejected"),
        FOGCanonicalClockAccumulator::Advance(Limit, MAX_int64, 2, Rejected, Error));

    FOGCanonicalClockState LargeResidue;
    LargeResidue.RateDenominator = MAX_int64;
    LargeResidue.FractionalNumerator = MAX_int64 - 1;
    TestTrue(TEXT("Carry does not overflow residue sum"),
        FOGCanonicalClockAccumulator::Advance(LargeResidue, 1, 2, Rejected, Error));
    TestEqual(TEXT("Carry adds one tick"), Rejected.CurrentWorldTick, int64(1));
    TestEqual(TEXT("Carry retains remainder"), Rejected.FractionalNumerator, int64(1));

    FOGCanonicalClockState LongHistory;
    LongHistory.RateDenominator = MAX_int64;
    TestTrue(TEXT("Representable quotient survives product overflow"),
        FOGCanonicalClockAccumulator::Advance(LongHistory, MAX_int64, 2, Rejected, Error));
    TestEqual(TEXT("Large elapsed exact quotient"), Rejected.CurrentWorldTick, int64(2));
    TestEqual(TEXT("Large elapsed exact residue"), Rejected.FractionalNumerator, int64(0));
    return true;
}
#endif
