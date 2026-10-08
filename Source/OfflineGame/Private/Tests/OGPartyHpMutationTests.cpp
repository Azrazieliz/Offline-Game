#include "Combat/OGWorldActionRuntime.h"
#include "Combat/OGDiagnosticEncounter.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

// Regression for the installed resolved-HP party mutation API: preserve live
// party state and use the existing defeated-member fallback.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGPartyResolvedHpPreservesStateTest,
    "OfflineGame.Combat.Action.ResolvedHpPreservesPartyAndFallback",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGPartyResolvedHpPreservesStateTest::RunTest(const FString& Parameters)
{
    UOGWorldPartyRuntimeComponent* Party = NewObject<UOGWorldPartyRuntimeComponent>();
    TArray<FOGCombatUnitState> Units = {
        OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 0),
        OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 1),
        OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 2)};
    Units[1].Presence = Units[2].Presence = EOGCombatPresence::Reserve;
    FString Error;
    if (!TestTrue(TEXT("Configure once"), Party->ConfigureParty(Units, Error))) return false;
    Party->SetQteReady(1, true);
    const int32 ControlledBefore = Party->GetControlledSlot();
    TestTrue(TEXT("Nonlethal resolved HP updates expected entity"),
        Party->ApplyResolvedHp(0, Units[0].UnitEntityId, FOGLargeNumber::FromInt64(70), Error));
    TestEqual(TEXT("Nonlethal damage preserves control"), Party->GetControlledSlot(), ControlledBefore);
    TestTrue(TEXT("Nonlethal damage preserves other QTE state"), Party->GetSlots()[1].bQteReady);
    for (int32 Index = 0; Index < Units.Num(); ++Index)
    {
        TestTrue(TEXT("Slot entity unchanged"), Party->GetSlots()[Index].Unit.UnitEntityId == Units[Index].UnitEntityId);
        TestTrue(TEXT("Identity unchanged"), Party->GetSlots()[Index].Unit.IdentityId == Units[Index].IdentityId);
        TestTrue(TEXT("Availability unchanged"), Party->GetSlots()[Index].bAvailable);
        TestTrue(TEXT("Resolved skill kit unchanged"), Party->GetSlots()[Index].Unit.SkillSet.UltimateSkill == Units[Index].SkillSet.UltimateSkill);
    }
    TestFalse(TEXT("Stale scheduled hit cannot damage a replacement unit"),
        Party->ApplyResolvedHp(0, FOGEntityId::NewId(), FOGLargeNumber(), Error));
    TestTrue(TEXT("Stale hit leaves HP untouched"), Party->GetSlots()[0].Unit.CurrentHp == FOGLargeNumber::FromInt64(70));
    TestTrue(TEXT("Lethal result invokes existing fallback"),
        Party->ApplyResolvedHp(0, Units[0].UnitEntityId, FOGLargeNumber(), Error));
    TestTrue(TEXT("Dead slot marked defeated"), Party->GetSlots()[0].bDefeated);
    TestTrue(TEXT("Control falls back to a living companion"), Party->GetControlledSlot() != 0 && Party->GetControlledSlot() != INDEX_NONE);
    TestFalse(TEXT("Dead slot cannot switch back in"), Party->TrySwitchTo(0, true));
    TestFalse(TEXT("Generic HP mutation cannot implicitly revive"),
        Party->ApplyResolvedHp(0, Units[0].UnitEntityId, FOGLargeNumber::FromInt64(10), Error));
    return true;
}
#endif