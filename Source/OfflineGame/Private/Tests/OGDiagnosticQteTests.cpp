#include "Combat/OGDiagnosticQteConditions.h"
#include "Combat/OGDiagnosticEncounter.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOGDiagnosticAuthoredQteTest,
    "OfflineGame.Diagnostic.QteUsesCanonicalTriggerConditions",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FOGDiagnosticAuthoredQteTest::RunTest(const FString& Parameters)
{
    UOGWorldPartyRuntimeComponent* Party = NewObject<UOGWorldPartyRuntimeComponent>();
    TArray<FOGCombatUnitState> Units = {
        OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 0),
        OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 1),
        OGDiagnosticContent::MakeUnit(FOGEntityId::NewId(), 2)};
    Units[1].Presence = Units[2].Presence = EOGCombatPresence::Reserve;
    FString Error;
    if (!TestTrue(TEXT("Configure party"), Party->ConfigureParty(Units, Error))) return false;
    FOGDiagnosticQteConditions Rules;
    if (!TestTrue(TEXT("Configure content conditions"), Rules.Configure(*Party, Error))) return false;
    const FOGEntityId Target = FOGEntityId::NewId();
    Rules.ConfirmSkillHit(Units[0].UnitEntityId, Target, Units[0].SkillSet.ActiveSkills[0], true, 1.0);
    TestFalse(TEXT("Wrong skill does not ready companion"), Party->GetSlots()[1].bQteReady);
    Rules.ConfirmSkillHit(Units[0].UnitEntityId, Target, Units[0].SkillSet.ActiveSkills[1], false, 1.0);
    TestFalse(TEXT("Dead target does not open combo"), Party->GetSlots()[1].bQteReady);
    Rules.ConfirmSkillHit(Units[0].UnitEntityId, Target, Units[0].SkillSet.ActiveSkills[1], true, 1.0);
    TestTrue(TEXT("Authored live burst hit queues and resolves readiness"), Party->GetSlots()[1].bQteReady);
    TestFalse(TEXT("Unrelated companion stays unready"), Party->GetSlots()[2].bQteReady);
    Rules.Refresh(4.9, true);
    TestTrue(TEXT("Combo window remains before expiry"), Party->GetSlots()[1].bQteReady);
    Rules.Refresh(5.0, true);
    TestFalse(TEXT("Combo window expires at boundary"), Party->GetSlots()[1].bQteReady);
    Rules.ConfirmPerfectDodge(FOGEntityId::NewId(), 6.0);
    TestFalse(TEXT("Foreign dodge does not ready assist"), Party->GetSlots()[2].bQteReady);
    Rules.ConfirmPerfectDodge(Units[0].UnitEntityId, 6.0);
    TestTrue(TEXT("Confirmed diagnostic perfect dodge opens authored second assist"), Party->GetSlots()[2].bQteReady);
    TestTrue(TEXT("Ordinary switch needs no QTE"), Party->TrySwitchTo(1, true));
    Rules.Reset();
    TestFalse(TEXT("Content cleanup clears owned readiness"), Party->GetSlots()[2].bQteReady);
    return true;
}
#endif
