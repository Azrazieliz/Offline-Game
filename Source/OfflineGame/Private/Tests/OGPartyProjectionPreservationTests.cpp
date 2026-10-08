#include "Combat/OGWorldActionRuntime.h"
#include "Combat/OGDiagnosticEncounter.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGPartyProjectionPreservationTest,
    "OfflineGame.Foundation.Party.CanonicalProjectionPreservesRuntimeState",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FOGPartyProjectionPreservationTest::RunTest(const FString& Parameters)
{
    auto* Party = NewObject<UOGWorldPartyRuntimeComponent>();
    TArray<FOGCombatUnitState> Units;
    for (int32 Index = 0; Index < 3; ++Index)
        Units.Add(OGDiagnosticContent::MakeUnit(FOGEntityId(FGuid(1, 2, 3, Index + 1)), Index));
    FString Error;
    if (!TestTrue(TEXT("Configure canonical party"), Party->ConfigureParty(Units, Error))) return false;
    Party->SetQteReady(1, true);
    TestTrue(TEXT("Damage changes authoritative slot HP"), Party->ApplyResolvedHp(0, Units[0].UnitEntityId, FOGLargeNumber::FromInt64(25), Error));
    const auto Original = Party->GetSlots()[0].Unit;
    auto Projection = Original;
    Projection.Stats.Attack = FOGLargeNumber::FromInt64(777);
    Projection.CurrentHp = FOGLargeNumber::FromInt64(999);
    Projection.Presence = EOGCombatPresence::Defeated;
    Projection.NextActionValue = 12345;
    TestTrue(TEXT("Apply derived character mechanics"), Party->ApplyResolvedCharacterProjection(0, Projection, Error));
    const auto& Updated = Party->GetSlots()[0].Unit;
    TestTrue(TEXT("Current HP is preserved"), Updated.CurrentHp == Original.CurrentHp);
    TestTrue(TEXT("Presence is preserved"), Updated.Presence == Original.Presence);
    TestEqual(TEXT("Timeline is preserved"), Updated.NextActionValue, Original.NextActionValue);
    TestTrue(TEXT("Derived attack updates"), Updated.Stats.Attack == Projection.Stats.Attack);
    TestTrue(TEXT("Authored companion readiness is preserved"), Party->GetSlots()[1].bQteReady);
    const int32 Control = Party->GetControlledSlot();
    Projection.UnitEntityId = FOGEntityId(FGuid(9, 8, 7, 6));
    TestFalse(TEXT("Reject mismatched canonical identity"), Party->ApplyResolvedCharacterProjection(0, Projection, Error));
    TestEqual(TEXT("Control does not change during projection"), Party->GetControlledSlot(), Control);
    Party->MarkDefeatedAndResolveFallback(0);
    Projection = Party->GetSlots()[0].Unit;
    Projection.Stats.Attack = FOGLargeNumber::FromInt64(888);
    Projection.CurrentHp = FOGLargeNumber::FromInt64(120);
    TestTrue(TEXT("Defeated body can receive derived mechanics"), Party->ApplyResolvedCharacterProjection(0, Projection, Error));
    TestTrue(TEXT("Projection cannot revive a defeated slot"), Party->GetSlots()[0].bDefeated && Party->GetSlots()[0].Unit.CurrentHp.IsZero());
    return true;
}
#endif
